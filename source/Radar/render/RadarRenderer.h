/*****************************************************************************
 *
 *  PROJECT:     The-Definitive-UI
 *  FILE:        source/Radar/render/RadarRenderer.h
 *  PURPOSE:     HUD radar renderer - tiles, blips, overlays and chrome
 *
 *****************************************************************************/

#pragma once

#include <d3d9.h>
#include <d3dx9.h>
#include <vector>
#include "RadarTypes.h"
#include "ColorUtils.h"
#include "BlipTypes.h"
#include "DrawResources.h"

class ShaderManager;
class CameraController;
class MapChunkManager;
class BlipManager;
class GpsRenderer;
class GangZoneRenderer;
class DxDrawPrimitives;
class RenderTarget;
class RenderRadio;
class CPed;

class RadarRenderer
{
public:
    RadarRenderer();
    ~RadarRenderer();

    bool Initialize(LPDIRECT3DDEVICE9 pDevice);
    void Shutdown();
    void Render();
    void MarkRadioNameVisible();

    LPDIRECT3DDEVICE9 GetDevice() const { return m_pd3dDevice; }

    bool GetShowGangZones() const { return m_bShowGangZones; }
    bool GetRadarShapeCircle() const { return m_bRadarShapeCircle; }
    bool GetBorderShapeCircle() const { return m_bBorderShapeCircle; }
    void SetShowGangZones(bool value);
    void SetRadarShapeCircle(bool value) { m_bRadarShapeCircle = value; }
    void SetBorderShapeCircle(bool value) { m_bBorderShapeCircle = value; }

    DxDrawPrimitives* GetDraw() const { return m_pDraw; }
    RenderTarget*     GetRenderTarget() const { return m_pRenderTarget; }
    MapChunkManager*  GetMapChunkManager() const { return m_pMapChunkManager; }

    void dxDrawCircleShader(float x, float y, float width, float height, LPDIRECT3DTEXTURE9 texture = nullptr,
                            DWORD color = 0xFFFFFFFF, float alpha = 1.0f);
    void dxDrawBorderShader(float x, float y, float width, float height, DWORD color = 0xFFFFFFFF,
                            float alpha = 1.0f, float borderThicknessPixels = 0.0f);
    void dxDrawGreenSquareFill(float x, float y, float width, float height, LPDIRECT3DTEXTURE9 texture, DWORD color, float fillLevel);
    void dxDrawImage2D(float x, float y, float width, float height, LPDIRECT3DTEXTURE9 texture, DWORD color = 0xFFFFFFFF);
    void dxDrawImage2DRotated(float x, float y, float width, float height, LPDIRECT3DTEXTURE9 texture, float rotationAngle,
                             DWORD color = 0xFFFFFFFF);
    void dxDrawRectangle(float x, float y, float width, float height, DWORD color = 0xFFFFFFFF);
    void dxDrawGTAIndicatorBlip(float screenX, float screenY, float size, DWORD color, eHeightIndicatorType type);
    void dxDrawImage3D(const D3DXVECTOR3& elementPos, const D3DXVECTOR3& elementRot, const D3DXVECTOR2& elementSize,
                      const D3DXVECTOR3& cameraPos, const D3DXVECTOR3& cameraRot,
                      float fov, float nearPlane, float farPlane,
                      LPDIRECT3DTEXTURE9 texture, DWORD color = 0xFFFFFFFF);
    void dxDrawText(const char* text, float x, float y, float sx, float sy, float rotation, DWORD color);

    LPDIRECT3DTEXTURE9 GetRenderTargetTexture() const;

    /**
     * Отрисовка текста радио (only from radar overlays, inside D3D stateblock)
     */
    void RenderRadioText();

private:
    bool InitializeResources();
    void CleanupResources();
    void CalculateRadarPosition(float& circleX, float& circleY, float& sizeX, float& sizeY);
    float CalculateBlipSize(float baseBlipSize = 24.0f, float baseWidth = 1920.0f) const;

    void RenderRadarTargetContents(float circleX, float circleY, float sizeX, float sizeY);
    void RenderRadarOverlays(float circleX, float circleY, float sizeX, float sizeY);
    void RenderRadarSprites(float circleX, float circleY, float sizeX, float sizeY);
    void RenderAirstrips(float circleX, float circleY, float sizeX, float sizeY);
    void RenderMissiles(float circleX, float circleY, float sizeX, float sizeY);

    void UpdateDrawResources();
    bool BeginRadarZone();
    void EndRadarZone();
    void ReleaseRadarStateBlock();

#ifdef _DEBUG
    void dxDrawRenderDebugMemory();
#endif

    bool                  m_bShowGangZones;
    bool                  m_bRadarShapeCircle;
    bool                  m_bBorderShapeCircle;

    LPDIRECT3DDEVICE9     m_pd3dDevice;
    IDirect3DStateBlock9* m_pStateBlock;
    ShaderManager*       m_pShaderManager;
    CameraController*     m_pCameraController;
    MapChunkManager*     m_pMapChunkManager;
    BlipManager*         m_pBlipManager;
    GangZoneRenderer*    m_pGangZoneRenderer;
    GpsRenderer*         m_pGpsRenderer;
    RenderRadio*         m_pRenderRadio;
    DxDrawPrimitives*    m_pDraw;
    RenderTarget*        m_pRenderTarget;

    DrawResources        m_drawResources;

    LPDIRECT3DVERTEXBUFFER9 m_pTriangleVB;

    LPD3DXEFFECT          m_pTriangleEffect;
    LPD3DXEFFECT          m_pBorderEffect;
    LPD3DXEFFECT          m_pImage3DEffect;
    LPD3DXEFFECT          m_pLineEffect;
    LPD3DXEFFECT          m_pLineSmoothEffect;
    LPD3DXEFFECT          m_pGreenSquareEffect;

    LPDIRECT3DTEXTURE9    m_pCircleTexture;
    LPDIRECT3DTEXTURE9    m_pNorthTexture;
    LPDIRECT3DTEXTURE9    m_pLineTexture;
    LPDIRECT3DTEXTURE9    m_pRadarRingPlaneTexture;

    LPD3DXFONT            m_pFont;
    LPD3DXSPRITE          m_pFontSprite;
    LPD3DXFONT            m_pRadioFont;        // Шрифт для текста радио (большего размера)
    LPD3DXSPRITE          m_pRadioFontSprite;  // Спрайт для шрифта радио

    int                   m_width;
    int                   m_height;

    CPed*                 m_cachedPlayer;
    bool                  m_cachedIsInAircraft;
    bool                  m_cachedIsInPlane;   // plane only - avionics (airstrip, roll ring) shown only in plane
    float                 m_cachedRollAngle;
    float                 m_cachedPitchAngle;

    float                 m_nearPlane;
    float                 m_farPlane;
    float                 m_initialAircraftAltitude;
    bool                  m_bWasInAircraft;

    bool                  m_bWasInInterior;
    unsigned int          m_exitInteriorImageStartTime;
    LPDIRECT3DTEXTURE9    m_pExitInteriorTexture;
    bool                  m_bRadioNameVisible;

    bool                  m_bInitialized;

#ifdef _DEBUG
    int                   m_debugChunksRenderedLastFrame;
#endif
};
