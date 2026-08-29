/*****************************************************************************
 *
 *  PROJECT:     The-Definitive-UI
 *  FILE:        source/Shader/Shader.h
 *  PURPOSE:     Map fog pixel shaders - ZonesVisited arcs and prebuilt mask
 *
 *****************************************************************************/

#pragma once

#include <d3d9.h>

// Map fog: ZonesVisited sampled in PS with convex/concave arcs matching orange contour
class Shader
{
public:
    Shader();
    ~Shader();

    bool Initialize(LPDIRECT3DDEVICE9 pDevice);
    void Shutdown();
    void OnDeviceLost();

    bool IsReady() const { return m_pFogPS != nullptr || m_pFogMaskPS != nullptr; }
    bool HasZonesFog() const { return m_pFogPS != nullptr; }

    // zones: n×n A8R8G8B8, R>0 = explored. cornerFrac e.g. 0.175, softW in cell units
    // UV (u0,v0)-(u1,v1) on the draw quad; 0..1 = map square (sea is outside)
    void DrawFogFromZones(LPDIRECT3DTEXTURE9 pZones, float fX, float fY, float fWidth, float fHeight,
                          int nCells, float cornerFrac, DWORD dwFogColor, float softW = 0.14f,
                          float u0 = 0.0f, float v0 = 0.0f, float u1 = 1.0f, float v1 = 1.0f);

    void DrawFogFromMask(LPDIRECT3DTEXTURE9 pMask, float fX, float fY, float fWidth, float fHeight,
                         DWORD dwFogColor);

private:
    bool CreateFogPixelShader();

    LPDIRECT3DDEVICE9      m_pDevice = nullptr;
    IDirect3DPixelShader9* m_pFogPS = nullptr;     // zones + corners
    IDirect3DPixelShader9* m_pFogMaskPS = nullptr; // prebuilt mask
    bool                   m_bInitialized = false;
};
