/*****************************************************************************
 *
 *  PROJECT:     The-Definitive-UI
 *  FILE:        source/Radar/render/RadarRenderer.cpp
 *  PURPOSE:     HUD radar renderer - tiles, blips, overlays and chrome
 *
 *****************************************************************************/

#include "RadarRenderer.h"
#include "RadarGeometry.h"
#include "RadarViewContext.h"
#include "GameState.h"
#include "Config.h"
#include "Radar.h"
#include "DxDrawPrimitives.h"
#include "RenderTarget.h"
#include "ModPaths.h"
#include "ShaderManager.h"
#include "CameraController.h"
#include "MapChunkManager.h"
#include "BlipManager.h"
#include "StockRadarDraw.h"
#include "GangZoneRenderer.h"
#include "GpsRender.h"
#include "RenderRadio.h"
#include "MathUtils.h"
#include <cstring>
#include <cmath>
#include <cfloat>
#include <vector>
#include "plugin.h"
#include "common.h"
#include "RenderWare.h"
#include "CWorld.h"
#include "CPlayerPed.h"
#include "CModelInfo.h"
#include "CTimer.h"
#include "CPools.h"
#include "CTimer.h"
#include "Base64Image.h"
#include "CPed.h"
#include "CVehicle.h"
#include "CProjectileInfo.h"
#include "CProjectile.h"
#include "eWeaponType.h"
#include "CRadar.h"
#include "CAERadioTrackManager.h"
#include "CHudColours.h"
#include "LanguageManager.h"
#ifdef _DEBUG
#include <Psapi.h>
#pragma comment(lib, "Psapi.lib")
#endif

namespace
{
    struct AirstripInfo
    {
        float posX, posY;
        float direction;
        float radius;
    };

    const AirstripInfo kAirstrips[] = {
        { +1750.0f, -2494.0f, 180.0f, 1000.0f * 0.75f },
        { -1373.0f,  +120.0f, 225.0f, 1500.0f * 0.75f },
        { +1478.0f, +1461.0f,  90.0f, 1200.0f * 0.75f },
        {  +175.0f, +2502.0f, 180.0f, 1000.0f * 0.75f },
    };
    constexpr int kAirstripCount = sizeof(kAirstrips) / sizeof(kAirstrips[0]);

    int NearestAirstrip(float worldX, float worldY)
    {
        float minDistSq = FLT_MAX;
        int index = 0;
        for (int i = 0; i < kAirstripCount; ++i)
        {
            const float distSq = MathUtils::DistanceSq2D(worldX, worldY, kAirstrips[i].posX, kAirstrips[i].posY);
            if (distSq < minDistSq)
            {
                minDistSq = distSq;
                index = i;
            }
        }
        return index;
    }

    bool IsPlayerOnRunwaySegment(float point1X, float point1Y, float point2X, float point2Y,
                                 float playerX, float playerY, float runwayWidth)
    {
        const float vx = point2X - point1X;
        const float vy = point2Y - point1Y;
        const float lenSq = vx * vx + vy * vy;
        if (lenSq < 1e-6f)
            return false;
        const float wx = playerX - point1X;
        const float wy = playerY - point1Y;
        const float t = (wx * vx + wy * vy) / lenSq;
        if (t < 0.0f || t > 1.0f)
            return false;
        const float projX = point1X + t * vx;
        const float projY = point1Y + t * vy;
        return MathUtils::DistanceSq2D(playerX, playerY, projX, projY) <= runwayWidth * runwayWidth;
    }

    bool WorldToCircleOffset(float worldX, float worldY,
        const D3DXVECTOR3& cameraPos, const D3DXVECTOR3& cameraRot, float fov, float nearPlane, float farPlane,
        float rtWidth, float rtHeight, float sizeX, float sizeY,
        float& outCircleX, float& outCircleY, float projectionAspect)
    {
        D3DXVECTOR3 worldPos;
        RadarGeometry::WorldToRadarPos(worldX, worldY, worldPos);
        float screenX = 0.0f;
        float screenY = 0.0f;
        if (!MathUtils::WorldToScreen(worldPos, cameraPos, cameraRot, fov, nearPlane, farPlane,
                                      rtWidth, rtHeight, screenX, screenY, projectionAspect))
            return false;
        outCircleX = (screenX / rtWidth - 0.5f) * sizeX;
        outCircleY = (screenY / rtHeight - 0.5f) * sizeY;
        return true;
    }

    bool GetAirstripOffsetRangeInsideCircle(float stripCenterX, float stripCenterY, float stripDirRad, float halfLen,
        float centerX, float centerY, float innerRadius,
        const D3DXVECTOR3& cameraPos, const D3DXVECTOR3& cameraRot, float fov, float nearPlane, float farPlane,
        float rtWidth, float rtHeight, float sizeX, float sizeY,
        float& outMinOffset, float& outMaxOffset, float projectionAspect)
    {
        const float stepWorld = 100.0f;
        const float cosDir = cosf(stripDirRad);
        const float sinDir = sinf(stripDirRad);
        float cx = 0.0f, cy = 0.0f, sx = 0.0f, sy = 0.0f;
        if (!WorldToCircleOffset(stripCenterX, stripCenterY, cameraPos, cameraRot, fov, nearPlane, farPlane,
                                 rtWidth, rtHeight, sizeX, sizeY, cx, cy, projectionAspect))
            return false;
        if (!WorldToCircleOffset(stripCenterX + stepWorld * cosDir, stripCenterY + stepWorld * sinDir,
                                 cameraPos, cameraRot, fov, nearPlane, farPlane,
                                 rtWidth, rtHeight, sizeX, sizeY, sx, sy, projectionAspect))
            return false;
        const float Dx = (sx - cx) / stepWorld;
        const float Dy = (sy - cy) / stepWorld;
        const float a = Dx * Dx + Dy * Dy;
        if (a < 1e-10f)
            return false;
        const float b = 2.0f * (cx * Dx + cy * Dy);
        const float c = cx * cx + cy * cy - innerRadius * innerRadius;
        const float disc = b * b - 4.0f * a * c;
        if (disc < 0.0f)
        {
            if (c <= 0.0f)
            {
                outMinOffset = -halfLen;
                outMaxOffset = halfLen;
                return true;
            }
            return false;
        }
        const float sqrtDisc = sqrtf(disc);
        const float w1 = (-b - sqrtDisc) / (2.0f * a);
        const float w2 = (-b + sqrtDisc) / (2.0f * a);
        outMinOffset = (w1 < w2) ? w1 : w2;
        outMaxOffset = (w1 < w2) ? w2 : w1;
        if (outMinOffset < -halfLen)
            outMinOffset = -halfLen;
        if (outMaxOffset > halfLen)
            outMaxOffset = halfLen;
        return outMinOffset <= outMaxOffset;
    }
}

RadarRenderer::RadarRenderer()
    : m_pd3dDevice(nullptr)
    , m_pStateBlock(nullptr)
    , m_pShaderManager(nullptr)
    , m_pCameraController(nullptr)
    , m_pMapChunkManager(nullptr)
    , m_pBlipManager(nullptr)
    , m_pGangZoneRenderer(nullptr)
    , m_pGpsRenderer(nullptr)
    , m_pRenderRadio(nullptr)
    , m_pFont(nullptr)
    , m_pFontSprite(nullptr)
    , m_pRadioFont(nullptr)
    , m_pRadioFontSprite(nullptr)
    , m_pTriangleVB(nullptr)
    , m_pTriangleEffect(nullptr)
    , m_pBorderEffect(nullptr)
    , m_pImage3DEffect(nullptr)
    , m_pLineEffect(nullptr)
    , m_pLineSmoothEffect(nullptr)
    , m_pGreenSquareEffect(nullptr)
    , m_pCircleTexture(nullptr)
    , m_pNorthTexture(nullptr)
    , m_pLineTexture(nullptr)
    , m_pRadarRingPlaneTexture(nullptr)
    , m_pDraw(nullptr)
    , m_pRenderTarget(nullptr)
    , m_pBlipRenderTarget(nullptr)
    , m_width(0)
    , m_height(0)
    , m_cachedPlayer(nullptr)
    , m_cachedIsInAircraft(false)
    , m_cachedIsInPlane(false)
    , m_cachedRollAngle(0.0f)
    , m_cachedPitchAngle(0.0f)
    , m_nearPlane(0.3f)
    , m_farPlane(10000.0f)
    , m_initialAircraftAltitude(0.0f)
    , m_bWasInAircraft(false)
    , m_bWasInInterior(false)
    , m_exitInteriorImageStartTime(0)
    , m_pExitInteriorTexture(nullptr)
    , m_bRadioNameVisible(false)
    , m_bShowGangZones(true)
    , m_bRadarShapeCircle(true)
    , m_bBorderShapeCircle(true)
    , m_bInitialized(false)
    , m_bSettingsPreview(false)
#ifdef _DEBUG
    , m_debugChunksRenderedLastFrame(0)
#endif
{
}

RadarRenderer::~RadarRenderer()
{
    Shutdown();
}

bool RadarRenderer::Initialize(LPDIRECT3DDEVICE9 pDevice)
{
    if (!pDevice)
        return false;

    m_pd3dDevice = pDevice;
    
    m_width = RsGlobal.maximumWidth;
    m_height = RsGlobal.maximumHeight;

    m_pShaderManager = new ShaderManager(m_pd3dDevice);
    if (!m_pShaderManager->Initialize())
    {
        return false;
    }

    // Initialize helper classes
    m_pCameraController = new CameraController();
    m_pMapChunkManager = new MapChunkManager(m_pd3dDevice);
    m_pMapChunkManager->Initialize();
    m_pBlipManager = new BlipManager(m_pd3dDevice);
    m_pBlipManager->Initialize();

    m_pGangZoneRenderer = new GangZoneRenderer(m_pd3dDevice);
    m_pGangZoneRenderer->SetEnabled(m_bShowGangZones);
    
    m_pGpsRenderer = new GpsRenderer(m_pd3dDevice);
    if (!m_pGpsRenderer->Initialize())
    {
        delete m_pGpsRenderer;
        m_pGpsRenderer = nullptr;
    }

    if (!InitializeResources())
    {
        return false;
    }

    m_pRenderTarget = new RenderTarget(m_pd3dDevice);
    m_pBlipRenderTarget = new RenderTarget(m_pd3dDevice);
    UpdateDrawResources();
    m_pDraw = new DxDrawPrimitives(m_pd3dDevice);
    if (!m_pDraw->Initialize(&m_drawResources))
    {
        delete m_pDraw;
        m_pDraw = nullptr;
        delete m_pRenderTarget;
        m_pRenderTarget = nullptr;
        delete m_pBlipRenderTarget;
        m_pBlipRenderTarget = nullptr;
        return false;
    }

    m_pRenderRadio = new RenderRadio(m_pDraw);
    if (m_pRadioFont)
    {
        m_pRenderRadio->SetFont(m_pRadioFont);
    }

    m_bInitialized = true;
    return true;
}

