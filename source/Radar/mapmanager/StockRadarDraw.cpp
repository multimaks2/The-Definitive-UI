/*****************************************************************************
 *
 *  PROJECT:     The-Definitive-UI
 *  FILE:        source/Radar/mapmanager/StockRadarDraw.cpp
 *  PURPOSE:     Project stock CRadar blips/gang overlay onto a custom plane
 *
 *****************************************************************************/

#include "StockRadarDraw.h"
#include "BlipManager.h"
#include "RadarOverlayCompat.h"
#include "RadarGeometry.h"
#include "MathUtils.h"
#include "Utils.h"
#include "GameState.h"
#include "plugin.h"
#include "RenderWare.h"
#include "common.h"
#include "CRadar.h"
#include "CWorld.h"
#include "CTimer.h"
#include "CMenuManager.h"
#include "CVehicle.h"
#include "CPlayerPed.h"
#include "CPlayerInfo.h"
#include "CTheScripts.h"
#include "CPools.h"
#include "CObject.h"
#include "CEntryExit.h"
#include "eVehicleType.h"
#include "eModelID.h"
#include "RenderWare.h"
#include <d3d9.h>
#include <cstring>
#include <cmath>

namespace
{
    constexpr float kSaScreenW   = 640.0f;
    constexpr float kSaScreenH   = 448.0f;
    constexpr float kWorldBound  = 3000.0f;
    constexpr float kRadarMinRange = 180.0f;
    constexpr float kRadarMaxRange = 350.0f;
    constexpr float kRadarMinSpeed = 0.3f;
    constexpr float kRadarMaxSpeed = 0.9f;
    constexpr uintptr_t kAddrTransform   = 0x583480;
    constexpr uintptr_t kAddrLimitPoint  = 0x5832F0;
    constexpr uintptr_t kAddrLimitToMap  = 0x583350;
    constexpr uintptr_t kAddrYouAreHere  = 0x584960;
    constexpr uintptr_t kAddrDrawSprite  = 0x585FF0;
    constexpr uintptr_t kAddrAirstrip    = 0x587D20;
    constexpr size_t kJmpSize = 5;

    bool  s_active = false;
    bool  s_hooksInstalled = false;
    bool  s_radarStateSaved = false;
    StockRadarPlane s_plane{};

    struct RadarGlobals
    {
        CVector2D origin{};
        float     range = 0.0f;
        float     sin   = 0.0f;
        float     cos   = 0.0f;
        float     orient = 0.0f;
        bool      drawingMap = false;
    };
    RadarGlobals s_savedRadar{};

    using TransformFn    = void(__cdecl*)(CVector2D&, CVector2D const&);
    using LimitToMapFn   = void(__cdecl*)(float*, float*);
    using YouAreHereFn   = void(__cdecl*)(float, float);
    using DrawSpriteFn   = void(__cdecl*)(unsigned short, float, float, unsigned char);

    TransformFn  s_origTransform = nullptr;
    LimitToMapFn s_origLimitToMap = nullptr;
    YouAreHereFn s_origYouAreHere = nullptr;
    DrawSpriteFn s_origDrawSprite = nullptr;
    unsigned char s_savedTransform[kJmpSize]{};
    unsigned char s_savedLimitToMap[kJmpSize]{};
    unsigned char s_savedYouAreHere[kJmpSize]{};
    unsigned char s_savedDrawSprite[kJmpSize]{};

    void* CreateGateway(uintptr_t src, size_t stealLen)
    {
        auto* gate = static_cast<unsigned char*>(
            VirtualAlloc(nullptr, stealLen + kJmpSize, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE));
        if (!gate)
            return nullptr;
        std::memcpy(gate, reinterpret_cast<void*>(src), stealLen);
        gate[stealLen] = 0xE9;
        const auto rel = static_cast<int32_t>(
            (src + stealLen) - (reinterpret_cast<uintptr_t>(gate) + stealLen + kJmpSize));
        std::memcpy(gate + stealLen + 1, &rel, sizeof(rel));
        return gate;
    }

