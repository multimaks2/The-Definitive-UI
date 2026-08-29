/*****************************************************************************
 *
 *  PROJECT:     The-Definitive-UI
 *  FILE:        source/Radar/mapmanager/BlipManager.cpp
 *  PURPOSE:     Radar blips - stock sprite textures and overlay drawing
 *
 *****************************************************************************/

#include "BlipManager.h"
#include "MapChunkManager.h"
#include "Radar.h"
#include "Config.h"
#include "CFileLoader.h"
#include "CRadar.h"
#include "CSprite2d.h"
#include "RenderWare.h"
#include "RadarGeometry.h"
#include "MathUtils.h"
#include "GameState.h"
#include "draw/DxDrawPrimitives.h"
#include "CPlayerPed.h"
#include "CTheScripts.h"
#include "CPools.h"
#include "CObject.h"
#include "CEntryExit.h"
#include "CVehicle.h"
#include "common.h"
#include "CMenuManager.h"
#include <d3dx9.h>
#include <cstring>
#include <cstdio>
#include <cmath>

namespace
{
    LPDIRECT3DTEXTURE9 RwTextureToD3D9(LPDIRECT3DDEVICE9 pDevice, RwTexture* rwTex)
    {
        if (!pDevice || !rwTex)
            return nullptr;

        RwRaster* raster = RwTextureGetRaster(rwTex);
        if (!raster)
            return nullptr;

        const int w = RwRasterGetWidth(raster);
        const int h = RwRasterGetHeight(raster);
        if (w <= 0 || h <= 0)
            return nullptr;

        RwImage* img = RwImageCreate(w, h, 32);
        if (!img)
            return nullptr;
        if (!RwImageAllocatePixels(img))
        {
            RwImageDestroy(img);
            return nullptr;
        }
        if (!RwImageSetFromRaster(img, raster))
        {
            RwImageFreePixels(img);
            RwImageDestroy(img);
            return nullptr;
        }

        LPDIRECT3DTEXTURE9 d3dTex = nullptr;
        if (FAILED(pDevice->CreateTexture((UINT)w, (UINT)h, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &d3dTex, nullptr))
            || !d3dTex)
        {
            RwImageFreePixels(img);
            RwImageDestroy(img);
            return nullptr;
        }

        D3DLOCKED_RECT locked{};
        if (SUCCEEDED(d3dTex->LockRect(0, &locked, nullptr, 0)))
        {
            RwUInt8* src = RwImageGetPixels(img);
            const int srcStride = RwImageGetStride(img);
            const int dstStride = locked.Pitch;
            for (int y = 0; y < h; ++y)
            {
                RwUInt8* rowSrc = src + y * srcStride;
                RwUInt8* rowDst = static_cast<RwUInt8*>(locked.pBits) + y * dstStride;
                for (int x = 0; x < w; ++x)
                {
                    const int off = x * 4;
                    rowDst[off + 0] = rowSrc[off + 2];
                    rowDst[off + 1] = rowSrc[off + 1];
                    rowDst[off + 2] = rowSrc[off + 0];
                    rowDst[off + 3] = rowSrc[off + 3];
                }
            }
            d3dTex->UnlockRect(0);
        }

        RwImageFreePixels(img);
        RwImageDestroy(img);
        return d3dTex;
    }

    static RwTexDictionary* s_deTxd = nullptr;
    static int              s_deSwapDepth = 0;
    static bool             s_deSwapped = false;
    static RwTexture*       s_savedSprites[BlipManager::MAX_BLIP_ID + 1]{};
    static bool             s_replacedSprites[BlipManager::MAX_BLIP_ID + 1]{};

    static RwTexDictionary* EnsureDeTxd()
    {
        if (!s_deTxd)
            s_deTxd = CFileLoader::LoadTexDictionary(PLUGIN_PATH(Radar::Path::BlipTxd));
        return s_deTxd;
    }
}

BlipManager::BlipManager(LPDIRECT3DDEVICE9 pDevice)
    : m_pDevice(pDevice)
    , m_pAuxTxd(nullptr)
    , m_bInitialized(false)
    , m_spriteTexDeIcons(false)
{
    ZeroMemory(m_stockTextures, sizeof(m_stockTextures));
    ZeroMemory(m_auxTextures, sizeof(m_auxTextures));
}

BlipManager::~BlipManager()
{
    Shutdown();
}