void RadarRenderer::Shutdown()
{
    ReleaseRadarStateBlock();

    if (m_pDraw)
    {
        m_pDraw->Shutdown();
        delete m_pDraw;
        m_pDraw = nullptr;
    }
    if (m_pRenderTarget)
    {
        delete m_pRenderTarget;
        m_pRenderTarget = nullptr;
    }
    if (m_pBlipRenderTarget)
    {
        delete m_pBlipRenderTarget;
        m_pBlipRenderTarget = nullptr;
    }

    CleanupResources();
    
    // Cleanup helper classes
    if (m_pGpsRenderer)
    {
        m_pGpsRenderer->Shutdown();
        delete m_pGpsRenderer;
        m_pGpsRenderer = nullptr;
    }
    if (m_pRenderRadio)
    {
        delete m_pRenderRadio;
        m_pRenderRadio = nullptr;
    }
    if (m_pBlipManager)
    {
        m_pBlipManager->Shutdown();
        delete m_pBlipManager;
        m_pBlipManager = nullptr;
    }
    if (m_pGangZoneRenderer)
    {
        delete m_pGangZoneRenderer;
        m_pGangZoneRenderer = nullptr;
    }
    
    if (m_pMapChunkManager)
    {
        m_pMapChunkManager->Cleanup();
        delete m_pMapChunkManager;
        m_pMapChunkManager = nullptr;
    }
    
    if (m_pCameraController)
    {
        delete m_pCameraController;
        m_pCameraController = nullptr;
    }
    
    // m_pNorthTexture from BlipManager (GetBlipTexture(4)), do not Release
    m_pNorthTexture = nullptr;
    
    if (m_pFont)
    {
        m_pFont->Release();
        m_pFont = nullptr;
    }
    if (m_pFontSprite)
    {
        m_pFontSprite->Release();
        m_pFontSprite = nullptr;
    }
    if (m_pRadioFont)
    {
        m_pRadioFont->Release();
        m_pRadioFont = nullptr;
    }
    if (m_pRadioFontSprite)
    {
        m_pRadioFontSprite->Release();
        m_pRadioFontSprite = nullptr;
    }

    if (m_pShaderManager)
    {
        m_pShaderManager->Shutdown();
        delete m_pShaderManager;
        m_pShaderManager = nullptr;
    }

    m_bInitialized = false;
}

void RadarRenderer::SetShowGangZones(bool value)
{
    m_bShowGangZones = value;
    if (m_pGangZoneRenderer)
        m_pGangZoneRenderer->SetEnabled(value);
}

bool RadarRenderer::InitializeResources()
{
    if (!m_pd3dDevice || !m_pShaderManager)
        return false;

    char shaderPath[MAX_PATH]{};
    auto loadFx = [&](const char* file, const char* cacheName, LPD3DXEFFECT& outEffect) -> bool {
        if (!ModPaths::BuildShaderPath(shaderPath, sizeof(shaderPath), file))
            return false;
        outEffect = m_pShaderManager->LoadEffectFromFile(shaderPath, cacheName);
        return outEffect != nullptr;
    };

    if (!loadFx("circle.fxo", "CircleShader", m_pTriangleEffect))
        return false;
    if (!loadFx("border.fxo", "BorderShader", m_pBorderEffect))
        return false;
    if (!loadFx("image3d.fxo", "Image3DShader", m_pImage3DEffect))
        return false;
    if (!loadFx("line.fxo", "LineShader", m_pLineEffect))
        return false;
    if (!loadFx("line_smooth.fxo", "LineSmoothShader", m_pLineSmoothEffect))
        return false;
    if (!loadFx("green_square.fxo", "GreenSquareShader", m_pGreenSquareEffect))
        return false;

    // Render target is created in Render() with radar size (sizeX x sizeY) for correct aspect ratio

    struct CircleVertex
    {
        float x, y, z;
        DWORD color;
        float u, v;
    };
    
    CircleVertex circle[] = {
        { -0.5f,  0.5f, 0.5f, D3DCOLOR_XRGB(255, 255, 255), 0.0f, 0.0f },
        { -0.5f, -0.5f, 0.5f, D3DCOLOR_XRGB(255, 255, 255), 0.0f, 1.0f },
        {  0.5f,  0.5f, 0.5f, D3DCOLOR_XRGB(255, 255, 255), 1.0f, 0.0f },
        { -0.5f, -0.5f, 0.5f, D3DCOLOR_XRGB(255, 255, 255), 0.0f, 1.0f },
        {  0.5f, -0.5f, 0.5f, D3DCOLOR_XRGB(255, 255, 255), 1.0f, 1.0f },
        {  0.5f,  0.5f, 0.5f, D3DCOLOR_XRGB(255, 255, 255), 1.0f, 0.0f }
    };

    if (FAILED(m_pd3dDevice->CreateVertexBuffer(
        6 * sizeof(CircleVertex),
        D3DUSAGE_WRITEONLY,
        D3DFVF_XYZ | D3DFVF_DIFFUSE | D3DFVF_TEX1,
        D3DPOOL_DEFAULT,
        &m_pTriangleVB,
        nullptr)))
    {
        return false;
    }

    void* pVertices = nullptr;
    if (FAILED(m_pTriangleVB->Lock(0, sizeof(circle), &pVertices, 0)))
    {
        return false;
    }
    memcpy(pVertices, circle, sizeof(circle));
    m_pTriangleVB->Unlock();

    // Fallback for dxDrawCircleShader when render target is null (map is from chunks in render target)
    if (SUCCEEDED(D3DXCreateTexture(m_pd3dDevice, 1, 1, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &m_pCircleTexture)))
    {
        D3DLOCKED_RECT locked;
        if (SUCCEEDED(m_pCircleTexture->LockRect(0, &locked, nullptr, 0)))
        {
            *(DWORD*)locked.pBits = 0xFFFFFFFF;
            m_pCircleTexture->UnlockRect(0);
        }
    }

    
    m_pNorthTexture = nullptr;

    // Line from blip.txd
    if (m_pBlipManager)
    {
        m_pLineTexture = m_pBlipManager->LoadAuxTextureFromTxd("line");
        if (!m_pLineTexture)
            m_pLineTexture = m_pBlipManager->LoadAuxTextureFromTxd("radarLine");
    }
    if (!m_pLineTexture)
    {
        const char* fallback = PLUGIN_PATH(Radar::Path::BlipLinePng);
        D3DXCreateTextureFromFileA(m_pd3dDevice, fallback, &m_pLineTexture);
    }

    // RingPlane from blip.txd
    if (m_pBlipManager)
    {
        m_pRadarRingPlaneTexture = m_pBlipManager->LoadAuxTextureFromTxd("radarRingPlane");
        if (!m_pRadarRingPlaneTexture)
            m_pRadarRingPlaneTexture = m_pBlipManager->LoadAuxTextureFromTxd("RingPlane");
    }
    if (!m_pRadarRingPlaneTexture)
    {
        const char* fallback = PLUGIN_PATH(Radar::Path::BlipRingPng);
        D3DXCreateTextureFromFileA(m_pd3dDevice, fallback, &m_pRadarRingPlaneTexture);
    }
    
    // Create DirectX font for text rendering
    D3DXFONT_DESC fontDesc;
    ZeroMemory(&fontDesc, sizeof(fontDesc));
    fontDesc.Height = 15;
    fontDesc.Width = 0;
    fontDesc.Weight = FW_NORMAL;
    fontDesc.MipLevels = 1;
    fontDesc.Italic = FALSE;
    fontDesc.CharSet = DEFAULT_CHARSET;
    fontDesc.OutputPrecision = OUT_DEFAULT_PRECIS;
    fontDesc.Quality = DEFAULT_QUALITY;
    fontDesc.PitchAndFamily = DEFAULT_PITCH | FF_DONTCARE;
    strcpy_s(fontDesc.FaceName, "Arial");
    
    if (FAILED(D3DXCreateFontIndirect(m_pd3dDevice, &fontDesc, &m_pFont)))
    {
        return false;
    }
    if (FAILED(D3DXCreateSprite(m_pd3dDevice, &m_pFontSprite)))
    {
        m_pFontSprite = nullptr;
    }

    // Radio HUD font — UTF-16 DrawTextW (LanguageManager strings are UTF-8)
    if (FAILED(D3DXCreateFontW(m_pd3dDevice, 32, 0, FW_BOLD, 1, FALSE, DEFAULT_CHARSET,
                               OUT_TT_PRECIS, ANTIALIASED_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
                               L"Arial", &m_pRadioFont)))
    {
        m_pRadioFont = nullptr;
    }
    if (FAILED(D3DXCreateSprite(m_pd3dDevice, &m_pRadioFontSprite)))
    {
        m_pRadioFontSprite = nullptr;
    }

    int exitImgW = 0, exitImgH = 0;
    m_pExitInteriorTexture = (LPDIRECT3DTEXTURE9)Base64Image::CreateTextureFromBase64(
        m_pd3dDevice, Base64Image::GetEmbeddedImageBase64(), exitImgW, exitImgH);
    if (!m_pExitInteriorTexture)
    {
            // Not critical - exit-from-interior effect simply won't show the image
    }
    Base64Image::ClearEmbeddedImageData();

    return true;
}

void RadarRenderer::ReleaseRadarStateBlock()
{
    if (m_pStateBlock)
    {
        m_pStateBlock->Release();
        m_pStateBlock = nullptr;
    }
}

bool RadarRenderer::BeginRadarZone()
{
    if (!m_pd3dDevice)
        return false;

    if (m_pStateBlock && FAILED(m_pStateBlock->Capture()))
        ReleaseRadarStateBlock();

    if (!m_pStateBlock)
    {
        if (FAILED(m_pd3dDevice->CreateStateBlock(D3DSBT_ALL, &m_pStateBlock)) || !m_pStateBlock)
            return false;
        m_pStateBlock->Capture();
    }

    // After Capture — safe HUD defaults (Apply restores game).
    m_pd3dDevice->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);
    m_pd3dDevice->SetRenderState(D3DRS_ZENABLE, FALSE);
    m_pd3dDevice->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
    m_pd3dDevice->SetRenderState(D3DRS_STENCILENABLE, FALSE);
    m_pd3dDevice->SetRenderState(D3DRS_COLORWRITEENABLE,
                                 D3DCOLORWRITEENABLE_RED | D3DCOLORWRITEENABLE_GREEN |
                                 D3DCOLORWRITEENABLE_BLUE | D3DCOLORWRITEENABLE_ALPHA);
    return true;
}

