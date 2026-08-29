/*****************************************************************************
 *
 *  PROJECT:     The-Definitive-UI
 *  FILE:        source/Radar/render/GpsRender.cpp
 *  PURPOSE:     GPS route rendering on the radar plane
 *
 *****************************************************************************/

#include "GpsRender.h"
#include "RadarGeometry.h"
#include "DxDrawPrimitives.h"
#include "Draw.h"
#include "Config.h"
#include "ColorUtils.h"
#include "plugin.h"
#include "common.h"
#include "RenderWare.h"
#include "CPlayerPed.h"
#include "CVehicle.h"
#include "CMenuManager.h"
#include "CRadar.h"
#include "CPathFind.h"
#include "CNodeAddress.h"
#include "CPathNode.h"
#include <algorithm>
#include <array>

#define GPS_LINE_WIDTH       6.0f
#define GPS_LINE_COLOR_A     255
#define GPS_LINE_R           255
#define GPS_LINE_G           220
#define GPS_LINE_B           0
#define MAX_NODE_POINTS      5000

namespace
{
    std::array<char, 1024> g_pathNodesToStream{};
    std::array<int, 50000> g_pathNodes{};
    constexpr std::array<uintptr_t, 8> kPathPatchAddresses = {
        0x44DE3C, 0x450D03, 0x451782, 0x451904,
        0x451AC3, 0x451B33, 0x4518F8, 0x4519B0
    };
    std::array<unsigned int, kPathPatchAddresses.size()> g_stockPathPatchValues{};
    bool g_stockPathPatchValuesSaved = false;
    bool g_pathfindingPatched = false;

    bool BuildWorldRoute(std::vector<CVector>& route)
    {
        route.clear();
        if (!RadarConfig::GetGps())
            return false;

        CPed* player = FindPlayerPed(0);
        if (!player || !player->m_pVehicle || !player->bInVehicle)
            return false;

        const int vehicleClass = player->m_pVehicle->m_nVehicleSubClass;
        if (vehicleClass == VEHICLE_PLANE || vehicleClass == VEHICLE_HELI || vehicleClass == VEHICLE_BMX)
            return false;
        if (!FrontEndMenuManager.m_nTargetBlipIndex)
            return false;

        const int blipArrId = LOWORD(FrontEndMenuManager.m_nTargetBlipIndex);
        const int blipCounter = HIWORD(FrontEndMenuManager.m_nTargetBlipIndex);
        if (blipArrId < 0 || blipArrId >= static_cast<int>(MAX_RADAR_TRACES))
            return false;
        if (CRadar::ms_RadarTrace[blipArrId].m_nCounter != static_cast<unsigned short>(blipCounter)
            || !CRadar::ms_RadarTrace[blipArrId].m_nBlipDisplay)
            return false;

        const CVector playerPos = FindPlayerCoors(0);
        CVector destPos = CRadar::ms_RadarTrace[blipArrId].m_vecPos;
        destPos.z = playerPos.z;

        short nodesCount = 0;
        float gpsDistance = 0.0f;
        static std::array<CNodeAddress, MAX_NODE_POINTS> resultNodes{};
        ThePaths.DoPathSearch(0, playerPos, CNodeAddress(), destPos,
            resultNodes.data(), &nodesCount, MAX_NODE_POINTS, &gpsDistance,
            999999.0f, nullptr, 999999.0f, false, CNodeAddress(), false,
            vehicleClass == VEHICLE_BOAT);

        if (nodesCount <= 0)
            return false;

        int startSegment = 0;
        D3DXVECTOR3 adjustedStart(playerPos.x, playerPos.y, playerPos.z);
        const D3DXVECTOR3 playerPoint(playerPos.x, playerPos.y, playerPos.z);

        for (short i = 0; i < nodesCount - 1; ++i)
        {
            const CVector p0 = ThePaths.GetPathNode(resultNodes[i])->GetNodeCoors();
            const CVector p1 = ThePaths.GetPathNode(resultNodes[i + 1])->GetNodeCoors();
            D3DXVECTOR3 segStart(p0.x, p0.y, p0.z);
            D3DXVECTOR3 segDir(p1.x - p0.x, p1.y - p0.y, p1.z - p0.z);
            const float segLength = D3DXVec3Length(&segDir);
            if (segLength < 0.01f)
                continue;
            D3DXVec3Normalize(&segDir, &segDir);

            const D3DXVECTOR3 toPlayer = playerPoint - segStart;
            const float projection = D3DXVec3Dot(&toPlayer, &segDir);
            if (projection >= segLength)
            {
                startSegment = i + 1;
                continue;
            }
            if (projection > 0.0f)
            {
                adjustedStart = segStart + segDir * projection;
                startSegment = i;
                break;
            }
            const D3DXVECTOR3 toSegment = segStart - playerPoint;
            if (D3DXVec3Length(&toSegment) < 50.0f)
            {
                adjustedStart = playerPoint;
                startSegment = i;
                break;
            }
        }

        route.reserve(static_cast<size_t>(nodesCount) + 2);
        route.emplace_back(adjustedStart.x, adjustedStart.y, adjustedStart.z);
        for (short i = static_cast<short>(startSegment + 1); i < nodesCount; ++i)
            route.push_back(ThePaths.GetPathNode(resultNodes[i])->GetNodeCoors());
        route.push_back(destPos);
        return route.size() >= 2;
    }
}

