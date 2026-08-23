/*****************************************************************************
 *
 *  PROJECT:     The-Definitive-UI
 *  FILE:        source/Radar/render/RadarGeometry.cpp
 *  PURPOSE:     Radar placement and scaling in screen space
 *
 *****************************************************************************/

#include "RadarGeometry.h"
#include "MathUtils.h"
#include "common.h"
#include <cmath>

const float RadarGeometry::RADAR_OFFSET_X = 3000.0f;
const float RadarGeometry::RADAR_OFFSET_Y = -3000.0f;
const float RadarGeometry::RADAR_ROUTE_Z  = 0.02f;

void RadarGeometry::WorldToRadarPos(float worldX, float worldY, float worldZ, float& outRadarX, float& outRadarY, float& outRadarZ)
{
    outRadarX = worldX + RADAR_OFFSET_X;
    outRadarY = worldY + RADAR_OFFSET_Y;
    outRadarZ = worldZ;
}

void RadarGeometry::WorldToRadarPos(float worldX, float worldY, D3DXVECTOR3& outRadarPos)
{
    outRadarPos.x = worldX + RADAR_OFFSET_X;
    outRadarPos.y = worldY + RADAR_OFFSET_Y;
    outRadarPos.z = 0.1f;
}

bool RadarGeometry::IsWithinRadarRange(const D3DXVECTOR3& posRadarSpace, const D3DXVECTOR3& playerPosRadarSpace, float range)
{
    return MathUtils::DistanceSq2D(posRadarSpace, playerPosRadarSpace) <= range * range;
}

bool RadarGeometry::WorldToCircleScreen(const D3DXVECTOR3& worldPos,
    const D3DXVECTOR3& cameraPos, const D3DXVECTOR3& cameraRot,
    float fov, float nearPlane, float farPlane,
    float rtWidth, float rtHeight, float sizeX, float sizeY,
    float centerX, float centerY,
    float& outCircleX, float& outCircleY,
    float projectionAspect)
{
    float screenX, screenY;
    if (!MathUtils::WorldToScreen(worldPos, cameraPos, cameraRot, fov, nearPlane, farPlane, rtWidth, rtHeight, screenX, screenY, projectionAspect, false))
        return false;
    float nx = screenX / rtWidth, ny = screenY / rtHeight;
    outCircleX = centerX + (nx - 0.5f) * sizeX;
    outCircleY = centerY + (ny - 0.5f) * sizeY;
    return true;
}

void RadarGeometry::ClampToOrbit(float circleScreenX, float circleScreenY,
    float centerX, float centerY, float halfX, float halfY,
    float& outX, float& outY, bool useSquare)
{
    float dx = circleScreenX - centerX, dy = circleScreenY - centerY;
    if (useSquare)
    {
        float normX = (halfX > 0.01f) ? (fabsf(dx) / halfX) : 0.0f;
        float normY = (halfY > 0.01f) ? (fabsf(dy) / halfY) : 0.0f;
        float maxNorm = (normX > normY) ? normX : normY;
        if (maxNorm > 1.0f && maxNorm > 0.01f)
        {
            float scale = 1.0f / maxNorm;
            dx *= scale;
            dy *= scale;
        }
        outX = centerX + dx;
        outY = centerY + dy;
    }
    else
    {
        float r = (halfX < halfY) ? halfX : halfY;
        float len = sqrtf(dx * dx + dy * dy);
        if (len > 0.01f) { dx /= len; dy /= len; }
        outX = centerX + dx * r;
        outY = centerY + dy * r;
    }
}

void RadarGeometry::PointOnOrbitEdge(float centerX, float centerY, float halfX, float halfY,
    float cosA, float sinA, bool useSquare,
    float& outX, float& outY)
{
    if (useSquare)
    {
        float tx = (fabsf(cosA) > 1e-6f) ? (halfX / fabsf(cosA)) : 1e6f;
        float ty = (fabsf(sinA) > 1e-6f) ? (halfY / fabsf(sinA)) : 1e6f;
        float t = (tx < ty) ? tx : ty;
        outX = centerX + cosA * t;
        outY = centerY + sinA * t;
    }
    else
    {
        float r = (halfX < halfY) ? halfX : halfY;
        outX = centerX + cosA * r;
        outY = centerY + sinA * r;
    }
}

bool RadarGeometry::IsInsideOrbit(float x, float y,
    float centerX, float centerY, float halfX, float halfY,
    bool useSquare)
{
    float dx = x - centerX, dy = y - centerY;
    if (useSquare)
        return (fabsf(dx) <= halfX && fabsf(dy) <= halfY);
    float r = (halfX < halfY) ? halfX : halfY;
    return (dx * dx + dy * dy) <= (r * r);
}

float RadarGeometry::GetRadarScale()
{
    const float baseWidth = 1920.0f, baseHeight = 1080.0f;
    float sw = (float)RsGlobal.maximumWidth, sh = (float)RsGlobal.maximumHeight;
    return (sw / baseWidth + sh / baseHeight) * 0.5f;
}

void RadarGeometry::GetRadarHalfExtents(float sizeX, float sizeY, float& halfX, float& halfY)
{
    GetRadarHalfExtents(sizeX, sizeY, 3.0f, true, halfX, halfY);
}

void RadarGeometry::GetRadarHalfExtents(float sizeX, float sizeY, float borderThicknessPx, bool shapeCircle, float& halfX, float& halfY)
{
    if (shapeCircle)
    {
        float minSize = (sizeX < sizeY ? sizeX : sizeY);
        float r = minSize * 0.5f - borderThicknessPx * 0.5f;
        halfX = halfY = (r > 1.0f) ? r : minSize * 0.5f - 1.0f;
    }
    else
    {
        halfX = sizeX * 0.5f + borderThicknessPx * 0.5f;
        halfY = sizeY * 0.5f + borderThicknessPx * 0.5f;
    }
}
