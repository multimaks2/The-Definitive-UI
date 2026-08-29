/*****************************************************************************
 *
 *  PROJECT:     The-Definitive-UI
 *  FILE:        source/Radar/render/draw/DxDrawPrimitives.h
 *  PURPOSE:     D3D9 primitives with full render-state save / restore
 *
 *****************************************************************************/

#pragma once

#include <d3d9.h>
#include <d3dx9.h>
#include <vector>
#include "RadarTypes.h"
#include "BlipTypes.h"
#include "DrawResources.h"

// Route point in radar 3D space (x+3000, y-3000, z)
struct RoutePoint3D
{
    float x, y, z;
    DWORD color;
};

class DxDrawPrimitives
{
public:
    DxDrawPrimitives(LPDIRECT3DDEVICE9 pDevice);
    ~DxDrawPrimitives();

    bool Initialize();
    bool Initialize(const DrawResources* pResources);
    void Shutdown();

    void dxDrawCircleShader(float x, float y, float width, float height, LPDIRECT3DTEXTURE9 texture = nullptr, DWORD color = 0xFFFFFFFF, float alpha = 1.0f);
    void dxDrawBorderShader(float x, float y, float width, float height, DWORD color = 0xFFFFFFFF, float alpha = 1.0f, float borderThicknessPixels = 0.0f);
    void dxDrawGreenSquareFill(float x, float y, float width, float height, LPDIRECT3DTEXTURE9 texture, DWORD color, float fillLevel);
    void dxDrawImage2D(float x, float y, float width, float height, LPDIRECT3DTEXTURE9 texture, DWORD color = 0xFFFFFFFF);
    void dxDrawImage2DRotated(float x, float y, float width, float height, LPDIRECT3DTEXTURE9 texture, float rotationAngle, DWORD color = 0xFFFFFFFF);
    // Draw in pixel space of an offscreen surface (e.g. radar blip RT).
    void dxDrawImage2DRotatedSurface(float x, float y, float width, float height, LPDIRECT3DTEXTURE9 texture,
                                   float rotationAngle, DWORD color, float surfaceWidth, float surfaceHeight);
    void dxDrawGTAIndicatorBlipSurface(float x, float y, float size, DWORD color, eHeightIndicatorType type,
                                       float surfaceWidth, float surfaceHeight);
    void dxDrawRectangle(float x, float y, float width, float height, DWORD color = 0xFFFFFFFF);
    void dxDrawGTAIndicatorBlip(float screenX, float screenY, float size, DWORD color, eHeightIndicatorType type);
    void dxDrawImage3D(const D3DXVECTOR3& elementPos, const D3DXVECTOR3& elementRot, const D3DXVECTOR2& elementSize,
                       const D3DXVECTOR3& cameraPos, const D3DXVECTOR3& cameraRot,
                       float fov, float nearPlane, float farPlane,
                       LPDIRECT3DTEXTURE9 texture, DWORD color = 0xFFFFFFFF);
    void dxDrawText(const char* text, float x, float y, float sx, float sy, float rotation, DWORD color);
    void dxDrawRoute3D(const std::vector<RoutePoint3D>& route, const D3DXVECTOR3& cameraPos, const D3DXVECTOR3& cameraRot,
                       float fov, float nearPlane, float farPlane, float aspect, float lineWidth);
    void dxDrawLine3D(const D3DXVECTOR3& start, const D3DXVECTOR3& end, float width, DWORD color,
                      const D3DXVECTOR3& cameraPos, const D3DXVECTOR3& cameraRot,
                      float fov, float nearPlane, float farPlane, float aspect);

    void Setup2DSpriteStates();
    void Setup2DSpriteStatesForSurface(float surfaceWidth, float surfaceHeight);
    void CreateScreenQuad(ScreenVertex* vertices, float x, float y, float width, float height, DWORD color);
    RenderStates SaveRenderStates();
    void         RestoreRenderStates(const RenderStates& states);

    LPDIRECT3DDEVICE9 GetDevice() const { return m_pDevice; }

private:
    LPDIRECT3DDEVICE9   m_pDevice;
    const DrawResources* m_pResources;
    bool                m_bInitialized;
};