void RadarRenderer::EndRadarZone()
{
    if (!m_pd3dDevice)
        return;

    // Drop RT-local scissor / shaders before restoring the game stateblock.
    StockRadarDraw::SanitizeDrawState();

    m_pd3dDevice->SetVertexShader(nullptr);
    m_pd3dDevice->SetPixelShader(nullptr);

    if (m_pStateBlock)
        m_pStateBlock->Apply();

    // Apply can reintroduce stale RW shader cache entries — re-baseline HUD.
    StockRadarDraw::SanitizeDrawState();
}

void RadarRenderer::UpdateDrawResources()
{
    m_drawResources.pDevice             = m_pd3dDevice;
    m_drawResources.pTriangleEffect      = m_pTriangleEffect;
    m_drawResources.pBorderEffect        = m_pBorderEffect;
    m_drawResources.pImage3DEffect       = m_pImage3DEffect;
    m_drawResources.pLineEffect          = m_pLineEffect;
    m_drawResources.pLineSmoothEffect     = m_pLineSmoothEffect;
    m_drawResources.pGreenSquareEffect   = m_pGreenSquareEffect;
    m_drawResources.pCircleTexture       = m_pCircleTexture;
    m_drawResources.pNorthTexture        = m_pNorthTexture;
    m_drawResources.pLineTexture         = m_pLineTexture;
    m_drawResources.pRadarRingPlaneTexture = m_pRadarRingPlaneTexture;
    m_drawResources.pFont                = m_pFont;
    m_drawResources.pFontSprite          = m_pFontSprite;
    m_drawResources.screenWidth          = m_width;
    m_drawResources.screenHeight         = m_height;
    m_drawResources.radarShapeCircle     = m_bRadarShapeCircle;
    m_drawResources.borderShapeCircle    = m_bBorderShapeCircle;
    if (m_pRenderTarget)
    {
        m_drawResources.renderTargetWidth  = m_pRenderTarget->GetWidth();
        m_drawResources.renderTargetHeight = m_pRenderTarget->GetHeight();
    }
    else
    {
        m_drawResources.renderTargetWidth  = 0;
        m_drawResources.renderTargetHeight = 0;
    }
}

void RadarRenderer::CleanupResources()
{
    ReleaseRadarStateBlock();

    if (m_pCircleTexture)
    {
        m_pCircleTexture->Release();
        m_pCircleTexture = nullptr;
    }


    if (m_pLineTexture)
    {
        m_pLineTexture->Release();
        m_pLineTexture = nullptr;
    }

    if (m_pRadarRingPlaneTexture)
    {
        m_pRadarRingPlaneTexture->Release();
        m_pRadarRingPlaneTexture = nullptr;
    }

    if (m_pExitInteriorTexture)
    {
        m_pExitInteriorTexture->Release();
        m_pExitInteriorTexture = nullptr;
    }

    if (m_pTriangleVB)
    {
        m_pTriangleVB->Release();
        m_pTriangleVB = nullptr;
    }

    m_pTriangleEffect = nullptr;
    m_pBorderEffect = nullptr;
    m_pImage3DEffect = nullptr;
    m_pLineEffect = nullptr;
    m_pLineSmoothEffect = nullptr;
    m_pGreenSquareEffect = nullptr;
}


void RadarRenderer::CalculateRadarPosition(float& circleX, float& circleY, float& sizeX, float& sizeY)
{
    float baseCircle = (float)RadarConfig::GetCircleSize();
    float baseSqX = (float)RadarConfig::GetSquareSizeX();
    float baseSqY = (float)RadarConfig::GetSquareSizeY();
    float baseOffsetX = (float)RadarConfig::GetOffsetX();
    float baseOffsetY = (float)RadarConfig::GetOffsetY();
#ifdef _DEBUG
    // Как в trilogy: offset 85 + 2 размера радара (в базовых 1920p единицах)
    float baseRadarSize = m_bRadarShapeCircle ? baseCircle : baseSqX;
    baseOffsetX = 85.0f + baseRadarSize;
#endif
    MathUtils::CalculateRadarPosition(circleX, circleY, sizeX, sizeY, baseCircle, baseSqX, baseSqY, m_bRadarShapeCircle, baseOffsetX, baseOffsetY);
}

float RadarRenderer::CalculateBlipSize(float baseBlipSize, float baseWidth) const
{
    float screenWidth = (float)RsGlobal.maximumWidth;
    float scaleX = screenWidth / baseWidth;
    return baseBlipSize * scaleX;
}

void RadarRenderer::RenderRadarTargetContents(float circleX, float circleY, float sizeX, float sizeY)
{
    (void)circleX; (void)circleY; (void)sizeX; (void)sizeY;
    if (m_pImage3DEffect && m_pCameraController && m_pMapChunkManager)
    {
        float offsetWorldX, offsetWorldY;
        D3DXVECTOR3 cameraPos, cameraRot;
        m_pCameraController->GetCachedCalculations(offsetWorldX, offsetWorldY, cameraPos, cameraRot);
        const CameraController::CameraState& camState = m_pCameraController->GetState();
        const bool flat2D = !RadarConfig::GetRadar3D();
        float visibleRadius = MapChunkManager::ComputeVisibleRadius(
            cameraPos.z, camState.fov,
            flat2D ? 0.0f : camState.offsetY,
            flat2D ? (-D3DX_PI * 0.5f) : camState.pitch,
            m_cachedIsInAircraft);
        if (!m_bRadarShapeCircle)
            visibleRadius *= 1.65f;  // square corners extend beyond circle; extra margin for square radar

        // Radius around player/icon; frustum from offset camera (pulled back by offsetY).
        const D3DXVECTOR3 focusPos(camState.posX, camState.posY, 0.0f);

        MapChunkManager::FrustumParams frustum = {};
        frustum.cameraPos = &cameraPos;
        frustum.cameraRot = &cameraRot;
        frustum.fov = camState.fov;
        frustum.nearPlane = m_nearPlane;
        frustum.farPlane = m_farPlane;
        frustum.screenWidth = m_pRenderTarget ? (float)m_pRenderTarget->GetWidth() : 0.0f;
        frustum.screenHeight = m_pRenderTarget ? (float)m_pRenderTarget->GetHeight() : 0.0f;
        frustum.projectionAspect = MathUtils::GetRadarProjectionAspect();
        frustum.cameraCullRadius = visibleRadius + std::fabs(flat2D ? 0.0f : camState.offsetY) * 1.5f;

        m_pMapChunkManager->ForEachChunkInRadius(focusPos, visibleRadius, &frustum, [this, &cameraPos, &cameraRot, &camState](int, const D3DXVECTOR3& elementPos, const D3DXVECTOR3& elementRot, const D3DXVECTOR2& elementSize, LPDIRECT3DTEXTURE9 chunkTex)
        {
            m_pDraw->dxDrawImage3D(elementPos, elementRot, elementSize, cameraPos, cameraRot, camState.fov, m_nearPlane, m_farPlane, chunkTex, tocolor(255, 255, 255, 255));
#ifdef _DEBUG
            ++m_debugChunksRenderedLastFrame;
#endif
        });

    }
    if (!m_bSettingsPreview && m_pGangZoneRenderer && m_pCameraController)
    {
        float offsetWorldX, offsetWorldY;
        D3DXVECTOR3 cameraPos, cameraRot;
        m_pCameraController->GetCachedCalculations(offsetWorldX, offsetWorldY, cameraPos, cameraRot);
        const CameraController::CameraState& camState = m_pCameraController->GetState();
        m_pGangZoneRenderer->Render(cameraPos, cameraRot, camState.fov, m_nearPlane, m_farPlane,
            camState.posX, camState.posY, m_pCircleTexture ? m_pCircleTexture : m_pLineTexture,
            [this](const D3DXVECTOR3& pos, const D3DXVECTOR3& rot, const D3DXVECTOR2& size,
                const D3DXVECTOR3& camPos, const D3DXVECTOR3& camRot,
                float fov, float nearPlane, float farPlane,
                LPDIRECT3DTEXTURE9 tex, DWORD color) {
            m_pDraw->dxDrawImage3D(pos, rot, size, camPos, camRot, fov, nearPlane, farPlane, tex, color);
        });
    }
    // GPS line: render to RT BEFORE stock blips overlay
    if (!m_bSettingsPreview && m_pGpsRenderer && m_pCameraController && m_pRenderTarget)
    {
        float rtWidth = (float)m_pRenderTarget->GetWidth();
        float rtHeight = (float)m_pRenderTarget->GetHeight();
        if (rtWidth > 0 && rtHeight > 0)
        {
            float centerX = rtWidth * 0.5f;
            float centerY = rtHeight * 0.5f;
            float halfX, halfY;
            RadarGeometry::GetRadarHalfExtents(rtWidth, rtHeight, MathUtils::ScaleRadarLength((float)RadarConfig::GetBorderThickness()), m_bRadarShapeCircle, halfX, halfY);
            float offsetWorldX, offsetWorldY;
            D3DXVECTOR3 cameraPos, cameraRot;
            m_pCameraController->GetCachedCalculations(offsetWorldX, offsetWorldY, cameraPos, cameraRot);
            const CameraController::CameraState& camState = m_pCameraController->GetState();
            float projectionAspect = MathUtils::GetRadarProjectionAspect();
            m_pGpsRenderer->Render(m_pDraw, centerX, centerY, rtWidth, rtHeight,
                cameraPos, cameraRot, camState.fov, m_nearPlane, m_farPlane,
                rtWidth, rtHeight, projectionAspect,
                m_bRadarShapeCircle, halfX, halfY);
        }
    }

  // GPS Redux / CopNThreat: bake into the radar RT. Use the same 3D camera as
    // the map and project overlays at RADAR_ROUTE_Z (0.02) so the line sits on
    // the tilted disc instead of floating above it via flat mapping.
    if (!m_bSettingsPreview && m_pRenderTarget && m_pRenderTarget->GetWidth() > 0 && m_pRenderTarget->GetHeight() > 0)
    {
        const float rtW = (float)m_pRenderTarget->GetWidth();
        const float rtH = (float)m_pRenderTarget->GetHeight();

        StockRadarPlane plane{};
        plane.cx = rtW * 0.5f;
        plane.cy = rtH * 0.5f;
        plane.half = rtW * 0.5f;
        plane.sizeX = rtW;
        plane.sizeY = rtH;
        plane.clipL = 0.0f;
        plane.clipT = 0.0f;
        plane.clipR = rtW;
        plane.clipB = rtH;
        plane.shapeCircle = m_bRadarShapeCircle;
        RadarGeometry::GetRadarHalfExtents(rtW, rtH, MathUtils::ScaleRadarLength((float)RadarConfig::GetBorderThickness()),
                                           m_bRadarShapeCircle, plane.halfX, plane.halfY);

        if (m_pCameraController)
        {
            float offsetWorldX = 0.0f;
            float offsetWorldY = 0.0f;
            m_pCameraController->GetCachedCalculations(offsetWorldX, offsetWorldY, plane.cameraPos, plane.cameraRot);
            const CameraController::CameraState& camState = m_pCameraController->GetState();
            plane.fov = camState.fov;
            plane.nearPlane = m_nearPlane;
            plane.farPlane = m_farPlane;
            plane.rtWidth = rtW;
            plane.rtHeight = rtH;
            plane.projectionAspect = MathUtils::GetRadarProjectionAspect();
            plane.playerRadarX = camState.posX;
            plane.playerRadarY = camState.posY;
            plane.yaw = camState.yaw;
            plane.use3D = true;
        }

        StockRadarDraw::EnsureHooksInstalled();
        StockRadarDraw::SetPlane(plane);
        StockRadarDraw::Begin();
        StockRadarDraw::DrawHudOverlaysOnly();
        StockRadarDraw::End();
    }
}