bool BlipManager::Initialize()
{
    if (m_bInitialized)
        return true;

    StockRadarDraw::EnsureHooksInstalled();

    if (!m_pAuxTxd)
        m_pAuxTxd = EnsureDeTxd();

    m_bInitialized = true;
    return true;
}

void BlipManager::Shutdown()
{
    for (int i = 0; i <= MAX_BLIP_ID; ++i)
    {
        if (m_stockTextures[i])
        {
            m_stockTextures[i]->Release();
            m_stockTextures[i] = nullptr;
        }
    }

    for (auto*& tex : m_auxTextures)
    {
        if (tex)
        {
            tex->Release();
            tex = nullptr;
        }
    }

    PopDeIcons();

    if (m_pAuxTxd)
    {
        if (s_deTxd == m_pAuxTxd)
            s_deTxd = nullptr;
        RwTexDictionaryDestroy(m_pAuxTxd);
        m_pAuxTxd = nullptr;
    }

    m_bInitialized = false;
}

void BlipManager::EnsureStockTexturesLoaded()
{
}

LPDIRECT3DTEXTURE9 BlipManager::ConvertRwTexture(RwTexture* rwTex)
{
    return RwTextureToD3D9(m_pDevice, rwTex);
}

LPDIRECT3DTEXTURE9 BlipManager::GetStockSpriteTexture(int spriteId)
{
    (void)spriteId;
    return nullptr;
}

LPDIRECT3DTEXTURE9 BlipManager::GetOwnedSpriteTexture(int spriteId)
{
    if (spriteId == RADAR_SPRITE_LIGHT || spriteId == RADAR_SPRITE_RUNWAY)
        return nullptr;
    if (spriteId < 0 || spriteId > MAX_BLIP_ID || !m_pDevice)
        return nullptr;

    // Match orbit path: use whatever CRadar sprites currently point at
    // (PushDeIcons swaps DE txd in; off = stock game textures).
    const bool deIcons = RadarConfig::GetDeIcons();
    if (m_spriteTexDeIcons != deIcons)
    {
        for (int i = 0; i <= MAX_BLIP_ID; ++i)
        {
            if (m_stockTextures[i])
            {
                m_stockTextures[i]->Release();
                m_stockTextures[i] = nullptr;
            }
        }
        m_spriteTexDeIcons = deIcons;
    }

    if (m_stockTextures[spriteId])
        return m_stockTextures[spriteId];

    RwTexture* rwTex = nullptr;
    if (CRadar::RadarBlipSprites)
        rwTex = CRadar::RadarBlipSprites[spriteId].m_pTexture;

    // Fallback: DE txd by numeric name if sprite slot empty.
    if (!rwTex && deIcons && m_pAuxTxd)
    {
        char texName[16];
        sprintf_s(texName, "%d", spriteId);
        rwTex = RwTexDictionaryFindNamedTexture(m_pAuxTxd, texName);
    }

    if (!rwTex)
        return nullptr;
    m_stockTextures[spriteId] = ConvertRwTexture(rwTex);
    return m_stockTextures[spriteId];
}

LPDIRECT3DTEXTURE9 BlipManager::LoadAuxTextureFromTxd(const char* texName)
{
    if (!m_pDevice || !m_pAuxTxd || !texName)
        return nullptr;

    RwTexture* rwTex = RwTexDictionaryFindNamedTexture(m_pAuxTxd, texName);
    if (!rwTex)
        return nullptr;

    if (strcmp(texName, "line") == 0 || strcmp(texName, "radarLine") == 0)
    {
        if (!m_auxTextures[0])
            m_auxTextures[0] = ConvertRwTexture(rwTex);
        return m_auxTextures[0];
    }

    if (strcmp(texName, "radarRingPlane") == 0 || strcmp(texName, "RingPlane") == 0)
    {
        if (!m_auxTextures[2])
            m_auxTextures[2] = ConvertRwTexture(rwTex);
        return m_auxTextures[2];
    }

    return ConvertRwTexture(rwTex);
}

void BlipManager::DrawStockOverlay(const StockRadarPlane& plane, bool gangZones)
{
    StockRadarDraw::SetPlane(plane);
    StockRadarDraw::Begin();
    StockRadarDraw::Draw(gangZones, false);
    StockRadarDraw::End();
}

namespace
{
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

    enum class RtProject
    {
        Miss,
        Inside,
        Orbit
    };