    bool PlaceOnOrbitFromRadarPos(const D3DXVECTOR3& radarPos, float& px, float& py)
    {
        const D3DXVECTOR3 playerPos(s_plane.playerRadarX, s_plane.playerRadarY, 0.0f);
        float angle = 0.0f;
        if (!MathUtils::DirectionToOrbitAngle(playerPos, radarPos, s_plane.yaw, angle))
            return false;
        RadarGeometry::PointOnOrbitEdge(s_plane.cx, s_plane.cy, s_plane.halfX, s_plane.halfY,
                                        cosf(angle), sinf(angle), !s_plane.shapeCircle, px, py);
        return true;
    }

    enum class HudProject
    {
        Miss,
        Inside,
        Orbit
    };

    HudProject ProjectWorldOntoHud(float worldX, float worldY, float& px, float& py, bool allowOrbit, float radarZ = -1.0f)
    {
        if (!s_plane.use3D || s_plane.rtWidth < 1.0f || s_plane.rtHeight < 1.0f)
            return HudProject::Miss;

        D3DXVECTOR3 radarPos;
        if (radarZ >= 0.0f)
        {
            // GPS route height (same as built-in GpsRender / GPS Redux on 3D).
            radarPos.x = worldX + RadarGeometry::RADAR_OFFSET_X;
            radarPos.y = worldY + RadarGeometry::RADAR_OFFSET_Y;
            radarPos.z = radarZ;
        }
        else
        {
            RadarGeometry::WorldToRadarPos(worldX, worldY, radarPos);
        }

        const bool useSquare = !s_plane.shapeCircle;
        float circleX = 0.0f;
        float circleY = 0.0f;
        if (RadarGeometry::WorldToCircleScreen(
                radarPos, s_plane.cameraPos, s_plane.cameraRot,
                s_plane.fov, s_plane.nearPlane, s_plane.farPlane,
                s_plane.rtWidth, s_plane.rtHeight, s_plane.sizeX, s_plane.sizeY,
                s_plane.cx, s_plane.cy, circleX, circleY, s_plane.projectionAspect))
        {
            if (RadarGeometry::IsInsideOrbit(circleX, circleY, s_plane.cx, s_plane.cy,
                                              s_plane.halfX, s_plane.halfY, useSquare))
            {
                px = circleX;
                py = circleY;
                return HudProject::Inside;
            }
            if (!allowOrbit)
                return HudProject::Miss;
            RadarGeometry::ClampToOrbit(circleX, circleY, s_plane.cx, s_plane.cy,
                                        s_plane.halfX, s_plane.halfY, px, py, useSquare);
            return HudProject::Orbit;
        }

        if (!allowOrbit || !PlaceOnOrbitFromRadarPos(radarPos, px, py))
            return HudProject::Miss;
        return HudProject::Orbit;
    }

    // Donor RenderBlips2D: POIs draw only if inside the 3D disc. Orbit clamp is a
    // second pass for waypoint / legends / CHAR-CAR-checkpoint indicators.
    bool ProjectActivePlane3D(CVector2D const& in, float& px, float& py)
    {
        CVector2D world{};
        CRadar::TransformRadarPointToRealWorldSpace(world, in);
        return ProjectWorldOntoHud(world.x, world.y, px, py, false) == HudProject::Inside;
    }

    bool GetTraceWorldPos(const tRadarTrace& trace, CVector& out)
    {
        if (trace.m_nBlipType == BLIP_CHAR && trace.m_nEntityHandle)
        {
            if (CPed* ped = CPools::GetPed(static_cast<int>(trace.m_nEntityHandle)))
            {
                out = ped->GetPosition();
                return true;
            }
        }
        if (trace.m_nBlipType == BLIP_CAR && trace.m_nEntityHandle)
        {
            if (CVehicle* veh = CPools::GetVehicle(static_cast<int>(trace.m_nEntityHandle)))
            {
                out = veh->GetPosition();
                return true;
            }
        }
        if (trace.m_nBlipType == BLIP_OBJECT && trace.m_nEntityHandle)
        {
            if (CObject* obj = CPools::GetObject(static_cast<int>(trace.m_nEntityHandle)))
            {
                out = obj->GetPosition();
                return true;
            }
        }

        out = trace.m_vecPos;
        if (trace.m_pEntryExit)
            trace.m_pEntryExit->GetPositionRelativeToOutsideWorld(out);
        return true;
    }