LPDIRECT3DTEXTURE9 RadarRenderer::GetRenderTargetTexture() const
{
    return m_pRenderTarget ? m_pRenderTarget->GetRenderTargetTexture() : nullptr;
}

LPDIRECT3DTEXTURE9 RadarRenderer::GetBlipRenderTargetTexture() const
{
    return m_pBlipRenderTarget ? m_pBlipRenderTarget->GetRenderTargetTexture() : nullptr;
}

static bool UsesCircularBlipRenderTarget()
{
    return RadarConfig::GetRadar3D() && GameState::ShouldDrawRadarMap();
}

void RadarRenderer::RenderBlipRenderTarget(float circleX, float circleY, float sizeX, float sizeY, bool previewPedOnly)
{
    (void)circleX; (void)circleY;
    if (!previewPedOnly && !UsesCircularBlipRenderTarget())
        return;
    if (!m_pBlipRenderTarget || !m_pBlipManager || !m_pDraw || !m_pCameraController)
        return;

    float baseSize = (sizeX < sizeY) ? sizeX : sizeY;
    int rtSize = (int)(baseSize + 0.5f);
    if (rtSize < 1)
        rtSize = 1;
    if (m_pBlipRenderTarget->GetWidth() != rtSize || m_pBlipRenderTarget->GetHeight() != rtSize)
        m_pBlipRenderTarget->dxCreateRenderTarget(rtSize, rtSize);

    const float rtW = (float)rtSize;
    StockRadarPlane plane{};
    plane.cx = rtW * 0.5f;
    plane.cy = rtW * 0.5f;
    plane.half = rtW * 0.5f;
    plane.sizeX = rtW;
    plane.sizeY = rtW;
    plane.shapeCircle = m_bRadarShapeCircle;
    RadarGeometry::GetRadarHalfExtents(rtW, rtW, MathUtils::ScaleRadarLength((float)RadarConfig::GetBorderThickness()),
                                       m_bRadarShapeCircle, plane.halfX, plane.halfY);

    float offsetWorldX = 0.0f;
    float offsetWorldY = 0.0f;
    D3DXVECTOR3 cameraPos;
    D3DXVECTOR3 cameraRot;
    m_pCameraController->GetCachedCalculations(offsetWorldX, offsetWorldY, cameraPos, cameraRot);
    const CameraController::CameraState& camState = m_pCameraController->GetState();
    plane.cameraPos = cameraPos;
    plane.cameraRot = cameraRot;
    plane.fov = camState.fov;
    plane.nearPlane = m_nearPlane;
    plane.farPlane = m_farPlane;
    plane.rtWidth = rtW;
    plane.rtHeight = rtW;
    plane.projectionAspect = MathUtils::GetRadarProjectionAspect();
    plane.playerRadarX = camState.posX;
    plane.playerRadarY = camState.posY;
    plane.yaw = camState.yaw;
    plane.use3D = true;

    if (!m_pBlipRenderTarget->GetSurface())
        return;

    if (m_pBlipRenderTarget->dxSetRenderTarget(m_pBlipRenderTarget->GetSurface()))
    {
        if (m_pd3dDevice)
            m_pd3dDevice->Clear(0, nullptr, D3DCLEAR_TARGET, 0x00000000, 1.0f, 0);

        m_pBlipManager->DrawBlipsToRenderTarget(plane, m_pDraw, sizeX, 0.0f, previewPedOnly);
        m_pBlipRenderTarget->dxSetRenderTarget(nullptr);
    }
}

void RadarRenderer::RenderRadarOverlays(float circleX, float circleY, float sizeX, float sizeY)
{
    const bool drawRim = m_bSettingsPreview || GameState::ShouldDrawRadarRim();
    LPDIRECT3DTEXTURE9 rtTex = GetRenderTargetTexture();
    LPDIRECT3DTEXTURE9 textureToUse = rtTex ? rtTex : m_pCircleTexture;
    float scale = RadarGeometry::GetRadarScale();

    if (drawRim)
    {
        // Stock: sea fill only in DrawRadarSection (outdoors). Interior = radardisc + blips, no underlay.
        if (m_bSettingsPreview || GameState::ShouldDrawRadarMap())
        {
            DWORD oldAlphaBlend = 0, oldAlphaTest = 0;
            m_pd3dDevice->GetRenderState(D3DRS_ALPHABLENDENABLE, &oldAlphaBlend);
            m_pd3dDevice->GetRenderState(D3DRS_ALPHATESTENABLE, &oldAlphaTest);
            m_pd3dDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
            m_pd3dDevice->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);

            int circleR, circleG, circleB, circleA;
            RadarConfig::GetCircleColor(circleR, circleG, circleB, circleA);
            m_pDraw->dxDrawCircleShader(circleX, circleY, sizeX, sizeY, textureToUse, tocolor(circleR, circleG, circleB, circleA));
            m_pd3dDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, oldAlphaBlend);
            m_pd3dDevice->SetRenderState(D3DRS_ALPHATESTENABLE, oldAlphaTest);
        }

        if (m_cachedIsInPlane && m_bRadarShapeCircle)
        {
            static LPDIRECT3DTEXTURE9 whiteTexture = nullptr;
            if (!whiteTexture && m_pd3dDevice)
            {
                if (SUCCEEDED(D3DXCreateTexture(m_pd3dDevice, 1, 1, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &whiteTexture)))
                {
                    D3DLOCKED_RECT lockedRect;
                    if (SUCCEEDED(whiteTexture->LockRect(0, &lockedRect, nullptr, 0)))
                    {
                        DWORD* pData = (DWORD*)lockedRect.pBits;
                        *pData = 0xFFFFFFFF;
                        whiteTexture->UnlockRect(0);
                    }
                }
            }
            if (m_pGreenSquareEffect && whiteTexture)
            {
                float pitchAngle = m_cachedPitchAngle;
                float fillLevel = (pitchAngle + D3DX_PI * 0.5f) / D3DX_PI;
                if (fillLevel < 0.0f) fillLevel = 0.0f;
                if (fillLevel > 1.0f) fillLevel = 1.0f;
                m_pDraw->dxDrawGreenSquareFill(circleX, circleY, sizeX, sizeY, whiteTexture, tocolor(0, 255, 0, 155), fillLevel);
            }
            if (m_pRadarRingPlaneTexture)
            {
                float rollAngle = m_cachedRollAngle;
                m_pDraw->dxDrawImage2DRotated(circleX, circleY, sizeX, sizeY, m_pRadarRingPlaneTexture, rollAngle, tocolor(255, 255, 255, 225));
            }
        }

        if (m_pExitInteriorTexture && m_exitInteriorImageStartTime != 0)
        {
            unsigned int now = CTimer::m_snTimeInMilliseconds;
            unsigned int elapsed = now - m_exitInteriorImageStartTime;
            if (elapsed >= 10000)
            {
                m_exitInteriorImageStartTime = 0;
                m_pExitInteriorTexture->Release();
                m_pExitInteriorTexture = nullptr;
            }
            else
            {
                float alpha = 1.0f - (elapsed / 10000.0f);
                DWORD oldAb = 0, oldSb = 0, oldDb = 0;
                m_pd3dDevice->GetRenderState(D3DRS_ALPHABLENDENABLE, &oldAb);
                m_pd3dDevice->GetRenderState(D3DRS_SRCBLEND, &oldSb);
                m_pd3dDevice->GetRenderState(D3DRS_DESTBLEND, &oldDb);
                m_pd3dDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
                m_pd3dDevice->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
                m_pd3dDevice->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);

                int circleR, circleG, circleB, circleA;
                RadarConfig::GetCircleColor(circleR, circleG, circleB, circleA);
                DWORD circleColor = tocolor(circleR, circleG, circleB, circleA);
                BYTE alphaByte = (BYTE)(alpha * 255.0f);
                circleColor = (circleColor & 0x00FFFFFF) | (alphaByte << 24);

                m_pDraw->dxDrawCircleShader(circleX, circleY, sizeX, sizeY, m_pExitInteriorTexture, circleColor);
                m_pd3dDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, oldAb);
                m_pd3dDevice->SetRenderState(D3DRS_SRCBLEND, oldSb);
                m_pd3dDevice->SetRenderState(D3DRS_DESTBLEND, oldDb);
            }
        }

        // Disc blips under border (The-Definitive-UI: border covers rim; orbit sprites stay after EndZone).
        if (m_bSettingsPreview || GameState::ShouldDrawRadarMap())
        {
            LPDIRECT3DTEXTURE9 blipRt = GetBlipRenderTargetTexture();
            if (blipRt && (UsesCircularBlipRenderTarget() || m_bSettingsPreview))
            {
                DWORD blAb = 0, blSb = 0, blDb = 0;
                m_pd3dDevice->GetRenderState(D3DRS_ALPHABLENDENABLE, &blAb);
                m_pd3dDevice->GetRenderState(D3DRS_SRCBLEND, &blSb);
                m_pd3dDevice->GetRenderState(D3DRS_DESTBLEND, &blDb);
                m_pd3dDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
                m_pd3dDevice->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
                m_pd3dDevice->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
                m_pDraw->dxDrawCircleShader(circleX, circleY, sizeX, sizeY, blipRt, tocolor(255, 255, 255, 255));
                m_pd3dDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, blAb);
                m_pd3dDevice->SetRenderState(D3DRS_SRCBLEND, blSb);
                m_pd3dDevice->SetRenderState(D3DRS_DESTBLEND, blDb);
            }
        }

        // Border last among disc layers — covers map + Blip RT edge.
        float borderThicknessPx = MathUtils::ScaleRadarLength((float)RadarConfig::GetBorderThickness());
        int borderR, borderG, borderB, borderA;
        RadarConfig::GetBorderColor(borderR, borderG, borderB, borderA);
        DWORD borderColor = tocolor(borderR, borderG, borderB, borderA);

        if (m_bBorderShapeCircle)
            m_pDraw->dxDrawBorderShader(circleX, circleY, sizeX, sizeY, borderColor, 1.0f, borderThicknessPx);
        else
        {
            float margin = borderThicknessPx;
            float borderSizeX = sizeX + 2.0f * margin;
            float borderSizeY = sizeY + 2.0f * margin;
            m_pDraw->dxDrawBorderShader(circleX - margin, circleY - margin, borderSizeX, borderSizeY, borderColor, 1.0f, margin);
        }
    }

    // The-Definitive-UI: missiles / altitude / radio after border, still in overlay pass.
    bool bInAircraft = m_cachedIsInAircraft;
    if (bInAircraft && !m_bWasInAircraft)
    {
        try {
            if (m_cachedPlayer && m_cachedPlayer->bInVehicle && m_cachedPlayer->m_pVehicle)
            {
                CVector pos = m_cachedPlayer->GetPosition();
                m_initialAircraftAltitude = pos.z;
            }
        }
        catch (...) { m_initialAircraftAltitude = 0.0f; }
    }
    m_bWasInAircraft = bInAircraft;

    if (!m_bSettingsPreview)
        RenderMissiles(circleX, circleY, sizeX, sizeY);

    if (!m_bSettingsPreview && drawRim && bInAircraft)
    {
        float stripX = circleX + sizeX, stripY = circleY;
        float stripWidth = 20.0f * scale, stripHeight = sizeY;
        float currentAltitude = 0.0f;
        try {
            if (m_cachedPlayer && m_cachedPlayer->bInVehicle && m_cachedPlayer->m_pVehicle)
                currentAltitude = m_cachedPlayer->GetPosition().z - m_initialAircraftAltitude;
        }
        catch (...) {}
        if (currentAltitude < 0.0f) currentAltitude = 0.0f;
        if (currentAltitude > 250.0f) currentAltitude = 250.0f;
        float progressBarY = stripY + sizeY - (sizeY * currentAltitude / 250.0f);
        m_pDraw->dxDrawRectangle(stripX + stripWidth, stripY, stripWidth, stripHeight, tocolor(0, 0, 0, 100));
        m_pDraw->dxDrawRectangle(stripX + stripWidth * 0.85f, progressBarY, stripWidth * 1.3f, stripWidth * 0.2f, tocolor(255, 255, 255, 225));
    }

    if (!m_bSettingsPreview)
        RenderRadioText();

