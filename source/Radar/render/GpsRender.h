/*****************************************************************************
 *
 *  PROJECT:     The-Definitive-UI
 *  FILE:        source/Radar/render/GpsRender.h
 *  PURPOSE:     GPS route rendering on the radar plane
 *
 *****************************************************************************/

#pragma once

#include <d3d9.h>
#include <d3dx9.h>
#include <vector>

struct D3DXVECTOR2;
struct D3DXVECTOR3;

class DxDrawPrimitives;
class Draw;

class GpsRenderer
{
public:
    GpsRenderer(LPDIRECT3DDEVICE9 pDevice);
    ~GpsRenderer();

    bool Initialize();
    void Shutdown();

    // Apply GPS Redux-compatible limits while GPS is enabled; restore stock DWORDs when disabled.
    static void SetPathfindingPatchesEnabled(bool enabled);

    // Draw the current route on the custom pause-menu map plane.
    static void RenderMap2D(Draw* pDraw,
        float mapL, float mapT, float mapSize,
        float clipL, float clipT, float clipR, float clipB,
        float screenWidth);

    // Render GPS route on radar (trilogy-style 3D lines). Call when radar RT is active.
    void Render(DxDrawPrimitives* pDraw,
        float centerX, float centerY, float sizeX, float sizeY,
        const D3DXVECTOR3& cameraPos, const D3DXVECTOR3& cameraRot,
        float fov, float nearPlane, float farPlane,
        float rtWidth, float rtHeight, float projectionAspect,
        bool shapeCircle, float halfX, float halfY);

private:
    LPDIRECT3DDEVICE9 m_pDevice;
    bool m_bInitialized;
};