    unsigned char StockHeightType(float blipZ, float playerZ)
    {
        const float zDiff = blipZ - playerZ;
        if (zDiff > 2.0f)
            return RADAR_TRACE_LOW;
        if (zDiff >= -4.0f)
            return RADAR_TRACE_NORMAL;
        return RADAR_TRACE_HIGH;
    }

    bool TraceShowsOnRadar(const tRadarTrace& trace)
    {
        if (!trace.m_bInUse)
            return false;
        if (trace.m_nBlipDisplay != BLIP_DISPLAY_BOTH
            && trace.m_nBlipDisplay != BLIP_DISPLAY_BLIP_ONLY)
            return false;
        if (trace.m_nBlipType == BLIP_CONTACTPOINT && CTheScripts::IsPlayerOnAMission())
            return false;
        return true;
    }

    void DrawOrbitSprite(unsigned short spriteId, float x, float y)
    {
        if (s_origDrawSprite)
            s_origDrawSprite(spriteId, x, y, 255);
    }

    CVector PlayerCentreForMap();

    void DrawOrbitIndicator(const tRadarTrace& trace, float x, float y, float blipZ, float playerZ)
    {
        const CRGBA color = CRadar::GetRadarTraceColour(
            trace.m_nColour, trace.m_bBright ? 1 : 0, trace.m_bFriendly ? 1 : 0);
        CRadar::ShowRadarTraceWithHeight(
            x, y, trace.m_nBlipSize, color.r, color.g, color.b, color.a,
            StockHeightType(blipZ, playerZ));
    }

    void DrawPlayerWaypointOnOrbit()
    {
        const int handle = FrontEndMenuManager.m_nTargetBlipIndex;
        if (!handle || !CRadar::ms_RadarTrace)
            return;

        const int idx = CRadar::GetActualBlipArrayIndex(handle);
        if (idx < 0)
            return;

        const tRadarTrace& trace = CRadar::ms_RadarTrace[idx];
        if (!trace.m_bInUse)
            return;

        CVector world{};
        if (!GetTraceWorldPos(trace, world))
            return;

        float px = 0.0f;
        float py = 0.0f;
        if (ProjectWorldOntoHud(world.x, world.y, px, py, true) != HudProject::Orbit)
            return;
        DrawOrbitSprite(RADAR_SPRITE_WAYPOINT, px, py);
    }