#ifdef _DEBUG
    // Overlay (font outlines) is optional — hold F3. Was a steady Debug FPS tax.
    if (GetAsyncKeyState(VK_F3) & 0x8000)
        dxDrawRenderDebugMemory();
#endif
}

void RadarRenderer::RenderRadarSprites(float circleX, float circleY, float sizeX, float sizeY)
{
    RwRaster* savedRwRaster = nullptr;
    RwRenderStateGet(rwRENDERSTATETEXTURERASTER, &savedRwRaster);
    StockRadarDraw::SyncRwSpritePipeline();
    BlipManager::PushDeIcons();

    if (m_pCameraController && CRadar::RadarBlipSprites && !m_bSettingsPreview)
    {
        float northMarkerSize = CalculateBlipSize(28.0f);
        float centerX = circleX + sizeX * 0.5f, centerY = circleY + sizeY * 0.5f;
        float halfX, halfY;
        RadarGeometry::GetRadarHalfExtents(sizeX, sizeY, MathUtils::ScaleRadarLength((float)RadarConfig::GetBorderThickness()), m_bRadarShapeCircle, halfX, halfY);
        const CameraController::CameraState& camState = m_pCameraController->GetState();
        float northAngle = -camState.yaw - D3DX_PI * 0.5f;
        float northCenterX, northCenterY;
        RadarGeometry::PointOnOrbitEdge(centerX, centerY, halfX, halfY, cosf(northAngle), sinf(northAngle), !m_bRadarShapeCircle, northCenterX, northCenterY);
        const float half = northMarkerSize * 0.5f;
        CRadar::RadarBlipSprites[RADAR_SPRITE_NORTH].Draw(
            CRect(northCenterX - half, northCenterY - half, northCenterX + half, northCenterY + half),
            CRGBA(255, 255, 255, 255));
    }

    if (m_pBlipManager && !m_bSettingsPreview)
    {
        StockRadarPlane plane;
        plane.cx = circleX + sizeX * 0.5f;
        plane.cy = circleY + sizeY * 0.5f;
        plane.half = sizeX * 0.5f;
        plane.clipL = circleX;
        plane.clipT = circleY;
        plane.clipR = circleX + sizeX;
        plane.clipB = circleY + sizeY;
        plane.sizeX = sizeX;
        plane.sizeY = sizeY;
        plane.shapeCircle = m_bRadarShapeCircle;
        RadarGeometry::GetRadarHalfExtents(sizeX, sizeY, MathUtils::ScaleRadarLength((float)RadarConfig::GetBorderThickness()),
                                           m_bRadarShapeCircle, plane.halfX, plane.halfY);

        if (m_pCameraController && m_pRenderTarget)
        {
            float offsetWorldX = 0.0f;
            float offsetWorldY = 0.0f;
            m_pCameraController->GetCachedCalculations(offsetWorldX, offsetWorldY, plane.cameraPos, plane.cameraRot);
            const CameraController::CameraState& camState = m_pCameraController->GetState();
            plane.fov = camState.fov;
            plane.nearPlane = m_nearPlane;
            plane.farPlane = m_farPlane;
            plane.rtWidth = (float)m_pRenderTarget->GetWidth();
            plane.rtHeight = (float)m_pRenderTarget->GetHeight();
            plane.projectionAspect = MathUtils::GetRadarProjectionAspect();
            plane.playerRadarX = camState.posX;
            plane.playerRadarY = camState.posY;
            plane.yaw = camState.yaw;
            plane.use3D = (plane.rtWidth >= 1.0f && plane.rtHeight >= 1.0f);
        }

        m_pBlipManager->DrawStockOverlay(plane, m_bShowGangZones && GameState::ShouldDrawRadarMap());
    }

    if (!m_bSettingsPreview)
        RenderAirstrips(circleX, circleY, sizeX, sizeY);
    RwRenderStateSet(rwRENDERSTATETEXTURERASTER, savedRwRaster);
    BlipManager::PopDeIcons();
}

