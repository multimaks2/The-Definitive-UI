/*****************************************************************************
 *
 *  PROJECT:     The-Definitive-UI
 *  FILE:        source/Radar/render/RadarGeometry.h
 *  PURPOSE:     Radar placement and scaling in screen space
 *
 *****************************************************************************/

#pragma once

#include <d3d9.h>
#include <d3dx9.h>

class RadarGeometry
{
public:
    static bool WorldToCircleScreen(const D3DXVECTOR3& worldPos,
        const D3DXVECTOR3& cameraPos, const D3DXVECTOR3& cameraRot,
        float fov, float nearPlane, float farPlane,
        float rtWidth, float rtHeight, float sizeX, float sizeY,
        float centerX, float centerY,
        float& outCircleX, float& outCircleY,
        float projectionAspect = 0.0f);

    static void ClampToOrbit(float circleScreenX, float circleScreenY,
        float centerX, float centerY, float halfX, float halfY,
        float& outX, float& outY, bool useSquare);

    static void PointOnOrbitEdge(float centerX, float centerY, float halfX, float halfY,
        float cosA, float sinA, bool useSquare,
        float& outX, float& outY);

    static bool IsInsideOrbit(float x, float y,
        float centerX, float centerY, float halfX, float halfY,
        bool useSquare);

    static float GetRadarScale();
    static void GetRadarHalfExtents(float sizeX, float sizeY, float& halfX, float& halfY);
    // Orbit at middle of border: pass borderThicknessPx and shapeCircle
    static void GetRadarHalfExtents(float sizeX, float sizeY, float borderThicknessPx, bool shapeCircle, float& halfX, float& halfY);

    // GTA SA radar space offset (world -> radar: x+3000, y-3000)
    static const float RADAR_OFFSET_X;
    static const float RADAR_OFFSET_Y;
    // Built-in GPS route height in radar space. Third-party GPS Redux HUD
    // overlays must project at this same Z on the 3D radar or the line floats.
    static const float RADAR_ROUTE_Z;

    static void WorldToRadarPos(float worldX, float worldY, float worldZ, float& outRadarX, float& outRadarY, float& outRadarZ);
    static void WorldToRadarPos(float worldX, float worldY, D3DXVECTOR3& outRadarPos);
    static bool IsWithinRadarRange(const D3DXVECTOR3& posRadarSpace, const D3DXVECTOR3& playerPosRadarSpace, float range);
};