    void DrawHudOrbitBlips()
    {
        if (!s_plane.use3D || !CRadar::ms_RadarTrace)
            return;

        DrawPlayerWaypointOnOrbit();

        bool hideLegendsFromOrbit = false;
        for (unsigned int i = 0; i < MAX_RADAR_TRACES; ++i)
        {
            const tRadarTrace& t = CRadar::ms_RadarTrace[i];
            if (!t.m_bInUse)
                continue;
            const bool missionCp = (t.m_nBlipType == BLIP_COORD || t.m_nBlipType == BLIP_CONTACTPOINT)
                && BlipManager::IsMissionCheckpointSprite(t.m_nRadarSprite);
            if (t.m_nBlipType == BLIP_CHAR || t.m_nBlipType == BLIP_CAR || missionCp)
            {
                hideLegendsFromOrbit = true;
                break;
            }
        }

        const float playerZ = PlayerCentreForMap().z;
        const int waypointIdx = FrontEndMenuManager.m_nTargetBlipIndex
            ? CRadar::GetActualBlipArrayIndex(FrontEndMenuManager.m_nTargetBlipIndex)
            : -1;

        for (unsigned int i = 0; i < MAX_RADAR_TRACES; ++i)
        {
            const tRadarTrace& trace = CRadar::ms_RadarTrace[i];
            if (!TraceShowsOnRadar(trace))
                continue;

            const unsigned char spriteId = trace.m_nRadarSprite;
            if (spriteId == RADAR_SPRITE_NORTH || spriteId == RADAR_SPRITE_CENTRE
                || spriteId == RADAR_SPRITE_LIGHT || spriteId == RADAR_SPRITE_RUNWAY)
                continue;
            if (static_cast<int>(i) == waypointIdx || spriteId == RADAR_SPRITE_WAYPOINT)
                continue;

            const bool legend = BlipManager::IsLegendSprite(spriteId);
            const bool missionCp = (trace.m_nBlipType == BLIP_COORD || trace.m_nBlipType == BLIP_CONTACTPOINT)
                && BlipManager::IsMissionCheckpointSprite(spriteId);
            const bool indicator = trace.m_nBlipType == BLIP_CHAR
                || trace.m_nBlipType == BLIP_CAR
                || trace.m_nBlipType == BLIP_SPOTLIGHT
                || missionCp;

            if (!legend && !indicator)
                continue;
            if (!CRadar::HasThisBlipBeenRevealed(static_cast<int>(i)))
                continue;

            CVector world{};
            if (!GetTraceWorldPos(trace, world))
                continue;

            float px = 0.0f;
            float py = 0.0f;
            if (ProjectWorldOntoHud(world.x, world.y, px, py, true) != HudProject::Orbit)
                continue;

            if (legend && !hideLegendsFromOrbit)
            {
                DrawOrbitSprite(spriteId, px, py);
                continue;
            }
            if (indicator)
                DrawOrbitIndicator(trace, px, py, world.z, playerZ);
        }
    }

    void ProjectFlatOntoHudPlane(CVector2D const& in, float& px, float& py)
    {
        // Stock HUD / GPS Redux / CopNThreat expect a flat radar rect mapping,
        // not the tilted 3D disc projection used for our own blips.
        const float halfX = (s_plane.sizeX > 0.0f) ? (s_plane.sizeX * 0.5f) : s_plane.half;
        const float halfY = (s_plane.sizeY > 0.0f) ? (s_plane.sizeY * 0.5f) : s_plane.half;
        px = s_plane.cx + halfX * in.x;
        py = s_plane.cy - halfY * in.y;
    }

    void ProjectActivePlanePoint(CVector2D const& in, float& px, float& py)
    {
        if (s_plane.use3D)
        {
            if (!ProjectActivePlane3D(in, px, py))
            {
                px = -10000.0f;
                py = -10000.0f;
            }
            return;
        }

        ProjectFlatOntoHudPlane(in, px, py);
    }

    bool UsesBase640Coords()
    {
        return FrontEndMenuManager.m_bDrawRadarOrMap;
    }

    void ToOutputCoordSpace(float px, float py, CVector2D& out)
    {
        if (UsesBase640Coords())
        {
            out.x = px * kSaScreenW / SCREEN_WIDTH;
            out.y = py * kSaScreenH / SCREEN_HEIGHT;
        }
        else
        {
            out.x = px;
            out.y = py;
        }
    }

    void __cdecl TransformRadarPointToScreenSpace_Hook(CVector2D& out, CVector2D const& in)
    {
        __asm { push edx }

        if (s_active)
        {
            float px = 0.0f;
            float py = 0.0f;

            // GPS Redux / CopNThreat: on the 3D radar project at RADAR_ROUTE_Z
            // (same height as built-in GPS). Flat mapping sits above the tilted
            // map. Scissor corners stay on the RT AABB.
            if (RadarOverlayCompat::IsInvokingHudOverlay())
            {
                if (std::fabs(std::fabs(in.x) - 1.0f) < 0.001f
                    && std::fabs(std::fabs(in.y) - 1.0f) < 0.001f)
                {
                    px = (in.x < 0.0f) ? s_plane.clipL : s_plane.clipR;
                    py = (in.y < 0.0f) ? s_plane.clipB : s_plane.clipT;
                }
                else if (s_plane.use3D)
                {
                    CVector2D world{};
                    CRadar::TransformRadarPointToRealWorldSpace(world, in);
                    if (ProjectWorldOntoHud(world.x, world.y, px, py, true, RadarGeometry::RADAR_ROUTE_Z)
                        == HudProject::Miss)
                        ProjectFlatOntoHudPlane(in, px, py);
                }
                else
                {
                    ProjectFlatOntoHudPlane(in, px, py);
                }
            }
            else
            {
                ProjectActivePlanePoint(in, px, py);
            }
            ToOutputCoordSpace(px, py, out);
        }
        else if (s_origTransform)
        {
            s_origTransform(out, in);
        }

        __asm { pop edx }
    }