void RadarRenderer::RenderAirstrips(float circleX, float circleY, float sizeX, float sizeY)
{
    if (!m_cachedIsInPlane || !m_pCameraController || !CRadar::RadarBlipSprites)
        return;

    const float centerX = circleX + sizeX * 0.5f;
    const float centerY = circleY + sizeY * 0.5f;
    float halfX = 0.0f, halfY = 0.0f;
    RadarGeometry::GetRadarHalfExtents(sizeX, sizeY, MathUtils::ScaleRadarLength((float)RadarConfig::GetBorderThickness()),
                                       m_bRadarShapeCircle, halfX, halfY);
    const float iconSize = CalculateBlipSize(24.0f);

    float offsetWorldX = 0.0f, offsetWorldY = 0.0f;
    D3DXVECTOR3 cameraPos, cameraRot;
    m_pCameraController->GetCachedCalculations(offsetWorldX, offsetWorldY, cameraPos, cameraRot);
    const CameraController::CameraState& camState = m_pCameraController->GetState();
    const float rtWidth = m_pRenderTarget ? (float)m_pRenderTarget->GetWidth() : 0.0f;
    const float rtHeight = m_pRenderTarget ? (float)m_pRenderTarget->GetHeight() : 0.0f;
    if (rtWidth < 1.0f || rtHeight < 1.0f)
        return;
    const float screenAspect = MathUtils::GetRadarProjectionAspect();

    float playerX = 0.0f, playerY = 0.0f;
    if (m_cachedPlayer)
    {
        const CVector ppos = m_cachedPlayer->GetPosition();
        playerX = ppos.x;
        playerY = ppos.y;
    }

    const int airstripIdx = NearestAirstrip(playerX, playerY);
    const AirstripInfo& strip = kAirstrips[airstripIdx];
    const float halfLen = strip.radius * 0.5f;
    const float dirRad = strip.direction * (3.14159265f / 180.0f);
    const float cosD = cosf(dirRad);
    const float sinD = sinf(dirRad);
    const bool playerOnRunway = IsPlayerOnRunwaySegment(
        strip.posX + halfLen * cosD, strip.posY + halfLen * sinD,
        strip.posX - halfLen * cosD, strip.posY - halfLen * sinD,
        playerX, playerY, strip.radius * 0.25f);

    float airstripCenterCircleX = 0.0f, airstripCenterCircleY = 0.0f;
    const bool airstripCenterVisible = WorldToCircleOffset(
        strip.posX, strip.posY, cameraPos, cameraRot, camState.fov, m_nearPlane, m_farPlane,
        rtWidth, rtHeight, sizeX, sizeY, airstripCenterCircleX, airstripCenterCircleY, screenAspect);
    const bool showLight = airstripCenterVisible || playerOnRunway;
    const bool isOnOrbit = !showLight;
    const bool useSquareOrbit = !m_bRadarShapeCircle;

    D3DXVECTOR3 blipWorldPos;
    RadarGeometry::WorldToRadarPos(strip.posX, strip.posY, blipWorldPos);
    float screenX = 0.0f, screenY = 0.0f;
    const bool visible = MathUtils::WorldToScreen(
        blipWorldPos, cameraPos, cameraRot, camState.fov, m_nearPlane, m_farPlane,
        rtWidth, rtHeight, screenX, screenY, screenAspect);

    float circleScreenX = 0.0f, circleScreenY = 0.0f;
    float dx = 0.0f, dy = 0.0f;
    if (visible)
    {
        circleScreenX = centerX + (screenX / rtWidth - 0.5f) * sizeX;
        circleScreenY = centerY + (screenY / rtHeight - 0.5f) * sizeY;
        dx = circleScreenX - centerX;
        dy = circleScreenY - centerY;
    }
    else
    {
        D3DXVECTOR3 playerPos(camState.posX, camState.posY, 0.0f);
        D3DXVECTOR3 airstripPos2D;
        RadarGeometry::WorldToRadarPos(strip.posX, strip.posY, airstripPos2D);
        float angle = 0.0f;
        if (!MathUtils::DirectionToOrbitAngle(playerPos, airstripPos2D, camState.yaw, angle))
            return;
        dx = cosf(angle);
        dy = sinf(angle);
        RadarGeometry::PointOnOrbitEdge(centerX, centerY, halfX, halfY, dx, dy, useSquareOrbit,
                                        circleScreenX, circleScreenY);
    }

    const float rotation = -camState.yaw - dirRad - (3.14159265f * 0.5f);

    auto drawRotated = [&](int spriteId, float x, float y, float angle) {
        if (spriteId < 0 || !CRadar::RadarBlipSprites)
            return;
        unsigned half = static_cast<unsigned>(iconSize * 0.5f);
        if (half < 1)
            half = 1;
        CRadar::DrawRotatingRadarSprite(
            &CRadar::RadarBlipSprites[spriteId],
            x, y, angle, half, half, CRGBA(255, 255, 255, 255));
    };

    if (isOnOrbit)
    {
        if (!visible)
        {
            const float orbitInset = iconSize * 0.5f;
            const float insetHalfX = (halfX > orbitInset) ? (halfX - orbitInset) : halfX;
            const float insetHalfY = (halfY > orbitInset) ? (halfY - orbitInset) : halfY;
            RadarGeometry::PointOnOrbitEdge(centerX, centerY, insetHalfX, insetHalfY,
                                            cosf(atan2f(dy, dx)), sinf(atan2f(dy, dx)),
                                            useSquareOrbit, circleScreenX, circleScreenY);
        }
        drawRotated(RADAR_SPRITE_RUNWAY, circleScreenX, circleScreenY, rotation);
        return;
    }

    const float rangeRadius = (halfX < halfY) ? halfX : halfY;
    float minOffset = 0.0f, maxOffset = 0.0f;
    const bool hasRange = GetAirstripOffsetRangeInsideCircle(
        strip.posX, strip.posY, dirRad, halfLen, centerX, centerY, rangeRadius,
        cameraPos, cameraRot, camState.fov, m_nearPlane, m_farPlane,
        rtWidth, rtHeight, sizeX, sizeY, minOffset, maxOffset, screenAspect);

    float offset = 0.0f;
    if (hasRange && maxOffset > minOffset)
    {
        const float range = maxOffset - minOffset;
        const float animLen = range * 0.75f;
        const float centerOffset = (minOffset + maxOffset) * 0.5f;
        const float t = static_cast<float>(CTimer::m_snTimeInMilliseconds) / 350.0f;
        const float tSmooth = 0.5f + 0.5f * sinf(t * 6.283185307f);
        offset = (centerOffset - animLen * 0.5f) + animLen * tSmooth;
    }

    float animCircleX = 0.0f, animCircleY = 0.0f;
    const bool lightPosOk = WorldToCircleOffset(
        strip.posX + offset * cosD, strip.posY + offset * sinD,
        cameraPos, cameraRot, camState.fov, m_nearPlane, m_farPlane,
        rtWidth, rtHeight, sizeX, sizeY, animCircleX, animCircleY, screenAspect);

    if (airstripCenterVisible && lightPosOk)
    {
        circleScreenX = centerX + animCircleX;
        circleScreenY = centerY + animCircleY;
    }
    else if (airstripCenterVisible)
    {
        circleScreenX = centerX + airstripCenterCircleX;
        circleScreenY = centerY + airstripCenterCircleY;
    }
    else
    {
        const float orbitAngle = atan2f(dy, dx);
        float innerRadius = rangeRadius - iconSize;
        if (innerRadius < 10.0f)
            innerRadius = 10.0f;
        circleScreenX = centerX + cosf(orbitAngle) * innerRadius;
        circleScreenY = centerY + sinf(orbitAngle) * innerRadius;
    }

    const float iconMargin = iconSize * 0.6f;
    const float innerHalfX = (halfX > iconMargin) ? (halfX - iconMargin) : halfX;
    const float innerHalfY = (halfY > iconMargin) ? (halfY - iconMargin) : halfY;
    const bool neededClamp = !RadarGeometry::IsInsideOrbit(
        circleScreenX, circleScreenY, centerX, centerY, innerHalfX, innerHalfY, useSquareOrbit);
    const bool trajectoryVisible = hasRange && (maxOffset > minOffset);
    const bool switchToRunway = neededClamp && !trajectoryVisible;
    if (neededClamp)
    {
        if (switchToRunway)
        {
            circleScreenX = centerX + airstripCenterCircleX;
            circleScreenY = centerY + airstripCenterCircleY;
        }
        RadarGeometry::ClampToOrbit(circleScreenX, circleScreenY, centerX, centerY,
                                    innerHalfX, innerHalfY, circleScreenX, circleScreenY, useSquareOrbit);
    }

    drawRotated(switchToRunway ? RADAR_SPRITE_RUNWAY : RADAR_SPRITE_LIGHT,
                circleScreenX, circleScreenY, neededClamp ? rotation : 0.0f);
}

void RadarRenderer::RenderMissiles(float circleX, float circleY, float sizeX, float sizeY)
{
    if (!m_pDraw || !m_pCameraController || !CProjectileInfo::ms_apProjectile || !gaProjectileInfo)
        return;

    const float centerX = circleX + sizeX * 0.5f;
    const float centerY = circleY + sizeY * 0.5f;
    float halfX = 0.0f, halfY = 0.0f;
    RadarGeometry::GetRadarHalfExtents(sizeX, sizeY, MathUtils::ScaleRadarLength((float)RadarConfig::GetBorderThickness()),
                                       m_bRadarShapeCircle, halfX, halfY);
    const bool useSquareOrbit = !m_bRadarShapeCircle;
    const float indicatorSize = CalculateBlipSize(12.0f);

    float offsetWorldX = 0.0f, offsetWorldY = 0.0f;
    D3DXVECTOR3 cameraPos, cameraRot;
    m_pCameraController->GetCachedCalculations(offsetWorldX, offsetWorldY, cameraPos, cameraRot);
    const CameraController::CameraState& camState = m_pCameraController->GetState();
    const float rtWidth = m_pRenderTarget ? (float)m_pRenderTarget->GetWidth() : 0.0f;
    const float rtHeight = m_pRenderTarget ? (float)m_pRenderTarget->GetHeight() : 0.0f;
    if (rtWidth < 1.0f || rtHeight < 1.0f)
        return;
    const float screenAspect = MathUtils::GetRadarProjectionAspect();

    CPlayerPed* playerPed = FindPlayerPed();
    CVehicle* playerVeh = FindPlayerVehicle();
    const CVector playerPosR = FindPlayerCoors();
    const DWORD missileColor = tocolor(255, 0, 0, 255);

    for (unsigned int i = 0; i < MAX_PROJECTILE_INFOS; ++i)
    {
        CProjectileInfo& info = gaProjectileInfo[i];
        if (!info.m_bActive || !CProjectileInfo::ms_apProjectile[i])
            continue;
        if (info.m_pCreator == playerPed || info.m_pCreator == playerVeh)
            continue;
        const unsigned int wtype = info.m_nWeaponType;
        if (wtype != WEAPONTYPE_ROCKET && wtype != WEAPONTYPE_ROCKET_HS
            && wtype != WEAPONTYPE_RLAUNCHER && wtype != WEAPONTYPE_RLAUNCHER_HS)
            continue;

        const CVector worldPos = CProjectileInfo::ms_apProjectile[i]->GetPosition();
        D3DXVECTOR3 missileWorldPos;
        RadarGeometry::WorldToRadarPos(worldPos.x, worldPos.y, missileWorldPos);

        float screenX = 0.0f, screenY = 0.0f;
        float iconX = 0.0f, iconY = 0.0f;
        if (!MathUtils::WorldToScreen(missileWorldPos, cameraPos, cameraRot, camState.fov,
                                      m_nearPlane, m_farPlane, rtWidth, rtHeight, screenX, screenY, screenAspect))
        {
            D3DXVECTOR3 playerPos(camState.posX, camState.posY, 0.0f);
            D3DXVECTOR3 missilePos2D;
            RadarGeometry::WorldToRadarPos(worldPos.x, worldPos.y, missilePos2D);
            float angle = 0.0f;
            if (!MathUtils::DirectionToOrbitAngle(playerPos, missilePos2D, camState.yaw, angle))
                continue;
            RadarGeometry::PointOnOrbitEdge(centerX, centerY, halfX, halfY, cosf(angle), sinf(angle),
                                            useSquareOrbit, iconX, iconY);
        }
        else
        {
            iconX = centerX + (screenX / rtWidth - 0.5f) * sizeX;
            iconY = centerY + (screenY / rtHeight - 0.5f) * sizeY;
            if (!RadarGeometry::IsInsideOrbit(iconX, iconY, centerX, centerY, halfX, halfY, useSquareOrbit))
                RadarGeometry::ClampToOrbit(iconX, iconY, centerX, centerY, halfX, halfY, iconX, iconY, useSquareOrbit);
        }

        m_pDraw->dxDrawGTAIndicatorBlip(iconX, iconY, indicatorSize, missileColor,
            BlipManager::GetHeightIndicatorType(worldPos.z, playerPosR.z, 2.5f));
    }
}

