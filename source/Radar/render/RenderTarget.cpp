/*****************************************************************************
 *
 *  PROJECT:     The-Definitive-UI
 *  FILE:        source/Radar/render/RenderTarget.cpp
 *  PURPOSE:     Offscreen render target for the radar plane
 *
 *****************************************************************************/

#include "RenderTarget.h"
#include "Config.h"
#include "RenderWare.h"

// RW D3D9 device cache — same addresses as gta-reversed WindowedMode.cpp
static auto& RwD3D9RenderSurface       = *reinterpret_cast<LPDIRECT3DSURFACE9*>(0xC97C30);
static auto& RwD3D9DepthStencilSurface = *reinterpret_cast<LPDIRECT3DSURFACE9*>(0xC97C2C);

static RwBool RwSetRenderTarget(LPDIRECT3DSURFACE9 surface)
{
    return ((RwBool(__cdecl*)(RwUInt32, LPDIRECT3DSURFACE9))0x7F5F20)(0, surface);
}

static void RwSetDepthStencil(LPDIRECT3DSURFACE9 surface)
{
    ((void(__cdecl*)(LPDIRECT3DSURFACE9))0x7F5EF0)(surface);
}

RenderTarget::RenderTarget(LPDIRECT3DDEVICE9 pDevice)
    : m_pDevice(pDevice)
    , m_pRenderTargetTexture(nullptr)
    , m_pRenderTargetSurface(nullptr)
    , m_pOldRenderTarget(nullptr)
    , m_pOldDepthStencil(nullptr)
    , m_pSavedRwRT(nullptr)
    , m_pSavedRwDS(nullptr)
    , m_bViewportSaved(false)
    , m_width(0)
    , m_height(0)
{
}

RenderTarget::~RenderTarget()
{
    dxSetRenderTarget(nullptr);

    if (m_pRenderTargetSurface)
    {
        m_pRenderTargetSurface->Release();
        m_pRenderTargetSurface = nullptr;
    }
    if (m_pRenderTargetTexture)
    {
        m_pRenderTargetTexture->Release();
        m_pRenderTargetTexture = nullptr;
    }
}

bool RenderTarget::dxCreateRenderTarget(int width, int height)
{
    if (!m_pDevice)
        return false;

    if (m_pRenderTargetSurface)
    {
        m_pRenderTargetSurface->Release();
        m_pRenderTargetSurface = nullptr;
    }
    if (m_pRenderTargetTexture)
    {
        m_pRenderTargetTexture->Release();
        m_pRenderTargetTexture = nullptr;
    }

    HRESULT hr = m_pDevice->CreateTexture(
        width, height,
        1,
        D3DUSAGE_RENDERTARGET,
        D3DFMT_A8R8G8B8,
        D3DPOOL_DEFAULT,
        &m_pRenderTargetTexture,
        nullptr
    );

    if (FAILED(hr))
        return false;

    hr = m_pRenderTargetTexture->GetSurfaceLevel(0, &m_pRenderTargetSurface);
    if (FAILED(hr))
    {
        m_pRenderTargetTexture->Release();
        m_pRenderTargetTexture = nullptr;
        return false;
    }

    m_width  = width;
    m_height = height;

    return true;
}

bool RenderTarget::dxSetRenderTarget(LPDIRECT3DSURFACE9 surface)
{
    if (!m_pDevice)
        return false;

    HRESULT hr = m_pDevice->TestCooperativeLevel();
    if (FAILED(hr) && hr != D3DERR_DEVICENOTRESET)
        return false;

    if (surface == nullptr)
    {
        if (m_pOldRenderTarget)
        {
            RwSetRenderTarget(m_pOldRenderTarget);
            m_pDevice->SetRenderTarget(0, m_pOldRenderTarget);
            m_pOldRenderTarget->Release();
            m_pOldRenderTarget = nullptr;
        }
        if (m_pOldDepthStencil)
        {
            RwSetDepthStencil(m_pOldDepthStencil);
            m_pDevice->SetDepthStencilSurface(m_pOldDepthStencil);
            m_pOldDepthStencil->Release();
            m_pOldDepthStencil = nullptr;
        }
        else
        {
            RwSetDepthStencil(m_pSavedRwDS);
        }
        RwD3D9RenderSurface = m_pSavedRwRT;
        RwD3D9DepthStencilSurface = m_pSavedRwDS;
        m_pSavedRwRT = nullptr;
        m_pSavedRwDS = nullptr;
        if (m_bViewportSaved)
        {
            m_pDevice->SetViewport(&m_oldViewport);
            m_bViewportSaved = false;
        }
        return true;
    }

    m_pDevice->GetRenderTarget(0, &m_pOldRenderTarget);
    m_pDevice->GetDepthStencilSurface(&m_pOldDepthStencil);
    m_pDevice->GetViewport(&m_oldViewport);
    m_bViewportSaved = true;
    m_pSavedRwRT = RwD3D9RenderSurface;
    m_pSavedRwDS = RwD3D9DepthStencilSurface;

    // D3D9 forbids a small color RT with the game's fullscreen DS.
    // Stock CRadar never changes RT; RW itself unbinds DS when sizes differ
    // (_rwD3D9SetDepthStencilSurface(NULL) in CameraBeginUpdate).
    RwSetDepthStencil(nullptr);
    if (FAILED(m_pDevice->SetDepthStencilSurface(nullptr)))
    {
        dxSetRenderTarget(nullptr);
        return false;
    }
    if (!RwSetRenderTarget(surface) || FAILED(m_pDevice->SetRenderTarget(0, surface)))
    {
        dxSetRenderTarget(nullptr);
        return false;
    }
    RwD3D9RenderSurface = surface;
    RwD3D9DepthStencilSurface = nullptr;

    D3DVIEWPORT9 vp;
    vp.X       = 0;
    vp.Y       = 0;
    vp.Width   = m_width;
    vp.Height  = m_height;
    vp.MinZ    = 0.0f;
    vp.MaxZ    = 1.0f;
    m_pDevice->SetViewport(&vp);

    int bgR, bgG, bgB, bgA;
    RadarConfig::GetBackgroundColor(bgR, bgG, bgB, bgA);
    m_pDevice->Clear(0, nullptr, D3DCLEAR_TARGET, D3DCOLOR_ARGB(bgA, bgR, bgG, bgB), 1.0f, 0);
    return true;
}