    void __cdecl LimitToMap_Hook(float* pX, float* pY)
    {
        if (!pX || !pY)
            return;

        if (s_active)
        {
            // External overlays (notably GPS Redux) expect stock LimitToMap
            // clamping. Sending an offscreen route point to -10000 creates
            // giant triangle-strip polygons.
            if (RadarOverlayCompat::IsInvokingPauseMapOverlay()
                || RadarOverlayCompat::IsInvokingHudOverlay())
            {
                if (*pX < s_plane.clipL) *pX = s_plane.clipL;
                if (*pX > s_plane.clipR) *pX = s_plane.clipR;
                if (*pY < s_plane.clipT) *pY = s_plane.clipT;
                if (*pY > s_plane.clipB) *pY = s_plane.clipB;
                return;
            }

            constexpr float pad = 28.0f;
            if (*pX < s_plane.clipL - pad || *pX > s_plane.clipR + pad
                || *pY < s_plane.clipT - pad || *pY > s_plane.clipB + pad)
            {
                *pX = -10000.0f;
                *pY = -10000.0f;
            }
            return;
        }

        if (s_origLimitToMap)
            s_origLimitToMap(pX, pY);
    }

    float __cdecl LimitRadarPoint_Hook(CVector2D& point)
    {
        const float mag = sqrtf(point.x * point.x + point.y * point.y);

        // Our own 3D HUD blips handle orbit separately. Pause map must keep
        // unclamped points. GPS Redux overlay still needs stock clamp.
        // Do NOT enable clamp for HudBlips: InvokeHudBlips runs stock DrawBlips
        // under that flag, and clamped edge blips then duplicate with orbit.
        if (FrontEndMenuManager.m_bDrawRadarOrMap)
            return mag;
        if (s_active && s_plane.use3D && !RadarOverlayCompat::IsInvokingHudOverlay())
            return mag;

        if (mag > 1.0f)
        {
            point.x /= mag;
            point.y /= mag;
        }
        return mag;
    }

    CVector PlayerCentreForMap()
    {
        return ((CVector(__cdecl*)(int))0x56E400)(0);
    }

    void __cdecl DrawYouAreHereSprite_Hook(float x, float y)
    {
        if (!s_active)
        {
            if (s_origYouAreHere)
                s_origYouAreHere(x, y);
            return;
        }

        static unsigned int& mapYouAreHereTimer = *reinterpret_cast<unsigned int*>(0xBAA358);
        static bool& mapYouAreHereDisplay = *reinterpret_cast<bool*>(0x8D0930);

        if (CTimer::m_snTimeInMillisecondsPauseMode - mapYouAreHereTimer > 700u)
        {
            mapYouAreHereTimer = CTimer::m_snTimeInMillisecondsPauseMode;
            mapYouAreHereDisplay = !mapYouAreHereDisplay;
        }

        CVector2D radarPos{};
        const CVector centre = PlayerCentreForMap();
        CRadar::TransformRealWorldPointToRadarSpace(radarPos, CVector2D(centre.x, centre.y));
        CRadar::LimitRadarPoint(radarPos);

        float px = 0.0f;
        float py = 0.0f;
        ProjectActivePlanePoint(radarPos, px, py);

        float drawX = px;
        float drawY = py;
        if (UsesBase640Coords())
        {
            drawX = px * kSaScreenW / SCREEN_WIDTH;
            drawY = py * kSaScreenH / SCREEN_HEIGHT;
        }

        constexpr float pad = 28.0f;
        const bool onMap = px >= s_plane.clipL - pad && px <= s_plane.clipR + pad
            && py >= s_plane.clipT - pad && py <= s_plane.clipB + pad;

        if (onMap && mapYouAreHereDisplay && CRadar::RadarBlipSprites)
        {
            constexpr float kPi = 3.14159265f;
            const float angle = FindPlayerHeading(0) + kPi;

            const unsigned half = static_cast<unsigned int>(SCREEN_WIDTH / kSaScreenW * 8.0f);
            CRadar::DrawRotatingRadarSprite(
                &CRadar::RadarBlipSprites[RADAR_SPRITE_CENTRE],
                drawX,
                drawY,
                angle,
                half,
                half,
                CRGBA(255, 255, 255, 255));
        }

        CRadar::AddBlipToLegendList(0, RADAR_SPRITE_MAP_HERE);
    }

