/*****************************************************************************
 *
 *  PROJECT:     The-Definitive-UI
 *  FILE:        source/Utils/MathUtils.cpp
 *  PURPOSE:     Vector math, world-to-screen projection and distances
 *
 *****************************************************************************/

#include "MathUtils.h"
#include "RenderWare.h"
#include <d3dx9.h>
#include <cmath>
#include <cfloat>

bool MathUtils::WorldToScreen(const D3DXVECTOR3& worldPos,
                              const D3DXVECTOR3& cameraPos,
                              const D3DXVECTOR3& cameraRot,
                              float fov,
                              float nearPlane,
                              float farPlane,
                              float screenWidth,
                              float screenHeight,
                              float& screenX,
                              float& screenY,
                              float projectionAspect,
                              bool clipDepth)
{
    D3DXMATRIX camWorld;
    {
        float pitch = cameraRot.x;
        float yaw = cameraRot.z;

        float cp = cosf(pitch);
        float sp = sinf(pitch);
        float cy = cosf(yaw);
        float sy = sinf(yaw);

        camWorld._11 = cy * 1.0f - sy * sp * 0.0f;
        camWorld._12 = 1.0f * sy + cy * sp * 0.0f;
        camWorld._13 = -cp * 0.0f;
        camWorld._14 = 0.0f;

        camWorld._21 = -cp * sy;
        camWorld._22 = cy * cp;
        camWorld._23 = sp;
        camWorld._24 = 0.0f;

        camWorld._31 = cy * 0.0f + 1.0f * sy * sp;
        camWorld._32 = sy * 0.0f - cy * 1.0f * sp;
        camWorld._33 = cp * 1.0f;
        camWorld._34 = 0.0f;

        camWorld._41 = cameraPos.x;
        camWorld._42 = cameraPos.y;
        camWorld._43 = cameraPos.z;
        camWorld._44 = 1.0f;
    }

    D3DXVECTOR3 forwardVec(camWorld._21, camWorld._22, camWorld._23);
    D3DXVECTOR3 downVec(0.0f, 0.0f, -1.0f);
    float dotProduct = D3DXVec3Dot(&downVec, &forwardVec);
    if (dotProduct > 1.0f) dotProduct = 1.0f;
    if (dotProduct < -1.0f) dotProduct = -1.0f;
    float rotOff = 600.0f * acosf(dotProduct) / (0.5f * D3DX_PI);

    D3DXVECTOR3 offX(
        camWorld._11 + camWorld._21 - rotOff * camWorld._31,
        camWorld._12 + camWorld._22 - rotOff * camWorld._32,
        camWorld._13 + camWorld._23 - rotOff * camWorld._33
    );

    D3DXVECTOR3 viewPos(camWorld._41 + offX.x, camWorld._42 + offX.y, camWorld._43 + offX.z);
    D3DXVECTOR3 zaxis = forwardVec;
    D3DXVec3Normalize(&zaxis, &zaxis);

    D3DXVECTOR3 upVec(camWorld._31, camWorld._32, camWorld._33);
    D3DXVECTOR3 xaxis, yaxis;
    D3DXVECTOR3 negUpVec(-upVec.x, -upVec.y, -upVec.z);
    D3DXVec3Cross(&xaxis, &negUpVec, &zaxis);
    D3DXVec3Normalize(&xaxis, &xaxis);
    D3DXVec3Cross(&yaxis, &xaxis, &zaxis);

    D3DXMATRIX view;
    view._11 = xaxis.x; view._12 = yaxis.x; view._13 = zaxis.x; view._14 = 0.0f;
    view._21 = xaxis.y; view._22 = yaxis.y; view._23 = zaxis.y; view._24 = 0.0f;
    view._31 = xaxis.z; view._32 = yaxis.z; view._33 = zaxis.z; view._34 = 0.0f;
    view._41 = -D3DXVec3Dot(&xaxis, &viewPos);
    view._42 = -D3DXVec3Dot(&yaxis, &viewPos);
    view._43 = -D3DXVec3Dot(&zaxis, &viewPos);
    view._44 = 1.0f;

    float aspect = (projectionAspect > 0.0f) ? projectionAspect : (screenHeight / screenWidth);
    float w = 1.0f / tanf(fov * 0.5f);
    float h = w / aspect;
    float Q = farPlane / (farPlane - nearPlane);

    D3DXMATRIX proj;
    D3DXMatrixIdentity(&proj);
    proj._11 = w;
    proj._22 = h;
    proj._33 = Q;
    proj._34 = 1.0f;
    proj._43 = -Q * nearPlane;
    proj._44 = 0.0f;

    D3DXVECTOR4 worldPos4(worldPos.x, worldPos.y, worldPos.z, 1.0f);
    D3DXVECTOR4 viewPos4, clipPos4;

    D3DXVec4Transform(&viewPos4, &worldPos4, &view);
    D3DXVec4Transform(&clipPos4, &viewPos4, &proj);

    if (clipPos4.w <= 0.0f)
        return false;
    if (clipDepth && (clipPos4.z < 0.0f || clipPos4.z > clipPos4.w))
        return false;

    float invW = 1.0f / clipPos4.w;
    float ndcX = clipPos4.x * invW;
    float ndcY = clipPos4.y * invW;

    screenX = (ndcX + 1.0f) * 0.5f * screenWidth;
    screenY = (1.0f - ndcY) * 0.5f * screenHeight;

    return true;
}