    // allowOrbitClamp: waypoint/legends/indicators may sit on orbit.
    // Orbit results must NOT be drawn into the circle-clipped Blip RT — HUD orbit pass draws them.
    RtProject ProjectWorldToBlipRt(float worldX, float worldY, const StockRadarPlane& plane,
                                   float& outX, float& outY, bool allowOrbitClamp)
    {
        if (!plane.use3D || plane.rtWidth < 1.0f || plane.rtHeight < 1.0f)
            return RtProject::Miss;

        const float cx = plane.rtWidth * 0.5f;
        const float cy = plane.rtHeight * 0.5f;
        const bool useSquare = !plane.shapeCircle;
        const float halfX = (plane.halfX > 1.0f) ? plane.halfX : cx;
        const float halfY = (plane.halfY > 1.0f) ? plane.halfY : cy;

        D3DXVECTOR3 radarPos;
        RadarGeometry::WorldToRadarPos(worldX, worldY, radarPos);

        float px = 0.0f;
        float py = 0.0f;
        if (RadarGeometry::WorldToCircleScreen(
                radarPos, plane.cameraPos, plane.cameraRot,
                plane.fov, plane.nearPlane, plane.farPlane,
                plane.rtWidth, plane.rtHeight, plane.rtWidth, plane.rtHeight,
                cx, cy, px, py, plane.projectionAspect))
        {
            if (RadarGeometry::IsInsideOrbit(px, py, cx, cy, halfX, halfY, useSquare))
            {
                outX = px;
                outY = py;
                return RtProject::Inside;
            }
            if (!allowOrbitClamp)
                return RtProject::Miss;
            RadarGeometry::ClampToOrbit(px, py, cx, cy, halfX, halfY, outX, outY, useSquare);
            return RtProject::Orbit;
        }

        if (!allowOrbitClamp)
            return RtProject::Miss;

        const D3DXVECTOR3 playerPos(plane.playerRadarX, plane.playerRadarY, 0.0f);
        float angle = 0.0f;
        if (!MathUtils::DirectionToOrbitAngle(playerPos, radarPos, plane.yaw, angle))
            return RtProject::Miss;
        RadarGeometry::PointOnOrbitEdge(cx, cy, halfX, halfY, cosf(angle), sinf(angle), useSquare, outX, outY);
        return RtProject::Orbit;
    }

    float BlipRtEdgeFade(float x, float y, const StockRadarPlane& plane)
    {
        if (!RadarConfig::GetBlipEdgeFade())
            return 1.0f;
        const float fadeW = MathUtils::ScaleRadarLength(18.0f) * (plane.rtWidth / (plane.sizeX > 1.0f ? plane.sizeX : plane.rtWidth));
        return RadarGeometry::ComputeOrbitEdgeFade(
            x, y, plane.rtWidth * 0.5f, plane.rtHeight * 0.5f,
            plane.halfX, plane.halfY, !plane.shapeCircle, fadeW);
    }
}

float BlipManager::GetIconCanvasScale()
{
    constexpr float kBase = 1.0f / 3.5f;
    return kBase * (static_cast<float>(RadarConfig::GetBlipIconScalePercent()) / 100.0f);
}

float BlipManager::GetStockSpriteDiameterPx(float rtScale)
{
    const float w = static_cast<float>(RsGlobal.maximumWidth);
    const float half = std::floor(w / 640.0f * 8.0f);
    return (std::max)(12.0f, half * 2.0f) * GetIconCanvasScale() * rtScale;
}

float BlipManager::GetTraceMarkerDiameterPx(unsigned char blipSize, float rtScale)
{
    const float w = static_cast<float>(RsGlobal.maximumWidth);
    const float stretch = w / 640.0f;
    return (std::max)(3.0f, static_cast<float>(blipSize) * 2.0f * stretch * rtScale * GetIconCanvasScale());
}