    void __cdecl DrawRadarSprite_Hook(unsigned short spriteId, float x, float y, unsigned char alpha)
    {
        if (s_active && !UsesBase640Coords()
            && (spriteId == RADAR_SPRITE_NORTH
                || spriteId == RADAR_SPRITE_LIGHT
                || spriteId == RADAR_SPRITE_RUNWAY))
            return;

        if (s_origDrawSprite)
            s_origDrawSprite(spriteId, x, y, alpha);
    }

    void SaveRadarGlobals()
    {
        s_savedRadar.origin     = CRadar::vec2DRadarOrigin;
        s_savedRadar.range      = CRadar::m_radarRange;
        s_savedRadar.sin        = CRadar::cachedSin;
        s_savedRadar.cos        = CRadar::cachedCos;
        s_savedRadar.orient     = CRadar::m_fRadarOrientation;
        s_savedRadar.drawingMap = FrontEndMenuManager.m_bDrawRadarOrMap;
        s_radarStateSaved       = true;
    }

    void RestoreRadarGlobals()
    {
        if (!s_radarStateSaved)
            return;
        CRadar::vec2DRadarOrigin                 = s_savedRadar.origin;
        CRadar::m_radarRange                     = s_savedRadar.range;
        CRadar::cachedSin                        = s_savedRadar.sin;
        CRadar::cachedCos                        = s_savedRadar.cos;
        CRadar::m_fRadarOrientation              = s_savedRadar.orient;
        FrontEndMenuManager.m_bDrawRadarOrMap    = s_savedRadar.drawingMap;
        s_radarStateSaved = false;
    }

    void __cdecl SetupAirstripBlips_Hook()
    {
        if (CRadar::airstrip_blip)
        {
            CRadar::ClearBlip(CRadar::airstrip_blip);
            CRadar::airstrip_blip = 0;
        }
    }

    void ApplyMapProjection()
    {
        CRadar::vec2DRadarOrigin    = CVector2D(0.0f, 0.0f);
        CRadar::m_radarRange        = kWorldBound - 10.0f;
        CRadar::cachedSin           = 0.0f;
        CRadar::cachedCos           = 1.0f;
        CRadar::m_fRadarOrientation = 0.0f;
    }

    // Stock CRadar::DrawMap range (0x586B00) — HUD never calls DrawMap, so ESC InitFrontEndMap
    // would otherwise leave range at ~2990 and every short-range POI would pass canBeDrawn.
    void UpdateHudRadarRange()
    {
        CPlayerPed* player = FindPlayerPed();
        if (!player)
        {
            CRadar::m_radarRange = kRadarMinRange;
            return;
        }

        CVehicle* vehicle = FindPlayerVehicle();
        CPlayerInfo* info = player->GetPlayerInfoForThisPlayerPed();
        const bool remote = info && (info->m_pRemoteVehicle || info->m_bAfterRemoteVehicleExplosion);

        if (!vehicle || remote)
        {
            if (CTheScripts::RadarZoomValue)
                CRadar::m_radarRange = kRadarMinRange - static_cast<float>(CTheScripts::RadarZoomValue);
            else
                CRadar::m_radarRange = kRadarMinRange;
            return;
        }

        if (vehicle->m_nVehicleSubClass == VEHICLE_PLANE && vehicle->m_nModelIndex != MODEL_VORTEX)
        {
            const float speedZ = vehicle->GetPosition().z / 200.0f;
            if (speedZ < kRadarMinSpeed)
                CRadar::m_radarRange = kRadarMaxRange - 10.0f;
            else if (speedZ < kRadarMaxSpeed)
                CRadar::m_radarRange = (speedZ - kRadarMinSpeed) * (1.0f / 60.0f) + (kRadarMaxRange - 10.0f);
            else
                CRadar::m_radarRange = kRadarMaxRange;
            return;
        }

        const float speed = FindPlayerSpeed().Magnitude();
        if (speed < kRadarMinSpeed)
            CRadar::m_radarRange = kRadarMinRange;
        else if (speed >= kRadarMaxSpeed)
            CRadar::m_radarRange = kRadarMaxRange;
        else
            CRadar::m_radarRange = (speed - kRadarMinSpeed) * (850.0f / (kRadarMinSpeed * 10.0f)) + kRadarMinRange;
    }
}

