/*****************************************************************************
 *
 *  PROJECT:     The-Definitive-UI
 *  FILE:        source/Utils/MathUtils.h
 *  PURPOSE:     Vector math, world-to-screen projection and distances
 *
 *****************************************************************************/

#pragma once

#include <d3d9.h>
#include <d3dx9.h>

class MathUtils
{
public:
    // projectionAspect: if > 0 use for projection (e.g. screen height/width); if <= 0 use screenHeight/screenWidth
    static bool  WorldToScreen(const D3DXVECTOR3& worldPos, const D3DXVECTOR3& cameraPos, const D3DXVECTOR3& cameraRot, float fov, float nearPlane,
                               float farPlane, float screenWidth, float screenHeight, float& screenX, float& screenY, float projectionAspect = 0.0f,
                               bool clipDepth = true);
    static void  CalculateRadarPosition(float& circleX, float& circleY, float& circleSize);
    // With configurable base sizes and shape; outputs sizeX, sizeY (for circle sizeX==sizeY)
    // baseOffsetX = offset from left edge (Full HD: 85), baseOffsetY = offset from bottom edge (Full HD: 55)
    static void  CalculateRadarPosition(float& circleX, float& circleY, float& sizeX, float& sizeY,
                                       float baseSizeCircle, float baseSizeSquareX, float baseSizeSquareY, bool shapeCircle,
                                       float baseOffsetX, float baseOffsetY);
    // 3D map FOV aspect (1080/1920). Independent of window aspect so tilt stays stable.
    // Flat 2D mode uses 1.0 (square RT / orthographic-style top-down).
    static float GetRadarProjectionAspect();
    static float GetRadarProjectionAspect(bool flat2D);

    // Shared radar camera: trilogy tilted (flat2D=false) or true top-down (flat2D=true, no rotOff).
    static void BuildRadarViewProj(const D3DXVECTOR3& cameraPos, const D3DXVECTOR3& cameraRot,
                                   float fov, float nearPlane, float farPlane, float aspect, bool flat2D,
                                   D3DXMATRIX& outView, D3DXMATRIX& outProj);
    // Length in screen px from a Full HD (1920x1080) design value.
    static float ScaleRadarLength(float basePx);
    static float CalculateDistance2D(const D3DXVECTOR3& a, const D3DXVECTOR3& b);
    static float DistanceSq2D(const D3DXVECTOR3& a, const D3DXVECTOR3& b);
    static float DistanceSq2D(float ax, float ay, float bx, float by);
    // Direction from A to B, normalised; returns orbit angle for PointOnOrbitEdge (or false if too close)
    static bool  DirectionToOrbitAngle(const D3DXVECTOR3& fromPos, const D3DXVECTOR3& toPos, float yaw, float& outAngle);

    // Radar map-tile cull: true if the XY quad intersects the camera view cone
    // on the ground (NDC AABB overlap), not merely "in front of the camera".
    static bool IsMapQuadInView(float minX, float minY, float maxX, float maxY,
                                const D3DXVECTOR3& cameraPos, const D3DXVECTOR3& cameraRot,
                                float fov, float nearPlane, float farPlane,
                                float screenWidth, float screenHeight, float projectionAspect,
                                float ndcMargin = -1.0f);
};