GpsRenderer::GpsRenderer(LPDIRECT3DDEVICE9 pDevice)
    : m_pDevice(pDevice)
    , m_bInitialized(false)
{
}

GpsRenderer::~GpsRenderer()
{
    Shutdown();
}

void GpsRenderer::SetPathfindingPatchesEnabled(bool enabled)
{
    if (!g_stockPathPatchValuesSaved)
    {
        for (size_t i = 0; i < kPathPatchAddresses.size(); ++i)
            g_stockPathPatchValues[i] = plugin::patch::GetUInt(kPathPatchAddresses[i]);
        g_stockPathPatchValuesSaved = true;
    }

    if (enabled)
    {
        if (g_pathfindingPatched)
            return;

        std::fill(g_pathNodesToStream.begin(), g_pathNodesToStream.end(), 1);
        std::fill(g_pathNodes.begin(), g_pathNodes.end(), -1);

        plugin::patch::SetPointer(0x44DE3C, g_pathNodesToStream.data());
        plugin::patch::SetPointer(0x450D03, g_pathNodesToStream.data());
        plugin::patch::SetPointer(0x451782, g_pathNodes.data());
        plugin::patch::SetPointer(0x451904, g_pathNodes.data());
        plugin::patch::SetPointer(0x451AC3, g_pathNodes.data());
        plugin::patch::SetPointer(0x451B33, g_pathNodes.data());
        plugin::patch::SetUInt(0x4518F8, 50000);
        plugin::patch::SetUInt(0x4519B0, 49950);
        g_pathfindingPatched = true;
        return;
    }

    if (!g_pathfindingPatched)
        return;
    for (size_t i = 0; i < kPathPatchAddresses.size(); ++i)
        plugin::patch::SetUInt(kPathPatchAddresses[i], g_stockPathPatchValues[i]);
    g_pathfindingPatched = false;
}

bool GpsRenderer::Initialize()
{
    if (!m_pDevice)
        return false;
    m_bInitialized = true;
    return true;
}

void GpsRenderer::Shutdown()
{
    m_bInitialized = false;
}