void MathUtils::CalculateRadarPosition(float& circleX, float& circleY, float& circleSize)
{
    const float baseSize = 265.0f;
    float sizeX, sizeY;
    CalculateRadarPosition(circleX, circleY, sizeX, sizeY, baseSize, baseSize, baseSize, true, 85.0f, 55.0f);
    circleSize = sizeX;
}

void MathUtils::CalculateRadarPosition(float& circleX, float& circleY, float& sizeX, float& sizeY,
                                       float baseSizeCircle, float baseSizeSquareX, float baseSizeSquareY, bool shapeCircle,
                                       float baseOffsetX, float baseOffsetY)
{
    const float baseWidth = 1920.0f;
    const float baseHeight = 1080.0f;

    float screenWidth = (float)RsGlobal.maximumWidth;
    float screenHeight = (float)RsGlobal.maximumHeight;

    float scaleX = screenWidth / baseWidth;
    float scaleY = screenHeight / baseHeight;
    float scale = (scaleX + scaleY) * 0.5f;

    if (shapeCircle)
    {
        sizeX = baseSizeCircle * scale;
        sizeY = sizeX;
    }
    else
    {
        sizeX = baseSizeSquareX * scale;
        sizeY = baseSizeSquareY * scale;
    }

    circleX = baseOffsetX * scaleX;
    circleY = screenHeight - (baseOffsetY * scaleY) - sizeY;
}

float MathUtils::GetRadarProjectionAspect()
{
    return 1080.0f / 1920.0f;
}

float MathUtils::ScaleRadarLength(float basePx)
{
    const float scaleX = static_cast<float>(RsGlobal.maximumWidth) / 1920.0f;
    const float scaleY = static_cast<float>(RsGlobal.maximumHeight) / 1080.0f;
    const float scaled = basePx * (scaleX + scaleY) * 0.5f;
    return (scaled < 1.0f) ? 1.0f : scaled;
}

float MathUtils::CalculateDistance2D(const D3DXVECTOR3& a, const D3DXVECTOR3& b)
{
    return sqrtf(DistanceSq2D(a, b));
}

float MathUtils::DistanceSq2D(const D3DXVECTOR3& a, const D3DXVECTOR3& b)
{
    float dx = a.x - b.x;
    float dy = a.y - b.y;
    return dx * dx + dy * dy;
}

float MathUtils::DistanceSq2D(float ax, float ay, float bx, float by)
{
    float dx = ax - bx;
    float dy = ay - by;
    return dx * dx + dy * dy;
}

bool MathUtils::DirectionToOrbitAngle(const D3DXVECTOR3& fromPos, const D3DXVECTOR3& toPos, float yaw, float& outAngle)
{
    float dx = toPos.x - fromPos.x;
    float dy = toPos.y - fromPos.y;
    float lenSq = dx * dx + dy * dy;
    if (lenSq < 0.0001f)
        return false;
    float len = sqrtf(lenSq);
    float worldAngle = atan2f(dy, dx);
    outAngle = -worldAngle - yaw;
    return true;
}

bool MathUtils::IsMapQuadInView(float minX, float minY, float maxX, float maxY,
                                const D3DXVECTOR3& cameraPos, const D3DXVECTOR3& cameraRot,
                                float fov, float nearPlane, float farPlane,
                                float screenWidth, float screenHeight, float projectionAspect,
                                float ndcMargin)
{
    // WorldToScreen accepts any point in front of the camera (including outside
    // FOV). Rebuild NDC and require the projected AABB to overlap the view rect.
    const float z = 0.1f;
    const float xs[5] = { minX, maxX, maxX, minX, (minX + maxX) * 0.5f };
    const float ys[5] = { minY, minY, maxY, maxY, (minY + maxY) * 0.5f };

    float minNdcX =  FLT_MAX;
    float maxNdcX = -FLT_MAX;
    float minNdcY =  FLT_MAX;
    float maxNdcY = -FLT_MAX;
    int   projected = 0;

    for (int i = 0; i < 5; ++i) {
        float sx = 0.0f, sy = 0.0f;
        if (!WorldToScreen(D3DXVECTOR3(xs[i], ys[i], z),
                           cameraPos, cameraRot, fov, nearPlane, farPlane,
                           screenWidth, screenHeight, sx, sy, projectionAspect, true))
            continue;

        const float ndcX = (sx / screenWidth) * 2.0f - 1.0f;
        const float ndcY = 1.0f - (sy / screenHeight) * 2.0f;
        if (ndcX < minNdcX) minNdcX = ndcX;
        if (ndcX > maxNdcX) maxNdcX = ndcX;
        if (ndcY < minNdcY) minNdcY = ndcY;
        if (ndcY > maxNdcY) maxNdcY = ndcY;
        ++projected;
    }

    if (projected == 0)
        return false;

    const float lo = -1.0f - ndcMargin;
    const float hi =  1.0f + ndcMargin;
    return maxNdcX >= lo && minNdcX <= hi && maxNdcY >= lo && minNdcY <= hi;
}