void RadarRenderer::RenderSettingsPreview(float panelLeft, float panelTop, float panelRight, float panelBottom,
                                          float screenW, float screenH, RadarConfig::RadarCamContext camCtx)
{
    if (!m_bInitialized || !m_pd3dDevice || screenW < 1.0f || screenH < 1.0f)
        return;

    HRESULT hr = m_pd3dDevice->TestCooperativeLevel();
    if (FAILED(hr) && hr != D3DERR_DEVICENOTRESET)
        return;

    if (!RwD3D9GetCurrentD3DDevice())
        return;

    struct PreviewSavedState
    {
        CameraController::CameraState cam{};
        bool                          settingsPreview = false;
        bool                          isInAircraft = false;
        bool                          isInPlane = false;
        CPed*                         player = nullptr;
        int                           width = 0;
        int                           height = 0;
    } saved;

    if (m_pCameraController)
        saved.cam = m_pCameraController->GetState();
    saved.settingsPreview = m_bSettingsPreview;
    saved.isInAircraft = m_cachedIsInAircraft;
    saved.isInPlane = m_cachedIsInPlane;
    saved.player = m_cachedPlayer;
    saved.width = m_width;
    saved.height = m_height;

    m_bSettingsPreview = true;
    m_cachedPlayer = nullptr;
    m_cachedIsInPlane = (camCtx == RadarConfig::RadarCamContext::Plane);
    m_cachedIsInAircraft = m_cachedIsInPlane || (camCtx == RadarConfig::RadarCamContext::Heli);
    m_cachedRollAngle = 0.0f;
    m_cachedPitchAngle = 0.0f;
    m_width = (int)screenW;
    m_height = (int)screenH;

    const bool shapeCircle = RadarConfig::GetShapeCircle();
    m_bRadarShapeCircle = shapeCircle;
    m_bBorderShapeCircle = shapeCircle;
    m_bShowGangZones = RadarConfig::GetShowGangZones();

    if (m_pCameraController)
    {
        m_pCameraController->ApplySettingsPreview(camCtx);
        float offsetWorldX = 0.0f;
        float offsetWorldY = 0.0f;
        D3DXVECTOR3 cameraPos;
        D3DXVECTOR3 cameraRot;
        m_pCameraController->GetCachedCalculations(offsetWorldX, offsetWorldY, cameraPos, cameraRot);
    }

    const float hudScaleX = screenW / 1920.0f;
    const float hudScaleY = screenH / 1080.0f;
    const float hudScale = (hudScaleX + hudScaleY) * 0.5f;

    float sizeX = shapeCircle
        ? static_cast<float>(RadarConfig::GetCircleSize()) * hudScale
        : static_cast<float>(RadarConfig::GetSquareSizeX()) * hudScale;
    float sizeY = shapeCircle
        ? sizeX
        : static_cast<float>(RadarConfig::GetSquareSizeY()) * hudScale;
    const float border = static_cast<float>(RadarConfig::GetBorderThickness());
    if (sizeX < 1.0f) sizeX = 1.0f;
    if (sizeY < 1.0f) sizeY = 1.0f;

    const float availW = (panelRight - panelLeft) - border * hudScale * 2.0f;
    const float availH = (panelBottom - panelTop) - border * hudScale * 2.0f;
    const float totalW = sizeX + border * hudScale * 2.0f;
    const float totalH = sizeY + border * hudScale * 2.0f;
    float fitScale = 1.0f;
    if (totalW > 1.0f && totalH > 1.0f && availW > 1.0f && availH > 1.0f)
        fitScale = (std::min)(availW / totalW, availH / totalH);
    fitScale = (std::max)(0.05f, (std::min)(fitScale, 1.0f));

    sizeX *= fitScale;
    sizeY *= fitScale;

    // Leave travel room so Offset X/Y can move the sample (do not fill the panel).
    {
        const float panelW = panelRight - panelLeft;
        const float panelH = panelBottom - panelTop;
        const float minSlack = 72.0f * hudScale;
        float shrink = 1.0f;
        if (sizeX > 1.0f && (panelW - sizeX) < minSlack * 2.0f)
        {
            const float want = panelW - minSlack * 2.0f;
            if (want > 1.0f)
                shrink = (std::min)(shrink, want / sizeX);
        }
        if (sizeY > 1.0f && (panelH - sizeY) < minSlack * 2.0f)
        {
            const float want = panelH - minSlack * 2.0f;
            if (want > 1.0f)
                shrink = (std::min)(shrink, want / sizeY);
        }
        if (shrink < 1.0f && shrink > 0.05f)
        {
            sizeX *= shrink;
            sizeY *= shrink;
        }
    }

    // HUD-like placement inside preview: offset from bottom-left (same as CalculateRadarPosition).
    const float maxOff = 1000.0f;
    float tX = (float)RadarConfig::GetOffsetX() / maxOff;
    float tY = (float)RadarConfig::GetOffsetY() / maxOff;
    if (tX < 0.0f) tX = 0.0f;
    if (tX > 1.0f) tX = 1.0f;
    if (tY < 0.0f) tY = 0.0f;
    if (tY > 1.0f) tY = 1.0f;
    const float roomX = (std::max)(0.0f, (panelRight - panelLeft) - sizeX);
    const float roomY = (std::max)(0.0f, (panelBottom - panelTop) - sizeY);
    const float circleX = panelLeft + tX * roomX;
    const float circleY = panelBottom - sizeY - tY * roomY;

    if (!BeginRadarZone())
    {
        m_bSettingsPreview = saved.settingsPreview;
        m_cachedIsInAircraft = saved.isInAircraft;
        m_cachedIsInPlane = saved.isInPlane;
        m_cachedPlayer = saved.player;
        m_width = saved.width;
        m_height = saved.height;
        if (m_pCameraController)
            m_pCameraController->SetState(saved.cam);
        return;
    }

    float baseSize = (sizeX < sizeY) ? sizeX : sizeY;
    int rtSize = (int)(baseSize + 0.5f);
    if (rtSize < 1) rtSize = 1;
    if (m_pRenderTarget && (m_pRenderTarget->GetWidth() != rtSize || m_pRenderTarget->GetHeight() != rtSize))
        m_pRenderTarget->dxCreateRenderTarget(rtSize, rtSize);

    UpdateDrawResources();

    if (m_pMapChunkManager)
    {
        m_pMapChunkManager->LoadAllChunks();
        // Preview can fall back to plugin map tiles when stock txds are unavailable.
        m_pMapChunkManager->SetPreviewTileFallback(true);
        if (m_pMapChunkManager->GetReadyTileCount() == 0)
            m_pMapChunkManager->EnsureStockChunks();
    }

    try
    {
        if (m_pRenderTarget && m_pRenderTarget->GetSurface())
        {
            if (m_pRenderTarget->dxSetRenderTarget(m_pRenderTarget->GetSurface()))
                RenderRadarTargetContents(circleX, circleY, sizeX, sizeY);
            m_pRenderTarget->dxSetRenderTarget(nullptr);

            D3DVIEWPORT9 screenViewport = {};
            screenViewport.Width = (DWORD)screenW;
            screenViewport.Height = (DWORD)screenH;
            screenViewport.MinZ = 0.0f;
            screenViewport.MaxZ = 1.0f;
            m_pd3dDevice->SetViewport(&screenViewport);
        }

        RenderBlipRenderTarget(circleX, circleY, sizeX, sizeY, true);
        RenderRadarOverlays(circleX, circleY, sizeX, sizeY);
    }
    catch (...)
    {
        if (m_pRenderTarget)
            m_pRenderTarget->dxSetRenderTarget(nullptr);
    }

    EndRadarZone();
    RenderRadarSprites(circleX, circleY, sizeX, sizeY);
    StockRadarDraw::InvalidateRwShaderCache();

    if (m_pMapChunkManager)
        m_pMapChunkManager->SetPreviewTileFallback(false);

    m_bSettingsPreview = saved.settingsPreview;
    m_cachedIsInAircraft = saved.isInAircraft;
    m_cachedIsInPlane = saved.isInPlane;
    m_cachedPlayer = saved.player;
    m_width = saved.width;
    m_height = saved.height;
    if (m_pCameraController)
    {
        m_pCameraController->SetState(saved.cam);
        m_pCameraController->InvalidateCache();
    }
}

void RadarRenderer::Render()
{
    if (!m_bInitialized || !m_pd3dDevice)
        return;
    
    bool inInterior = GameState::IsPlayerInInterior();
    if (m_bWasInInterior && !inInterior)
        m_exitInteriorImageStartTime = CTimer::m_snTimeInMilliseconds;
    m_bWasInInterior = inInterior;
    if (!GameState::ShouldDrawRadar())
        return;
    

    HRESULT hr = m_pd3dDevice->TestCooperativeLevel();
    if (FAILED(hr) && hr != D3DERR_DEVICENOTRESET)
        return;
    
    if (!RwD3D9GetCurrentD3DDevice())
        return;
    
    if (RsGlobal.maximumWidth <= 0 || RsGlobal.maximumHeight <= 0)
        return;

    m_width = RsGlobal.maximumWidth;
    m_height = RsGlobal.maximumHeight;
#ifdef _DEBUG
    m_debugChunksRenderedLastFrame = 0;
#endif

    if (!BeginRadarZone())
        return;

    float circleX = 0.0f, circleY = 0.0f, sizeX = 0.0f, sizeY = 0.0f;
    bool haveRadarLayout = false;

    try {
        m_cachedPlayer = FindPlayerPed();
        
        if (m_pCameraController)
        {
            m_pCameraController->UpdateFromGame(m_cachedPlayer);
            m_pCameraController->UpdateFromSpeed(m_cachedPlayer);
            
            m_cachedIsInAircraft = m_pCameraController->IsInAircraft(m_cachedPlayer);
            m_cachedIsInPlane = m_pCameraController->IsInPlane(m_cachedPlayer);

            if (m_cachedIsInAircraft)
            {
                m_cachedRollAngle = m_pCameraController->GetVehicleRollAngle(m_cachedPlayer);
                m_cachedPitchAngle = m_pCameraController->GetVehiclePitchAngle(m_cachedPlayer);
            }
        }

    CalculateRadarPosition(circleX, circleY, sizeX, sizeY);
    haveRadarLayout = true;

    float baseSize = (sizeX < sizeY) ? sizeX : sizeY;
    int rtSize = (int)(baseSize + 0.5f);
    if (rtSize < 1) rtSize = 1;
    if (m_pRenderTarget && (m_pRenderTarget->GetWidth() != rtSize || m_pRenderTarget->GetHeight() != rtSize))
        m_pRenderTarget->dxCreateRenderTarget(rtSize, rtSize);

    UpdateDrawResources();

    if (m_pMapChunkManager && !RadarConfig::GetCustomRadarTxd())
        m_pMapChunkManager->EnsureStockChunks();

    if (m_pRenderTarget && m_pRenderTarget->GetSurface() && GameState::ShouldDrawRadarMap())
    {
        if (m_pRenderTarget->dxSetRenderTarget(m_pRenderTarget->GetSurface()))
            RenderRadarTargetContents(circleX, circleY, sizeX, sizeY);
        m_pRenderTarget->dxSetRenderTarget(nullptr);

        D3DVIEWPORT9 screenViewport;
        screenViewport.X = 0;
        screenViewport.Y = 0;
        screenViewport.Width = RsGlobal.maximumWidth;
        screenViewport.Height = RsGlobal.maximumHeight;
        screenViewport.MinZ = 0.0f;
        screenViewport.MaxZ = 1.0f;
        m_pd3dDevice->SetViewport(&screenViewport);
    }

    RenderBlipRenderTarget(circleX, circleY, sizeX, sizeY, false);
    RenderRadarOverlays(circleX, circleY, sizeX, sizeY);
    }
    catch (...) {
        if (m_pRenderTarget)
            m_pRenderTarget->dxSetRenderTarget(nullptr);
        if (m_pBlipRenderTarget)
            m_pBlipRenderTarget->dxSetRenderTarget(nullptr);
    }

    // The-Definitive-UI: overlays (map→BlipRT→border→missiles/alt/radio) then EndZone → sprites (north/orbit).
    EndRadarZone();

    if (haveRadarLayout)
        RenderRadarSprites(circleX, circleY, sizeX, sizeY);

    StockRadarDraw::InvalidateRwShaderCache();
}