void StockRadarDraw::EnsureHooksInstalled()
{
    if (s_hooksInstalled)
        return;

    plugin::patch::GetRaw(kAddrTransform, s_savedTransform, kJmpSize);
    plugin::patch::GetRaw(kAddrLimitToMap, s_savedLimitToMap, kJmpSize);
    plugin::patch::GetRaw(kAddrYouAreHere, s_savedYouAreHere, kJmpSize);
    plugin::patch::GetRaw(kAddrDrawSprite, s_savedDrawSprite, kJmpSize);

    s_origTransform = reinterpret_cast<TransformFn>(CreateGateway(kAddrTransform, kJmpSize));
    s_origLimitToMap = reinterpret_cast<LimitToMapFn>(CreateGateway(kAddrLimitToMap, kJmpSize));
    s_origYouAreHere = reinterpret_cast<YouAreHereFn>(CreateGateway(kAddrYouAreHere, kJmpSize));
    s_origDrawSprite = reinterpret_cast<DrawSpriteFn>(CreateGateway(kAddrDrawSprite, kJmpSize));
    if (!s_origTransform || !s_origLimitToMap || !s_origYouAreHere || !s_origDrawSprite)
        return;

    plugin::patch::RedirectJump(kAddrTransform, TransformRadarPointToScreenSpace_Hook);
    plugin::patch::RedirectJump(kAddrLimitPoint, LimitRadarPoint_Hook);
    plugin::patch::RedirectJump(kAddrLimitToMap, LimitToMap_Hook);
    plugin::patch::RedirectJump(kAddrYouAreHere, DrawYouAreHereSprite_Hook);
    plugin::patch::RedirectJump(kAddrDrawSprite, DrawRadarSprite_Hook);
    plugin::patch::RedirectJump(kAddrAirstrip, SetupAirstripBlips_Hook);
    s_hooksInstalled = true;
}

void StockRadarDraw::SetPlane(const StockRadarPlane& plane)
{
    s_plane = plane;
}

void StockRadarDraw::SyncRwSpritePipeline()
{
    _rwD3D9SetPixelShader(nullptr);
    _rwD3D9SetVertexShader(nullptr);
    RwD3D9SetTexture(nullptr, 0);
    RwD3D9SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
    RwD3D9SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
    RwD3D9SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
    RwD3D9SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_MODULATE);
    RwD3D9SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
    RwD3D9SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);
    RwD3D9SetTextureStageState(1, D3DTSS_COLOROP, D3DTOP_DISABLE);
    RwD3D9SetTextureStageState(1, D3DTSS_ALPHAOP, D3DTOP_DISABLE);
}