void BlipManager::DrawBlipsToRenderTarget(const StockRadarPlane& plane, DxDrawPrimitives* draw,
                                          float hudRadarSizeX, float /*defaultSpriteSize*/,
                                          bool previewPedOnly)
{
    if (!draw || !plane.use3D || plane.rtWidth < 1.0f || plane.rtHeight < 1.0f)
        return;

    const float rtW = plane.rtWidth;
    const float rtH = plane.rtHeight;
    const float rtScale = rtW / (hudRadarSizeX > 1.0f ? hudRadarSizeX : rtW);
    const float spriteSize = BlipManager::GetStockSpriteDiameterPx(rtScale);

    PushDeIcons();

    auto drawSprite = [&](int spriteId, float px, float py, float size, float angle, DWORD color) {
        if ((color >> 24) == 0)
            return;
        LPDIRECT3DTEXTURE9 tex = GetOwnedSpriteTexture(spriteId);
        if (!tex)
            return;
        const float half = size * 0.5f;
        draw->dxDrawImage2DRotatedSurface(px - half, py - half, size, size, tex, angle, color, rtW, rtH);
    };

    auto playerHeadingRad = [](CPed* ped) -> float {
        if (!ped)
            return 0.0f;
        if (!GameState::IsPedInVehicleTransition(ped))
        {
            __try { return FindPlayerHeading(0); }
            __except (EXCEPTION_EXECUTE_HANDLER) { return ped->GetHeading(); }
        }
        return ped->GetHeading();
    };

    // Negated heading + yaw give correct L/R; drop ±π (that was a 180° sprite flip).
    auto centreSpriteAngle = [&](CPed* ped, bool preview) -> float {
        if (preview)
            return -plane.yaw;
        return (-playerHeadingRad(ped)) - plane.yaw;
    };

    auto drawAtWorld = [&](float worldX, float worldY, bool allowOrbitClamp, auto&& fn) {
        float px = 0.0f;
        float py = 0.0f;
        // Orbit icons are drawn on HUD (no circle clip). RT only gets Inside.
        if (ProjectWorldToBlipRt(worldX, worldY, plane, px, py, allowOrbitClamp) != RtProject::Inside)
            return;
        const float fade = BlipRtEdgeFade(px, py, plane);
        if (fade <= 0.0f)
            return;
        fn(px, py, fade);
    };

    if (previewPedOnly)
    {
        // Settings preview focuses camera on MAP_CENTER — same world as map tiles.
        const float previewWorldX = MapChunkManager::MAP_CENTER_X - RadarGeometry::RADAR_OFFSET_X;
        const float previewWorldY = MapChunkManager::MAP_CENTER_Y - RadarGeometry::RADAR_OFFSET_Y;
        drawAtWorld(previewWorldX, previewWorldY, false, [&](float px, float py, float fade) {
            const DWORD color = D3DCOLOR_ARGB((BYTE)(255.0f * fade), 255, 255, 255);
            drawSprite(RADAR_SPRITE_CENTRE, px, py, spriteSize, centreSpriteAngle(nullptr, true), color);
        });
        PopDeIcons();
        return;
    }

    if (!CRadar::ms_RadarTrace)
    {
        PopDeIcons();
        return;
    }

    CPed* player = FindPlayerPed();
    const float playerZ = player ? player->GetPosition().z : 0.0f;

    if (player)
    {
        const CVector pos = player->GetPosition();
        const float angle = centreSpriteAngle(player, false);
        drawAtWorld(pos.x, pos.y, false, [&](float px, float py, float fade) {
            const DWORD color = D3DCOLOR_ARGB((BYTE)(255.0f * fade), 255, 255, 255);
            drawSprite(RADAR_SPRITE_CENTRE, px, py, spriteSize, angle, color);
        });
    }

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

        CVector world{};
        if (!GetTraceWorldPos(trace, world))
            continue;

        const bool legend = IsLegendSprite(spriteId);
        const bool missionCp = (trace.m_nBlipType == BLIP_COORD || trace.m_nBlipType == BLIP_CONTACTPOINT)
            && IsMissionCheckpointSprite(spriteId);
        const bool indicator = trace.m_nBlipType == BLIP_CHAR
            || trace.m_nBlipType == BLIP_CAR
            || trace.m_nBlipType == BLIP_SPOTLIGHT
            || missionCp;
        const bool waypoint = (static_cast<int>(i) == waypointIdx) || (spriteId == RADAR_SPRITE_WAYPOINT);
        // Legends + waypoint: always HUD above border (Inside and Orbit). Never Blip RT.
        if (legend || waypoint)
            continue;
        // Indicators: Inside → RT under border; Orbit → HUD (unchanged).
        const bool allowOrbit = indicator;

        drawAtWorld(world.x, world.y, allowOrbit, [&](float px, float py, float fade) {
            if (spriteId != RADAR_SPRITE_NONE)
            {
                const BYTE alpha = (BYTE)((255.0f * fade) + 0.5f);
                const DWORD color = D3DCOLOR_ARGB(alpha, 255, 255, 255);
                drawSprite(spriteId, px, py, spriteSize, 0.0f, color);
                return;
            }

            const DWORD baseColor = TraceColorToD3D(trace.m_nColour, trace.m_bBright != 0, trace.m_bFriendly != 0);
            const BYTE alpha = (BYTE)((float)((baseColor >> 24) & 0xFF) * fade);
            const DWORD color = (baseColor & 0x00FFFFFF) | (alpha << 24);

            const float traceSize = BlipManager::GetTraceMarkerDiameterPx(trace.m_nBlipSize, rtScale);

            const eHeightIndicatorType heightType = GetHeightIndicatorType(world.z, playerZ);
            draw->dxDrawGTAIndicatorBlipSurface(px, py, traceSize, color, heightType, rtW, rtH);
        });
    }

    PopDeIcons();
}