void GpsRenderer::RenderMap2D(Draw* pDraw,
    float mapL, float mapT, float mapSize,
    float clipL, float clipT, float clipR, float clipB,
    float screenWidth)
{
    if (!pDraw || mapSize <= 1.0f || clipR <= clipL || clipB <= clipT)
        return;

    std::vector<CVector> worldRoute;
    if (!BuildWorldRoute(worldRoute))
        return;

    LPDIRECT3DDEVICE9 device = pDraw->GetDevice();
    if (!device)
        return;

    DWORD oldScissorEnable = 0;
    RECT oldScissorRect{};
    device->GetRenderState(D3DRS_SCISSORTESTENABLE, &oldScissorEnable);
    device->GetScissorRect(&oldScissorRect);

    RECT clipRect{};
    clipRect.left = static_cast<LONG>(clipL);
    clipRect.top = static_cast<LONG>(clipT);
    clipRect.right = static_cast<LONG>(clipR);
    clipRect.bottom = static_cast<LONG>(clipB);
    device->SetRenderState(D3DRS_SCISSORTESTENABLE, TRUE);
    device->SetScissorRect(&clipRect);

    const float half = mapSize * 0.5f;
    const float mapCx = mapL + half;
    const float mapCy = mapT + half;
    float lineWidth = GPS_LINE_WIDTH * (screenWidth / 1920.0f);
    if (lineWidth < 2.0f)
        lineWidth = 2.0f;
    const DWORD lineColor = tocolor(GPS_LINE_R, GPS_LINE_G, GPS_LINE_B, GPS_LINE_COLOR_A);

    for (size_t i = 1; i < worldRoute.size(); ++i)
    {
        const CVector& p0 = worldRoute[i - 1];
        const CVector& p1 = worldRoute[i];
        const float x0 = mapCx + half * (p0.x / 3000.0f);
        const float y0 = mapCy - half * (p0.y / 3000.0f);
        const float x1 = mapCx + half * (p1.x / 3000.0f);
        const float y1 = mapCy - half * (p1.y / 3000.0f);
        pDraw->DrawThickLine(x0, y0, x1, y1, lineWidth, lineColor);
    }

    device->SetScissorRect(&oldScissorRect);
    device->SetRenderState(D3DRS_SCISSORTESTENABLE, oldScissorEnable);
}

void GpsRenderer::Render(DxDrawPrimitives* pDraw,
    float centerX, float centerY, float sizeX, float sizeY,
    const D3DXVECTOR3& cameraPos, const D3DXVECTOR3& cameraRot,
    float fov, float nearPlane, float farPlane,
    float rtWidth, float rtHeight, float projectionAspect,
    bool shapeCircle, float halfX, float halfY)
{
    (void)centerX; (void)centerY; (void)sizeX; (void)sizeY;
    (void)shapeCircle; (void)halfX; (void)halfY;

    if (!m_bInitialized || !pDraw)
        return;

    std::vector<CVector> worldRoute;
    if (!BuildWorldRoute(worldRoute))
        return;

    const DWORD lineColor = tocolor(GPS_LINE_R, GPS_LINE_G, GPS_LINE_B, GPS_LINE_COLOR_A);
    std::vector<RoutePoint3D> route;
    route.reserve(worldRoute.size());
    for (const CVector& point : worldRoute)
    {
        route.push_back({
            point.x + RadarGeometry::RADAR_OFFSET_X,
            point.y + RadarGeometry::RADAR_OFFSET_Y,
            RadarGeometry::RADAR_ROUTE_Z,
            lineColor
        });
    }

    float lineWidth = GPS_LINE_WIDTH * RadarGeometry::GetRadarScale();

    // Keep scissor restore even if the D3D route draw early-outs.
    struct ScissorGuard
    {
        IDirect3DDevice9* device = nullptr;
        DWORD oldEnable = 0;
        RECT oldRect{};
        bool active = false;

        ~ScissorGuard()
        {
            if (!active || !device)
                return;
            device->SetScissorRect(&oldRect);
            device->SetRenderState(D3DRS_SCISSORTESTENABLE, oldEnable);
        }
    } scissor;

    const bool canScissor = (reinterpret_cast<D3DCAPS9 const*>(RwD3D9GetCaps())->RasterCaps & D3DPRASTERCAPS_SCISSORTEST) != 0;
    if (canScissor && m_pDevice)
    {
        scissor.device = m_pDevice;
        m_pDevice->GetRenderState(D3DRS_SCISSORTESTENABLE, &scissor.oldEnable);
        m_pDevice->GetScissorRect(&scissor.oldRect);
        scissor.active = true;

        RECT rect;
        rect.left = static_cast<LONG>(centerX - sizeX * 0.5f + 2.0f);
        rect.top = static_cast<LONG>(centerY - sizeY * 0.5f + 2.0f);
        rect.right = static_cast<LONG>(centerX + sizeX * 0.5f - 2.0f);
        rect.bottom = static_cast<LONG>(centerY + sizeY * 0.5f - 2.0f);
        m_pDevice->SetRenderState(D3DRS_SCISSORTESTENABLE, TRUE);
        m_pDevice->SetScissorRect(&rect);
    }

    pDraw->dxDrawRoute3D(route, cameraPos, cameraRot, fov, nearPlane, farPlane,
        projectionAspect, lineWidth);
}