void StockRadarDraw::SanitizeDrawState()
{
    // GPS Redux leaves D3DRS_SCISSORTESTENABLE on the radar RT rect; if that
    // leaks past EndRadarZone the whole HUD draws into a tiny top-left clip.
    if (auto* device = static_cast<IDirect3DDevice9*>(RwD3D9GetCurrentD3DDevice()))
    {
        device->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);
        device->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
        device->SetRenderState(D3DRS_STENCILENABLE, FALSE);
        device->SetRenderState(D3DRS_ZENABLE, FALSE);
        device->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
        device->SetRenderState(D3DRS_CLIPPING, TRUE);
        device->SetRenderState(D3DRS_COLORWRITEENABLE,
            D3DCOLORWRITEENABLE_RED | D3DCOLORWRITEENABLE_GREEN |
            D3DCOLORWRITEENABLE_BLUE | D3DCOLORWRITEENABLE_ALPHA);
        device->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
        device->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
        device->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
        device->SetRenderState(D3DRS_SEPARATEALPHABLENDENABLE, FALSE);
        device->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
        device->SetRenderState(D3DRS_LIGHTING, FALSE);
        device->SetRenderState(D3DRS_FOGENABLE, FALSE);
        device->SetVertexShader(nullptr);
        device->SetPixelShader(nullptr);
        device->SetVertexDeclaration(nullptr);
        device->SetTexture(0, nullptr);
        device->SetTexture(1, nullptr);
    }

    SyncRwSpritePipeline();
    InvalidateRwShaderCache();
}

void StockRadarDraw::InvalidateRwShaderCache()
{
    Ui::PoisonRwShaderCache();
}

void StockRadarDraw::RestoreHudPipeline()
{
    InvalidateRwShaderCache();
}

void StockRadarDraw::Begin()
{
    SaveRadarGlobals();
    SyncRwSpritePipeline();
    BlipManager::PushDeIcons();
    s_active = true;
}

void StockRadarDraw::End()
{
    s_active = false;
    s_plane.use3D = false;
    BlipManager::PopDeIcons();
    RestoreRadarGlobals();
}

bool StockRadarDraw::IsActive()
{
    return s_active;
}

void StockRadarDraw::Draw(bool gangOverlay, bool gangInMenu)
{
    if (s_plane.use3D)
        FrontEndMenuManager.m_bDrawRadarOrMap = false;

    if (FrontEndMenuManager.m_bDrawRadarOrMap)
    {
        ApplyMapProjection();
    }
    else
    {
        CRadar::CalculateCachedSinCos();
        const CVector centre = PlayerCentreForMap();
        CRadar::vec2DRadarOrigin = CVector2D(centre.x, centre.y);
        UpdateHudRadarRange();
    }

    if (s_plane.use3D)
    {
        // Outdoors: third-party overlays were already baked into the radar RT.
        // Indoors / no map RT: still dispatch them onto the screen disc.
        if (!GameState::ShouldDrawRadarMap())
            RadarOverlayCompat::InvokeHudOverlay();
    }
    else if (FrontEndMenuManager.m_bDrawRadarOrMap)
    {
        // Replace the direct pause-map gang call with its live plugin-sdk chain,
        // preserving the original once when the setting allows it.
        RadarOverlayCompat::InvokePauseMapOverlay(gangOverlay);
    }
    else
    {
        // Rare 2D HUD fallback (no RT): stock gang + third-party overlays.
        if (gangOverlay)
            CRadar::DrawRadarGangOverlay(gangInMenu);
        RadarOverlayCompat::InvokeHudOverlay();
    }

    // CopNThreat hooks drawBlipsEvent (0x58AA2D / 0x575B44), not overlay.
    // Call the live trampoline so stock DrawBlips + AFTER callbacks run.
    if (FrontEndMenuManager.m_bDrawRadarOrMap)
        RadarOverlayCompat::InvokePauseMapBlips();
    else
        RadarOverlayCompat::InvokeHudBlips();

    if (s_plane.use3D)
        DrawHudOrbitBlips();
}

void StockRadarDraw::DrawHudOverlaysOnly()
{
    if (!s_active)
        return;

    FrontEndMenuManager.m_bDrawRadarOrMap = false;
    CRadar::CalculateCachedSinCos();
    const CVector centre = PlayerCentreForMap();
    CRadar::vec2DRadarOrigin = CVector2D(centre.x, centre.y);
    UpdateHudRadarRange();
    SyncRwSpritePipeline();
    RadarOverlayCompat::InvokeHudOverlay();
    SanitizeDrawState();
}