void BlipManager::PushDeIcons()
{
    if (++s_deSwapDepth != 1)
        return;
    s_deSwapped = false;
    if (!RadarConfig::GetDeIcons() || !CRadar::RadarBlipSprites)
        return;
    RwTexDictionary* txd = EnsureDeTxd();
    if (!txd)
        return;

    ZeroMemory(s_replacedSprites, sizeof(s_replacedSprites));
    for (int i = 0; i <= MAX_BLIP_ID; ++i)
    {
        if (i == RADAR_SPRITE_LIGHT || i == RADAR_SPRITE_RUNWAY)
            continue;
        char texName[16];
        sprintf_s(texName, "%d", i);
        RwTexture* deTex = RwTexDictionaryFindNamedTexture(txd, texName);
        if (!deTex)
            continue;
        s_savedSprites[i] = CRadar::RadarBlipSprites[i].m_pTexture;
        s_replacedSprites[i] = true;
        CRadar::RadarBlipSprites[i].m_pTexture = deTex;
        s_deSwapped = true;
    }
}

void BlipManager::PopDeIcons()
{
    if (s_deSwapDepth <= 0)
        return;
    if (--s_deSwapDepth != 0)
        return;
    if (!s_deSwapped || !CRadar::RadarBlipSprites)
    {
        s_deSwapped = false;
        return;
    }
    for (int i = 0; i <= MAX_BLIP_ID; ++i)
    {
        if (!s_replacedSprites[i])
            continue;
        CRadar::RadarBlipSprites[i].m_pTexture = s_savedSprites[i];
        s_replacedSprites[i] = false;
        s_savedSprites[i] = nullptr;
    }
    s_deSwapped = false;
}

bool BlipManager::IsLegendSprite(unsigned char spriteId)
{
    switch (spriteId)
    {
    case RADAR_SPRITE_BIGSMOKE:
    case RADAR_SPRITE_CATALINAPINK:
    case RADAR_SPRITE_CESARVIAPANDO:
    case RADAR_SPRITE_CJ:
    case RADAR_SPRITE_CRASH1:
    case RADAR_SPRITE_MCSTRAP:
    case RADAR_SPRITE_OGLOC:
    case RADAR_SPRITE_RYDER:
    case RADAR_SPRITE_SWEET:
    case RADAR_SPRITE_THETRUTH:
    case RADAR_SPRITE_TORENORANCH:
    case RADAR_SPRITE_WOOZIE:
    case RADAR_SPRITE_ZERO:
        return true;
    default:
        return false;
    }
}

bool BlipManager::IsMissionCheckpointSprite(unsigned char spriteId)
{
    switch (spriteId)
    {
    case RADAR_SPRITE_NONE:
    case RADAR_SPRITE_QMARK:
        return true;
    default:
        return false;
    }
}

eHeightIndicatorType BlipManager::GetHeightIndicatorType(float blipZ, float playerZ, float threshold)
{
    const float diff = blipZ - playerZ;
    if (diff > threshold)
        return HEIGHT_INDICATOR_ABOVE;
    if (diff < -threshold)
        return HEIGHT_INDICATOR_BELOW;
    return HEIGHT_INDICATOR_SAME;
}

DWORD BlipManager::TraceColorToD3D(unsigned int blipColour, bool bright, bool friendly)
{
    CRGBA color = CRadar::GetRadarTraceColour(blipColour, bright ? 1 : 0, friendly ? 1 : 0);
    return D3DCOLOR_ARGB(color.a, color.r, color.g, color.b);
}