void RadarRenderer::dxDrawCircleShader(float x, float y, float width, float height, LPDIRECT3DTEXTURE9 texture, DWORD color, float alpha)
{
    if (m_pDraw) m_pDraw->dxDrawCircleShader(x, y, width, height, texture, color, alpha);
}

void RadarRenderer::dxDrawBorderShader(float x, float y, float width, float height, DWORD color, float alpha, float borderThicknessPixels)
{
    if (m_pDraw) m_pDraw->dxDrawBorderShader(x, y, width, height, color, alpha, borderThicknessPixels);
}

void RadarRenderer::dxDrawGreenSquareFill(float x, float y, float width, float height, LPDIRECT3DTEXTURE9 texture, DWORD color, float fillLevel)
{
    if (m_pDraw) m_pDraw->dxDrawGreenSquareFill(x, y, width, height, texture, color, fillLevel);
}

void RadarRenderer::dxDrawImage2D(float x, float y, float width, float height, LPDIRECT3DTEXTURE9 texture, DWORD color)
{
    if (m_pDraw) m_pDraw->dxDrawImage2D(x, y, width, height, texture, color);
}

void RadarRenderer::dxDrawImage2DRotated(float x, float y, float width, float height, LPDIRECT3DTEXTURE9 texture, float rotationAngle, DWORD color)
{
    if (m_pDraw) m_pDraw->dxDrawImage2DRotated(x, y, width, height, texture, rotationAngle, color);
}

void RadarRenderer::dxDrawRectangle(float x, float y, float width, float height, DWORD color)
{
    if (m_pDraw) m_pDraw->dxDrawRectangle(x, y, width, height, color);
}

void RadarRenderer::dxDrawGTAIndicatorBlip(float screenX, float screenY, float size, DWORD color, eHeightIndicatorType type)
{
    if (m_pDraw) m_pDraw->dxDrawGTAIndicatorBlip(screenX, screenY, size, color, type);
}

void RadarRenderer::dxDrawImage3D(const D3DXVECTOR3& elementPos, const D3DXVECTOR3& elementRot, const D3DXVECTOR2& elementSize,
                                 const D3DXVECTOR3& cameraPos, const D3DXVECTOR3& cameraRot,
                                 float fov, float nearPlane, float farPlane,
                                 LPDIRECT3DTEXTURE9 texture, DWORD color)
{
    if (m_pDraw) m_pDraw->dxDrawImage3D(elementPos, elementRot, elementSize, cameraPos, cameraRot, fov, nearPlane, farPlane, texture, color);
}

void RadarRenderer::dxDrawText(const char* text, float x, float y, float sx, float sy, float rotation, DWORD color)
{
    if (m_pDraw) m_pDraw->dxDrawText(text, x, y, sx, sy, rotation, color);
}

#ifdef _DEBUG
void RadarRenderer::dxDrawRenderDebugMemory()
{
    if (!m_pd3dDevice || !m_pFont)
        return;

    // EnumProcessModules every frame tanks FPS — refresh stats ~1 Hz.
    static const char s_targetModuleName[] = "The-Definitive-UI.SA.asi";
    static SIZE_T s_sizeMb = 0, s_sizeKb = 0, s_processMb = 0;
    static DWORD s_lastSampleMs = 0;
    const DWORD now = GetTickCount();
    if (s_lastSampleMs == 0 || (now - s_lastSampleMs) >= 1000u)
    {
        s_lastSampleMs = now;
        HMODULE hOurModule = nullptr;
        // Address of this TU's static data → our ASI module (not a member-fn ptr).
        GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
            reinterpret_cast<LPCSTR>(s_targetModuleName), &hOurModule);
        if (!hOurModule)
            hOurModule = GetModuleHandleA(s_targetModuleName);
        if (hOurModule)
        {
            MODULEINFO mi = {};
            if (GetModuleInformation(GetCurrentProcess(), hOurModule, &mi, sizeof(mi)))
            {
                s_sizeMb = mi.SizeOfImage / (1024 * 1024);
                s_sizeKb = (mi.SizeOfImage % (1024 * 1024)) / 1024;
            }
        }
        PROCESS_MEMORY_COUNTERS pmc = {};
        pmc.cb = sizeof(pmc);
        if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc)))
            s_processMb = pmc.WorkingSetSize / (1024 * 1024);
    }
    const SIZE_T sizeMb = s_sizeMb;
    const SIZE_T sizeKb = s_sizeKb;
    const SIZE_T processMb = s_processMb;

    auto drawWithOutline = [this](const char* text, float x, float y, float sx, float sy, DWORD color) {
        const DWORD black = D3DCOLOR_XRGB(0, 0, 0);
        for (int dy = -1; dy <= 1; ++dy)
            for (int dx = -1; dx <= 1; ++dx)
                if (dx != 0 || dy != 0)
                    m_pDraw->dxDrawText(text, x + (float)dx, y + (float)dy, sx, sy, 0.0f, black);
        m_pDraw->dxDrawText(text, x, y, sx, sy, 0.0f, color);
    };

    const DWORD color = tocolor(255, 255, 255, 255);
    char buf[160];
    float lineY = 10.0f;
    sprintf_s(buf, "DLL (%s): %u \xCC\xC1 %u \xCA\xC1", s_targetModuleName, (unsigned)sizeMb, (unsigned)sizeKb);
    drawWithOutline(buf, 10.0f, lineY, 420.0f, 22.0f, color);
    lineY += 24.0f;
    sprintf_s(buf, "\xCF\xF0\xEE\xF6\xE5\xF1\xF1: %u \xCC\xC1", (unsigned)processMb);
    drawWithOutline(buf, 10.0f, lineY, 420.0f, 22.0f, color);
    lineY += 24.0f;

    int chunksRendered = 0;
    int chunksLoaded = 0;
    int chunksUnloaded = MapChunkManager::MAP_CHUNKS_COUNT;
    size_t blipsTotal = 0;
    if (CRadar::ms_RadarTrace)
    {
        for (unsigned int i = 0; i < MAX_RADAR_TRACES; ++i)
            if (CRadar::ms_RadarTrace[i].m_bInUse)
                ++blipsTotal;
    }
    if (m_pMapChunkManager)
    {
        chunksLoaded = m_pMapChunkManager->GetLoadedChunksCount();
        chunksUnloaded = MapChunkManager::MAP_CHUNKS_COUNT - chunksLoaded;
        chunksRendered = m_debugChunksRenderedLastFrame;
    }
    sprintf_s(buf, "Chunks: %d rend., %d load., %d unload. (of %d)", chunksRendered, chunksLoaded, chunksUnloaded, MapChunkManager::MAP_CHUNKS_COUNT);
    drawWithOutline(buf, 10.0f, lineY, 560.0f, 22.0f, color);
    lineY += 24.0f;
    sprintf_s(buf, "\xC1\xEB\xE8\xEF\xFB: %zu active", blipsTotal);
    drawWithOutline(buf, 10.0f, lineY, 420.0f, 22.0f, color);
}

#endif // _DEBUG

void RadarRenderer::MarkRadioNameVisible()
{
    m_bRadioNameVisible = true;
}

/**
 * HUD radio name — drawn from radar overlays (inside D3D stateblock).
 * DisplayRadioStationName only arms the timer and sets m_bRadioNameVisible.
 */
void RadarRenderer::RenderRadioText()
{
    if (!m_bRadioNameVisible)
        return;
    m_bRadioNameVisible = false;

    if (!m_pRenderRadio || !m_pRadioFont)
        return;

    int station = AERadioTrackManager.m_nStationsListed
        + AERadioTrackManager.m_TempSettings.m_nCurrentRadioStation;
    constexpr int kRadioCount = 14;
    if (!station)
        return;
    if (station >= kRadioCount)
        station -= kRadioCount - 1;
    else if (station <= 0)
        station += kRadioCount - 1;

    const char* stationName = LanguageManager::GetRadioStation(station);
    if (!stationName || stationName[0] == '\0')
        return;

    const bool listing = AERadioTrackManager.m_nStationsListed
        || AERadioTrackManager.m_nStationsListDown;
    const CRGBA rgb = HudColour.GetRGB(
        listing ? HUD_COLOUR_GREY : HUD_COLOUR_ORANGE, 255);
    const DWORD textColor = tocolor(rgb.r, rgb.g, rgb.b, rgb.a);
    const DWORD dropColor = tocolor(0, 0, 0, 255);

    float circleX, circleY, sizeX, sizeY;
    CalculateRadarPosition(circleX, circleY, sizeX, sizeY);

    float textWidth = 250.0f;
    float textHeight = 80.0f;

    float radarCenterX = circleX + sizeX * 0.5f;
    float textX = radarCenterX - textWidth * 0.5f;
    float textY = circleY - textHeight - 10.0f;

    m_pRenderRadio->RenderWithOutline(
        stationName,
        "",
        textX, textY,
        textWidth, textHeight,
        textColor,
        dropColor,
        66.0f
    );
}
