/*****************************************************************************
 *
 *  PROJECT:     The-Definitive-UI
 *  FILE:        source/Draw/Draw.cpp
 *  PURPOSE:     2D drawing helpers (texture + text), inspired by MTA CGraphics
 *
 *****************************************************************************/

#include "Draw.h"
#include "Utils.h"

#include "plugin.h"
#include "RenderWare.h"

#include <windows.h>
#include <cmath>

namespace
{
    // Same addresses as RenderTarget / gta-reversed WindowedMode
    auto& RwD3D9RenderSurface       = *reinterpret_cast<LPDIRECT3DSURFACE9*>(0xC97C30);
    auto& RwD3D9DepthStencilSurface = *reinterpret_cast<LPDIRECT3DSURFACE9*>(0xC97C2C);

    RwBool RwSetRenderTarget(LPDIRECT3DSURFACE9 surface)
    {
        return ((RwBool(__cdecl*)(RwUInt32, LPDIRECT3DSURFACE9))0x7F5F20)(0, surface);
    }

    void RwSetDepthStencil(LPDIRECT3DSURFACE9 surface)
    {
        ((void(__cdecl*)(LPDIRECT3DSURFACE9))0x7F5EF0)(surface);
    }

    struct ScreenVertex
    {
        float x, y, z, rhw;
        DWORD color;
        float u, v;
    };

    const DWORD SCREEN_FVF = D3DFVF_XYZRHW | D3DFVF_DIFFUSE | D3DFVF_TEX1;

    int Utf8ToWide(const char* utf8, wchar_t* out, int outChars)
    {
        if (!utf8 || !out || outChars <= 0)
            return 0;
        return MultiByteToWideChar(CP_UTF8, 0, utf8, -1, out, outChars);
    }

    // After D3D Reset GTA rebinds a backbuffer-sized depth stencil. Binding a smaller
    // offscreen RT while that DS is attached makes D3D9 disable rasterization (MSDN:
    // "If the depth stencil surface is not the same size as the render target,
    // rendering is disabled."). SetRenderTarget still succeeds — cutout blit is empty.
    struct SavedFb
    {
        LPDIRECT3DSURFACE9 rt = nullptr;
        LPDIRECT3DSURFACE9 ds = nullptr;
        LPDIRECT3DSURFACE9 rwRT = nullptr; // game globals, no AddRef
        LPDIRECT3DSURFACE9 rwDS = nullptr;
        D3DVIEWPORT9       vp{};
    };

    void ReleaseSavedFb(SavedFb& s)
    {
        if (s.rt) { s.rt->Release(); s.rt = nullptr; }
        if (s.ds) { s.ds->Release(); s.ds = nullptr; }
        s.rwRT = nullptr;
        s.rwDS = nullptr;
    }

    bool SetOffscreenColorRt(LPDIRECT3DDEVICE9 dev, LPDIRECT3DSURFACE9 surf, UINT w, UINT h)
    {
        if (!dev || !surf)
            return false;
        if (!RwSetRenderTarget(surf) || FAILED(dev->SetRenderTarget(0, surf)))
            return false;
        RwD3D9RenderSurface = surf;
        RwD3D9DepthStencilSurface = nullptr;

        D3DVIEWPORT9 vp{};
        vp.Width = w;
        vp.Height = h;
        vp.MinZ = 0.0f;
        vp.MaxZ = 1.0f;
        dev->SetViewport(&vp);
        return true;
    }

    void RestoreFb(LPDIRECT3DDEVICE9 dev, SavedFb& s)
    {
        if (dev)
        {
            if (s.rt)
            {
                RwSetRenderTarget(s.rt);
                dev->SetRenderTarget(0, s.rt);
            }
            if (s.ds)
            {
                RwSetDepthStencil(s.ds);
                dev->SetDepthStencilSurface(s.ds);
            }
            else
            {
                RwSetDepthStencil(s.rwDS);
                dev->SetDepthStencilSurface(s.rwDS);
            }
            RwD3D9RenderSurface = s.rwRT;
            RwD3D9DepthStencilSurface = s.rwDS;
            dev->SetViewport(&s.vp);
        }
        ReleaseSavedFb(s);
    }

    bool BindOffscreen(LPDIRECT3DDEVICE9 dev, LPDIRECT3DSURFACE9 surf, UINT w, UINT h, SavedFb* save)
    {
        if (!dev || !surf || w < 1 || h < 1)
            return false;

        if (save)
        {
            *save = {};
            dev->GetRenderTarget(0, &save->rt);
            dev->GetDepthStencilSurface(&save->ds);
            dev->GetViewport(&save->vp);
            save->rwRT = RwD3D9RenderSurface;
            save->rwDS = RwD3D9DepthStencilSurface;
        }

        RwSetDepthStencil(nullptr);
        if (FAILED(dev->SetDepthStencilSurface(nullptr)))
        {
            if (save)
            {
                if (save->rt)
                    dev->SetRenderTarget(0, save->rt);
                dev->SetDepthStencilSurface(save->ds);
                dev->SetViewport(&save->vp);
            }
            return false;
        }
        if (!RwSetRenderTarget(surf) || FAILED(dev->SetRenderTarget(0, surf)))
        {
            if (save)
            {
                RwSetDepthStencil(save->ds);
                if (save->rt)
                {
                    RwSetRenderTarget(save->rt);
                    dev->SetRenderTarget(0, save->rt);
                }
                dev->SetDepthStencilSurface(save->ds);
                dev->SetViewport(&save->vp);
                RwD3D9RenderSurface = save->rwRT;
                RwD3D9DepthStencilSurface = save->rwDS;
            }
            return false;
        }
        RwD3D9RenderSurface = surf;
        RwD3D9DepthStencilSurface = nullptr;

        D3DVIEWPORT9 vp{};
        vp.Width = w;
        vp.Height = h;
        vp.MinZ = 0.0f;
        vp.MaxZ = 1.0f;
        dev->SetViewport(&vp);
        dev->SetRenderState(D3DRS_ZENABLE, FALSE);
        dev->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
        dev->SetRenderState(D3DRS_STENCILENABLE, FALSE);
        return true;
    }
}

Draw::Draw()
    : m_pDevice(nullptr)
    , m_pSprite(nullptr)
    , m_pFont(nullptr)
    , m_pFontHover(nullptr)
    , m_pCutoutTex(nullptr)
    , m_pCutoutSurf(nullptr)
    , m_pMaskTex(nullptr)
    , m_pMaskSurf(nullptr)
    , m_nCutoutW(0)
    , m_nCutoutH(0)
    , m_pClipTex(nullptr)
    , m_pClipSurf(nullptr)
    , m_pClipSavedRT(nullptr)
    , m_pClipSavedDS(nullptr)
    , m_pClipSavedRwRT(nullptr)
    , m_pClipSavedRwDS(nullptr)
    , m_clipSavedVp{}
    , m_nClipTexW(0)
    , m_nClipTexH(0)
    , m_clipScreenX(0.0f)
    , m_clipScreenY(0.0f)
    , m_clipW(0.0f)
    , m_clipH(0.0f)
    , m_bClipActive(false)
    , m_pStateBlock(nullptr)
    , m_nUiDepth(0)
    , m_nFontHeight(0)
    , m_nFontHoverHeight(0)
    , m_nBackW(0)
    , m_nBackH(0)
    , m_bUiFocused(false)
    , m_bInitialized(false)
{
}

Draw::~Draw()
{
    Shutdown();
}

bool Draw::Initialize(LPDIRECT3DDEVICE9 pDevice)
{
    if (m_bInitialized)
        return true;

    if (!pDevice)
        return false;

    m_pDevice = pDevice;

    if (!CreateResources())
    {
        Shutdown();
        return false;
    }

    m_bInitialized = true;
    return true;
}

void Draw::Shutdown()
{
    ReleaseResources();
    m_pDevice = nullptr;
    m_bInitialized = false;
}

void Draw::BeginUi()
{
    if (!m_bInitialized || !m_pDevice)
        return;

    if (m_nUiDepth++ > 0)
        return;

    RefreshAfterFocusRestore();
    SyncBackbufferSize();

    if (m_nBackW >= 64 && m_nBackH >= 64)
    {
        D3DVIEWPORT9 vp{};
        vp.X = 0;
        vp.Y = 0;
        vp.Width = m_nBackW;
        vp.Height = m_nBackH;
        vp.MinZ = 0.0f;
        vp.MaxZ = 1.0f;
        m_pDevice->SetViewport(&vp);
    }

    if (m_pStateBlock && FAILED(m_pStateBlock->Capture()))
    {
        m_pStateBlock->Release();
        m_pStateBlock = nullptr;
        DropDefaultPoolRts();
        RecreateFonts();
    }

    if (!m_pStateBlock)
    {
        if (FAILED(m_pDevice->CreateStateBlock(D3DSBT_ALL, &m_pStateBlock)) || !m_pStateBlock)
        {
            m_nUiDepth = 0;
            return;
        }
        m_pStateBlock->Capture();
    }

    // Safe UI defaults after capture (Apply restores game). MTA does the same for stencil.
    m_pDevice->SetRenderState(D3DRS_STENCILENABLE, FALSE);
    m_pDevice->SetRenderState(D3DRS_COLORWRITEENABLE,
                              D3DCOLORWRITEENABLE_RED | D3DCOLORWRITEENABLE_GREEN |
                              D3DCOLORWRITEENABLE_BLUE | D3DCOLORWRITEENABLE_ALPHA);
    m_pDevice->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);
    m_pDevice->SetRenderState(D3DRS_ZENABLE, FALSE);
    m_pDevice->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
}

void Draw::EndUi()
{
    if (!m_bInitialized || !m_pDevice || m_nUiDepth <= 0)
        return;

    if (--m_nUiDepth > 0)
        return;

    m_pDevice->SetVertexShader(nullptr);
    m_pDevice->SetPixelShader(nullptr);

    if (m_pStateBlock)
        m_pStateBlock->Apply();

    Ui::PoisonRwShaderCache();
}

bool Draw::CreateResources()
{
    if (FAILED(D3DXCreateSprite(m_pDevice, &m_pSprite)))
        return false;

    return CreateFontFace(50);
}

bool Draw::CreateFontFace(int height)
{
    if (height < 1)
        height = 1;

    if (m_pFont)
    {
        m_pFont->Release();
        m_pFont = nullptr;
    }

    // Grayscale AA — soft edges; better for cutout masks than ClearType RGB fringe
    if (FAILED(D3DXCreateFontW(m_pDevice, height, 0, FW_BOLD, 1, FALSE, DEFAULT_CHARSET,
                               OUT_TT_PRECIS, ANTIALIASED_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
                               L"Arial", &m_pFont)))
        return false;

    m_nFontHeight = height;
    return true;
}

bool Draw::CreateHoverFontFace(int height)
{
    if (height < 1)
        height = 1;

    if (m_pFontHover)
    {
        m_pFontHover->Release();
        m_pFontHover = nullptr;
    }

    if (FAILED(D3DXCreateFontW(m_pDevice, height, 0, FW_BOLD, 1, FALSE, DEFAULT_CHARSET,
                               OUT_TT_PRECIS, ANTIALIASED_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
                               L"Arial", &m_pFontHover)))
        return false;

    m_nFontHoverHeight = height;
    return true;
}

bool Draw::EnsureFontHeight(int height)
{
    if (!m_pDevice)
        return false;
    if (m_pFont && m_nFontHeight == height)
        return true;
    return CreateFontFace(height);
}

bool Draw::EnsureHoverFontHeight(int height)
{
    if (!m_pDevice)
        return false;
    if (m_pFontHover && m_nFontHoverHeight == height)
        return true;
    return CreateHoverFontFace(height);
}

void Draw::ReleaseResources()
{
    if (m_bClipActive)
        EndClipRT();

    while (m_nUiDepth > 0)
        EndUi();

    if (m_pStateBlock)
    {
        m_pStateBlock->Release();
        m_pStateBlock = nullptr;
    }

    if (m_pClipSavedRT)
    {
        m_pClipSavedRT->Release();
        m_pClipSavedRT = nullptr;
    }
    if (m_pClipSavedDS)
    {
        m_pClipSavedDS->Release();
        m_pClipSavedDS = nullptr;
    }
    if (m_pClipSurf)
    {
        m_pClipSurf->Release();
        m_pClipSurf = nullptr;
    }
    if (m_pClipTex)
    {
        m_pClipTex->Release();
        m_pClipTex = nullptr;
    }
    m_nClipTexW = 0;
    m_nClipTexH = 0;
    m_bClipActive = false;

    if (m_pMaskSurf)
    {
        m_pMaskSurf->Release();
        m_pMaskSurf = nullptr;
    }
    if (m_pMaskTex)
    {
        m_pMaskTex->Release();
        m_pMaskTex = nullptr;
    }
    if (m_pCutoutSurf)
    {
        m_pCutoutSurf->Release();
        m_pCutoutSurf = nullptr;
    }
    if (m_pCutoutTex)
    {
        m_pCutoutTex->Release();
        m_pCutoutTex = nullptr;
    }
    m_nCutoutW = 0;
    m_nCutoutH = 0;

    m_nBackW = 0;
    m_nBackH = 0;
    m_bUiFocused = false;

    if (m_pFont)
    {
        m_pFont->Release();
        m_pFont = nullptr;
    }
    m_nFontHeight = 0;

    if (m_pFontHover)
    {
        m_pFontHover->Release();
        m_pFontHover = nullptr;
    }
    m_nFontHoverHeight = 0;

    if (m_pSprite)
    {
        m_pSprite->Release();
        m_pSprite = nullptr;
    }
}

void Draw::DropDefaultPoolRts()
{
    if (m_bClipActive)
        EndClipRT();

    if (m_pClipSavedRT) { m_pClipSavedRT->Release(); m_pClipSavedRT = nullptr; }
    if (m_pClipSavedDS) { m_pClipSavedDS->Release(); m_pClipSavedDS = nullptr; }
    m_pClipSavedRwRT = nullptr;
    m_pClipSavedRwDS = nullptr;
    if (m_pClipSurf) { m_pClipSurf->Release(); m_pClipSurf = nullptr; }
    if (m_pClipTex) { m_pClipTex->Release(); m_pClipTex = nullptr; }
    m_nClipTexW = 0;
    m_nClipTexH = 0;
    m_bClipActive = false;

    if (m_pMaskSurf) { m_pMaskSurf->Release(); m_pMaskSurf = nullptr; }
    if (m_pMaskTex) { m_pMaskTex->Release(); m_pMaskTex = nullptr; }
    if (m_pCutoutSurf) { m_pCutoutSurf->Release(); m_pCutoutSurf = nullptr; }
    if (m_pCutoutTex) { m_pCutoutTex->Release(); m_pCutoutTex = nullptr; }
    m_nCutoutW = 0;
    m_nCutoutH = 0;
}

void Draw::RecreateFonts()
{
    const int h = m_nFontHeight;
    const int hh = m_nFontHoverHeight;
    if (h > 0)
        CreateFontFace(h);
    if (hh > 0)
        CreateHoverFontFace(hh);
}

void Draw::SyncBackbufferSize()
{
    if (!m_pDevice)
        return;

    LPDIRECT3DSURFACE9 bb = nullptr;
    if (FAILED(m_pDevice->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &bb)) || !bb)
        return;

    D3DSURFACE_DESC desc{};
    bb->GetDesc(&desc);
    bb->Release();

    if (desc.Width == m_nBackW && desc.Height == m_nBackH)
        return;

    DropDefaultPoolRts();
    RecreateFonts();
    m_nBackW = desc.Width;
    m_nBackH = desc.Height;
}

void Draw::NotifyBackbufferChanged()
{
    m_nBackW = 0;
    m_nBackH = 0;
    SyncBackbufferSize();
}

void Draw::RefreshAfterFocusRestore()
{
    HWND hwnd = Ui::GameWindow();
    const bool live = hwnd && !IsIconic(hwnd) && GetForegroundWindow() == hwnd;
    if (live && !m_bUiFocused)
    {
        // Windowed Alt+Tab often skips D3D Reset; DEFAULT-pool cutout RTs and D3DX fonts
        // stay allocated but draw empty until dropped / OnResetDevice.
        DropDefaultPoolRts();
        if (m_pSprite)
        {
            m_pSprite->OnLostDevice();
            m_pSprite->OnResetDevice();
        }
        if (m_pFont)
        {
            m_pFont->OnLostDevice();
            m_pFont->OnResetDevice();
        }
        if (m_pFontHover)
        {
            m_pFontHover->OnLostDevice();
            m_pFontHover->OnResetDevice();
        }
        if (m_pStateBlock)
        {
            m_pStateBlock->Release();
            m_pStateBlock = nullptr;
        }
    }
    m_bUiFocused = live;
}

bool Draw::EnsureCutoutTarget(UINT width, UINT height)
{
    if (!m_pDevice || width < 1 || height < 1)
        return false;

    const UINT needW = (width + 15u) & ~15u;
    const UINT needH = (height + 15u) & ~15u;
    if (m_pCutoutTex && m_pMaskTex && m_nCutoutW >= needW && m_nCutoutH >= needH)
        return true;

    if (m_pMaskSurf) { m_pMaskSurf->Release(); m_pMaskSurf = nullptr; }
    if (m_pMaskTex) { m_pMaskTex->Release(); m_pMaskTex = nullptr; }
    if (m_pCutoutSurf) { m_pCutoutSurf->Release(); m_pCutoutSurf = nullptr; }
    if (m_pCutoutTex) { m_pCutoutTex->Release(); m_pCutoutTex = nullptr; }

    if (FAILED(m_pDevice->CreateTexture(needW, needH, 1, D3DUSAGE_RENDERTARGET, D3DFMT_A8R8G8B8,
                                        D3DPOOL_DEFAULT, &m_pCutoutTex, nullptr)))
        return false;
    if (FAILED(m_pCutoutTex->GetSurfaceLevel(0, &m_pCutoutSurf)))
    {
        m_pCutoutTex->Release();
        m_pCutoutTex = nullptr;
        return false;
    }

    if (FAILED(m_pDevice->CreateTexture(needW, needH, 1, D3DUSAGE_RENDERTARGET, D3DFMT_A8R8G8B8,
                                        D3DPOOL_DEFAULT, &m_pMaskTex, nullptr)))
    {
        m_pCutoutSurf->Release(); m_pCutoutSurf = nullptr;
        m_pCutoutTex->Release(); m_pCutoutTex = nullptr;
        return false;
    }
    if (FAILED(m_pMaskTex->GetSurfaceLevel(0, &m_pMaskSurf)))
    {
        m_pMaskTex->Release(); m_pMaskTex = nullptr;
        m_pCutoutSurf->Release(); m_pCutoutSurf = nullptr;
        m_pCutoutTex->Release(); m_pCutoutTex = nullptr;
        return false;
    }

    m_nCutoutW = needW;
    m_nCutoutH = needH;
    return true;
}

void Draw::DrawTexture(LPDIRECT3DTEXTURE9 pTexture, float fX, float fY, float fWidth, float fHeight,
                       DWORD dwColor, float fRotation)
{
    if (!m_bInitialized || !m_pDevice || !pTexture)
        return;

    if (fRotation == 0.0f)
    {
        DrawTexture(pTexture, fX, fY, fWidth, fHeight, 0.0f, 0.0f, 1.0f, 1.0f, dwColor);
        return;
    }

    if (!m_pSprite)
        return;

    D3DSURFACE_DESC desc{};
    if (FAILED(pTexture->GetLevelDesc(0, &desc)))
        return;

    RECT src = { 0, 0, static_cast<LONG>(desc.Width), static_cast<LONG>(desc.Height) };
    D3DXVECTOR2 scaling(fWidth / static_cast<float>(desc.Width), fHeight / static_cast<float>(desc.Height));
    D3DXVECTOR2 center(fWidth * 0.5f, fHeight * 0.5f);
    D3DXVECTOR2 position(fX, fY);

    D3DXMATRIX mat;
    D3DXMatrixTransformation2D(&mat, nullptr, 0.0f, &scaling, &center, fRotation, &position);

    if (FAILED(m_pSprite->Begin(D3DXSPRITE_ALPHABLEND)))
        return;

    m_pSprite->SetTransform(&mat);
    m_pSprite->Draw(pTexture, &src, nullptr, nullptr, dwColor);
    m_pSprite->End();
}

void Draw::DrawTexture(LPDIRECT3DTEXTURE9 pTexture, float fX, float fY, float fWidth, float fHeight,
                       float fU, float fV, float fSizeU, float fSizeV, DWORD dwColor)
{
    if (!m_bInitialized || !m_pDevice || !pTexture)
        return;

    // D3D9 texel-pixel alignment (avoids blurry/blocky sampling vs in-game path)
    const float x1 = fX - 0.5f;
    const float y1 = fY - 0.5f;
    const float x2 = fX + fWidth - 0.5f;
    const float y2 = fY + fHeight - 0.5f;
    const float u2 = fU + fSizeU;
    const float v2 = fV + fSizeV;

    ScreenVertex vertices[6] = {
        { x1, y1, 0.0f, 1.0f, dwColor, fU, fV },
        { x2, y1, 0.0f, 1.0f, dwColor, u2, fV },
        { x1, y2, 0.0f, 1.0f, dwColor, fU, v2 },
        { x2, y1, 0.0f, 1.0f, dwColor, u2, fV },
        { x2, y2, 0.0f, 1.0f, dwColor, u2, v2 },
        { x1, y2, 0.0f, 1.0f, dwColor, fU, v2 },
    };

    m_pDevice->SetVertexShader(nullptr);
    m_pDevice->SetPixelShader(nullptr);
    m_pDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
    m_pDevice->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
    m_pDevice->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
    m_pDevice->SetRenderState(D3DRS_ZENABLE, FALSE);
    m_pDevice->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
    m_pDevice->SetRenderState(D3DRS_LIGHTING, FALSE);

    // Frontend leaves POINT filter → pixelated upscale; force LINEAR like post-game path
    m_pDevice->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
    m_pDevice->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
    m_pDevice->SetSamplerState(0, D3DSAMP_MIPFILTER, D3DTEXF_NONE);
    m_pDevice->SetSamplerState(0, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
    m_pDevice->SetSamplerState(0, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);

    m_pDevice->SetTexture(0, pTexture);
    m_pDevice->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
    m_pDevice->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
    m_pDevice->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
    m_pDevice->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_MODULATE);
    m_pDevice->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
    m_pDevice->SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);
    m_pDevice->SetTextureStageState(1, D3DTSS_COLOROP, D3DTOP_DISABLE);
    m_pDevice->SetTextureStageState(1, D3DTSS_ALPHAOP, D3DTOP_DISABLE);
    m_pDevice->SetFVF(SCREEN_FVF);
    m_pDevice->DrawPrimitiveUP(D3DPT_TRIANGLELIST, 2, vertices, sizeof(ScreenVertex));
    m_pDevice->SetTexture(0, nullptr);
}

void Draw::DrawRect(float fX, float fY, float fWidth, float fHeight, DWORD dwColor)
{
    if (!m_bInitialized || !m_pDevice)
        return;

    const float x1 = fX - 0.5f;
    const float y1 = fY - 0.5f;
    const float x2 = fX + fWidth - 0.5f;
    const float y2 = fY + fHeight - 0.5f;

    ScreenVertex vertices[6] = {
        { x1, y1, 0.0f, 1.0f, dwColor, 0.0f, 0.0f },
        { x2, y1, 0.0f, 1.0f, dwColor, 0.0f, 0.0f },
        { x1, y2, 0.0f, 1.0f, dwColor, 0.0f, 0.0f },
        { x2, y1, 0.0f, 1.0f, dwColor, 0.0f, 0.0f },
        { x2, y2, 0.0f, 1.0f, dwColor, 0.0f, 0.0f },
        { x1, y2, 0.0f, 1.0f, dwColor, 0.0f, 0.0f },
    };

    // Clear leftover PS/VS (SkyGFX etc.) so MapUnderlay stays blue under sea fog
    m_pDevice->SetVertexShader(nullptr);
    m_pDevice->SetPixelShader(nullptr);
    m_pDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
    m_pDevice->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
    m_pDevice->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
    m_pDevice->SetRenderState(D3DRS_ZENABLE, FALSE);
    m_pDevice->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
    m_pDevice->SetRenderState(D3DRS_LIGHTING, FALSE);

    m_pDevice->SetTexture(0, nullptr);
    m_pDevice->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
    m_pDevice->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_DIFFUSE);
    m_pDevice->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
    m_pDevice->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_DIFFUSE);
    m_pDevice->SetFVF(SCREEN_FVF);
    m_pDevice->DrawPrimitiveUP(D3DPT_TRIANGLELIST, 2, vertices, sizeof(ScreenVertex));
}

void Draw::FillRectRaw(float fX, float fY, float fWidth, float fHeight, DWORD dwColor)
{
    if (!m_bInitialized || !m_pDevice)
        return;

    // Exact pixel coverage on RT — no -0.5 underfill (avoids α fringe on edges)
    const float x1 = fX;
    const float y1 = fY;
    const float x2 = fX + fWidth;
    const float y2 = fY + fHeight;

    ScreenVertex vertices[6] = {
        { x1, y1, 0.0f, 1.0f, dwColor, 0.0f, 0.0f },
        { x2, y1, 0.0f, 1.0f, dwColor, 0.0f, 0.0f },
        { x1, y2, 0.0f, 1.0f, dwColor, 0.0f, 0.0f },
        { x2, y1, 0.0f, 1.0f, dwColor, 0.0f, 0.0f },
        { x2, y2, 0.0f, 1.0f, dwColor, 0.0f, 0.0f },
        { x1, y2, 0.0f, 1.0f, dwColor, 0.0f, 0.0f },
    };

    m_pDevice->SetVertexShader(nullptr);
    m_pDevice->SetPixelShader(nullptr);
    m_pDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
    m_pDevice->SetRenderState(D3DRS_ZENABLE, FALSE);
    m_pDevice->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
    m_pDevice->SetRenderState(D3DRS_LIGHTING, FALSE);
    m_pDevice->SetRenderState(D3DRS_COLORWRITEENABLE,
                              D3DCOLORWRITEENABLE_RED | D3DCOLORWRITEENABLE_GREEN |
                              D3DCOLORWRITEENABLE_BLUE | D3DCOLORWRITEENABLE_ALPHA);
    m_pDevice->SetTexture(0, nullptr);
    m_pDevice->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
    m_pDevice->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_DIFFUSE);
    m_pDevice->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
    m_pDevice->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_DIFFUSE);
    m_pDevice->SetFVF(SCREEN_FVF);
    m_pDevice->DrawPrimitiveUP(D3DPT_TRIANGLELIST, 2, vertices, sizeof(ScreenVertex));
    m_pDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
}

void Draw::BlitCutout(float fX, float fY, float fWidth, float fHeight, float fU2, float fV2)
{
    if (!m_bInitialized || !m_pDevice || !m_pCutoutTex)
        return;

    // Half-texel inset so edge never samples cleared padding (α0 → bright fringe on α200)
    const float uInset = (m_nCutoutW > 0) ? (0.5f / static_cast<float>(m_nCutoutW)) : 0.0f;
    const float vInset = (m_nCutoutH > 0) ? (0.5f / static_cast<float>(m_nCutoutH)) : 0.0f;
    const float u1 = uInset;
    const float v1 = vInset;
    const float u2 = fU2 - uInset;
    const float v2 = fV2 - vInset;

    const float x1 = fX - 0.5f;
    const float y1 = fY - 0.5f;
    const float x2 = fX + fWidth - 0.5f;
    const float y2 = fY + fHeight - 0.5f;
    const DWORD dwColor = 0xFFFFFFFF;

    ScreenVertex vertices[6] = {
        { x1, y1, 0.0f, 1.0f, dwColor, u1, v1 },
        { x2, y1, 0.0f, 1.0f, dwColor, u2, v1 },
        { x1, y2, 0.0f, 1.0f, dwColor, u1, v2 },
        { x2, y1, 0.0f, 1.0f, dwColor, u2, v1 },
        { x2, y2, 0.0f, 1.0f, dwColor, u2, v2 },
        { x1, y2, 0.0f, 1.0f, dwColor, u1, v2 },
    };

    m_pDevice->SetVertexShader(nullptr);
    m_pDevice->SetPixelShader(nullptr);
    m_pDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
    m_pDevice->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
    m_pDevice->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
    m_pDevice->SetRenderState(D3DRS_ZENABLE, FALSE);
    m_pDevice->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
    m_pDevice->SetRenderState(D3DRS_LIGHTING, FALSE);

    // POINT — no soft edge blend with transparent padding
    m_pDevice->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_POINT);
    m_pDevice->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_POINT);
    m_pDevice->SetSamplerState(0, D3DSAMP_MIPFILTER, D3DTEXF_NONE);
    m_pDevice->SetSamplerState(0, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
    m_pDevice->SetSamplerState(0, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);

    m_pDevice->SetTexture(0, m_pCutoutTex);
    m_pDevice->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
    m_pDevice->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
    m_pDevice->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
    m_pDevice->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_MODULATE);
    m_pDevice->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
    m_pDevice->SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);
    m_pDevice->SetTextureStageState(1, D3DTSS_COLOROP, D3DTOP_DISABLE);
    m_pDevice->SetTextureStageState(1, D3DTSS_ALPHAOP, D3DTOP_DISABLE);
    m_pDevice->SetFVF(SCREEN_FVF);
    m_pDevice->DrawPrimitiveUP(D3DPT_TRIANGLELIST, 2, vertices, sizeof(ScreenVertex));
    m_pDevice->SetTexture(0, nullptr);
}

bool Draw::EnsureClipTarget(UINT width, UINT height)
{
    if (!m_pDevice || width < 1 || height < 1)
        return false;

    const UINT needW = (width + 15u) & ~15u;
    const UINT needH = (height + 15u) & ~15u;
    if (m_pClipTex && m_nClipTexW >= needW && m_nClipTexH >= needH)
        return true;

    if (m_pClipSurf) { m_pClipSurf->Release(); m_pClipSurf = nullptr; }
    if (m_pClipTex) { m_pClipTex->Release(); m_pClipTex = nullptr; }

    if (FAILED(m_pDevice->CreateTexture(needW, needH, 1, D3DUSAGE_RENDERTARGET, D3DFMT_A8R8G8B8,
                                        D3DPOOL_DEFAULT, &m_pClipTex, nullptr)))
        return false;
    if (FAILED(m_pClipTex->GetSurfaceLevel(0, &m_pClipSurf)))
    {
        m_pClipTex->Release();
        m_pClipTex = nullptr;
        return false;
    }

    m_nClipTexW = needW;
    m_nClipTexH = needH;
    return true;
}

bool Draw::BeginClipRT(float screenX, float screenY, float width, float height)
{
    if (!m_bInitialized || !m_pDevice || m_bClipActive)
        return false;
    if (width < 1.0f || height < 1.0f)
        return false;

    const UINT tw = static_cast<UINT>(width + 0.5f);
    const UINT th = static_cast<UINT>(height + 0.5f);
    if (!EnsureClipTarget(tw, th))
        return false;

    if (FAILED(m_pDevice->GetRenderTarget(0, &m_pClipSavedRT)))
        return false;
    m_pDevice->GetDepthStencilSurface(&m_pClipSavedDS);
    m_pDevice->GetViewport(&m_clipSavedVp);
    m_pClipSavedRwRT = RwD3D9RenderSurface;
    m_pClipSavedRwDS = RwD3D9DepthStencilSurface;

    // Small color RT cannot share fullscreen DS — same rule as radar RenderTarget.
    RwSetDepthStencil(nullptr);
    if (FAILED(m_pDevice->SetDepthStencilSurface(nullptr)))
    {
        m_pDevice->SetDepthStencilSurface(m_pClipSavedDS);
        if (m_pClipSavedDS) { m_pClipSavedDS->Release(); m_pClipSavedDS = nullptr; }
        m_pClipSavedRT->Release();
        m_pClipSavedRT = nullptr;
        m_pClipSavedRwRT = nullptr;
        m_pClipSavedRwDS = nullptr;
        return false;
    }
    if (!RwSetRenderTarget(m_pClipSurf) || FAILED(m_pDevice->SetRenderTarget(0, m_pClipSurf)))
    {
        RwSetDepthStencil(m_pClipSavedDS);
        m_pDevice->SetDepthStencilSurface(m_pClipSavedDS);
        if (m_pClipSavedDS) { m_pClipSavedDS->Release(); m_pClipSavedDS = nullptr; }
        m_pClipSavedRT->Release();
        m_pClipSavedRT = nullptr;
        m_pClipSavedRwRT = nullptr;
        m_pClipSavedRwDS = nullptr;
        return false;
    }
    RwD3D9RenderSurface = m_pClipSurf;
    RwD3D9DepthStencilSurface = nullptr;

    D3DVIEWPORT9 vp{};
    vp.X = 0;
    vp.Y = 0;
    vp.Width = tw;
    vp.Height = th;
    vp.MinZ = 0.0f;
    vp.MaxZ = 1.0f;
    m_pDevice->SetViewport(&vp);
    m_pDevice->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);
    m_pDevice->SetRenderState(D3DRS_ZENABLE, FALSE);
    m_pDevice->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
    m_pDevice->SetRenderState(D3DRS_STENCILENABLE, FALSE);
    m_pDevice->Clear(0, nullptr, D3DCLEAR_TARGET, 0x00000000, 1.0f, 0);

    m_clipScreenX = screenX;
    m_clipScreenY = screenY;
    m_clipW = width;
    m_clipH = height;
    m_bClipActive = true;
    return true;
}

void Draw::BlitClipToScreen()
{
    if (!m_pDevice || !m_pClipTex)
        return;

    const float u2 = m_clipW / static_cast<float>(m_nClipTexW);
    const float v2 = m_clipH / static_cast<float>(m_nClipTexH);
    const float uInset = (m_nClipTexW > 0) ? (0.5f / static_cast<float>(m_nClipTexW)) : 0.0f;
    const float vInset = (m_nClipTexH > 0) ? (0.5f / static_cast<float>(m_nClipTexH)) : 0.0f;

    const float x1 = m_clipScreenX - 0.5f;
    const float y1 = m_clipScreenY - 0.5f;
    const float x2 = m_clipScreenX + m_clipW - 0.5f;
    const float y2 = m_clipScreenY + m_clipH - 0.5f;
    const DWORD dwColor = 0xFFFFFFFF;

    ScreenVertex vertices[6] = {
        { x1, y1, 0.0f, 1.0f, dwColor, uInset, vInset },
        { x2, y1, 0.0f, 1.0f, dwColor, u2 - uInset, vInset },
        { x1, y2, 0.0f, 1.0f, dwColor, uInset, v2 - vInset },
        { x2, y1, 0.0f, 1.0f, dwColor, u2 - uInset, vInset },
        { x2, y2, 0.0f, 1.0f, dwColor, u2 - uInset, v2 - vInset },
        { x1, y2, 0.0f, 1.0f, dwColor, uInset, v2 - vInset },
    };

    m_pDevice->SetVertexShader(nullptr);
    m_pDevice->SetPixelShader(nullptr);
    m_pDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
    m_pDevice->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
    m_pDevice->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
    m_pDevice->SetRenderState(D3DRS_ZENABLE, FALSE);
    m_pDevice->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
    m_pDevice->SetRenderState(D3DRS_LIGHTING, FALSE);
    m_pDevice->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_POINT);
    m_pDevice->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_POINT);
    m_pDevice->SetSamplerState(0, D3DSAMP_MIPFILTER, D3DTEXF_NONE);
    m_pDevice->SetSamplerState(0, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
    m_pDevice->SetSamplerState(0, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);
    m_pDevice->SetTexture(0, m_pClipTex);
    m_pDevice->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
    m_pDevice->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
    m_pDevice->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
    m_pDevice->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_MODULATE);
    m_pDevice->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
    m_pDevice->SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);
    m_pDevice->SetFVF(SCREEN_FVF);
    m_pDevice->DrawPrimitiveUP(D3DPT_TRIANGLELIST, 2, vertices, sizeof(ScreenVertex));
    m_pDevice->SetTexture(0, nullptr);
}

void Draw::EndClipRT()
{
    if (!m_bClipActive || !m_pDevice)
        return;

    if (m_pClipSavedRT)
    {
        RwSetRenderTarget(m_pClipSavedRT);
        m_pDevice->SetRenderTarget(0, m_pClipSavedRT);
        m_pClipSavedRT->Release();
        m_pClipSavedRT = nullptr;
    }
    if (m_pClipSavedDS)
    {
        RwSetDepthStencil(m_pClipSavedDS);
        m_pDevice->SetDepthStencilSurface(m_pClipSavedDS);
        m_pClipSavedDS->Release();
        m_pClipSavedDS = nullptr;
    }
    else
    {
        RwSetDepthStencil(m_pClipSavedRwDS);
        m_pDevice->SetDepthStencilSurface(m_pClipSavedRwDS);
    }
    RwD3D9RenderSurface = m_pClipSavedRwRT;
    RwD3D9DepthStencilSurface = m_pClipSavedRwDS;
    m_pClipSavedRwRT = nullptr;
    m_pClipSavedRwDS = nullptr;
    m_pDevice->SetViewport(&m_clipSavedVp);

    BlitClipToScreen();
    m_bClipActive = false;
}

void Draw::DrawCircle(float fCenterX, float fCenterY, float fRadius, DWORD dwColor)
{
    if (!m_bInitialized || !m_pDevice || fRadius < 0.5f)
        return;

    static constexpr int kSegs = 48;
    ScreenVertex verts[kSegs + 2];
    verts[0] = { fCenterX, fCenterY, 0.0f, 1.0f, dwColor, 0.0f, 0.0f };
    for (int i = 0; i <= kSegs; ++i)
    {
        const float a = (3.14159265f * 2.0f) * (static_cast<float>(i) / static_cast<float>(kSegs));
        verts[i + 1] = {
            fCenterX + cosf(a) * fRadius,
            fCenterY + sinf(a) * fRadius,
            0.0f, 1.0f, dwColor, 0.0f, 0.0f
        };
    }

    m_pDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
    m_pDevice->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
    m_pDevice->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
    m_pDevice->SetRenderState(D3DRS_ZENABLE, FALSE);
    m_pDevice->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
    m_pDevice->SetRenderState(D3DRS_LIGHTING, FALSE);
    m_pDevice->SetTexture(0, nullptr);
    m_pDevice->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
    m_pDevice->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_DIFFUSE);
    m_pDevice->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
    m_pDevice->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_DIFFUSE);
    m_pDevice->SetFVF(SCREEN_FVF);
    m_pDevice->DrawPrimitiveUP(D3DPT_TRIANGLEFAN, kSegs, verts, sizeof(ScreenVertex));
}

void Draw::DrawCircleAA(float fCenterX, float fCenterY, float fRadius, DWORD dwColor)
{
    if (!m_bInitialized || !m_pDevice || fRadius < 0.5f)
        return;

    const DWORD rgb = dwColor & 0x00FFFFFF;
    const int baseA = static_cast<int>((dwColor >> 24) & 0xFF);
    // Soft fringe (outer → inner), then solid fill
    const float fringes[] = { 1.75f, 1.15f, 0.55f };
    const float alphas[]  = { 0.22f, 0.45f, 0.75f };
    for (int i = 0; i < 3; ++i)
    {
        const int a = static_cast<int>(baseA * alphas[i] + 0.5f);
        DrawCircle(fCenterX, fCenterY, fRadius + fringes[i], rgb | (static_cast<DWORD>(a) << 24));
    }
    DrawCircle(fCenterX, fCenterY, fRadius, dwColor);
}

void Draw::DrawQuarterDisk(float fCenterX, float fCenterY, float fRadius, float fAngle0, float fAngle1,
                           DWORD dwColor, int segments)
{
    if (!m_bInitialized || !m_pDevice || fRadius < 0.5f)
        return;
    if (segments < 4)
        segments = 4;
    if (segments > 48)
        segments = 48;

    ScreenVertex verts[50];
    verts[0] = { fCenterX, fCenterY, 0.0f, 1.0f, dwColor, 0.0f, 0.0f };
    for (int i = 0; i <= segments; ++i)
    {
        const float u = static_cast<float>(i) / static_cast<float>(segments);
        const float a = fAngle0 + (fAngle1 - fAngle0) * u;
        verts[i + 1] = {
            fCenterX + cosf(a) * fRadius,
            fCenterY + sinf(a) * fRadius,
            0.0f, 1.0f, dwColor, 0.0f, 0.0f
        };
    }

    m_pDevice->SetVertexShader(nullptr);
    m_pDevice->SetPixelShader(nullptr);
    m_pDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
    m_pDevice->SetRenderState(D3DRS_ZENABLE, FALSE);
    m_pDevice->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
    m_pDevice->SetRenderState(D3DRS_LIGHTING, FALSE);
    m_pDevice->SetRenderState(D3DRS_COLORWRITEENABLE,
                              D3DCOLORWRITEENABLE_RED | D3DCOLORWRITEENABLE_GREEN |
                              D3DCOLORWRITEENABLE_BLUE | D3DCOLORWRITEENABLE_ALPHA);
    m_pDevice->SetTexture(0, nullptr);
    m_pDevice->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
    m_pDevice->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_DIFFUSE);
    m_pDevice->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
    m_pDevice->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_DIFFUSE);
    m_pDevice->SetFVF(SCREEN_FVF);
    m_pDevice->DrawPrimitiveUP(D3DPT_TRIANGLEFAN, segments, verts, sizeof(ScreenVertex));
    m_pDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
}

void Draw::DrawThickArc(float fCenterX, float fCenterY, float fRadius, float fAngle0, float fAngle1,
                        float fThickness, DWORD dwColor, int segments)
{
    if (!m_bInitialized || !m_pDevice || fRadius < 0.5f || fThickness < 0.5f)
        return;

    if (segments < 8)
        segments = 8;
    if (segments > 128)
        segments = 128;

    const float half = fThickness * 0.5f;
    // Triangle strip: 2 verts per sample
    ScreenVertex verts[129 * 2];
    const int n = segments;

    for (int i = 0; i <= n; ++i)
    {
        const float u = static_cast<float>(i) / static_cast<float>(n);
        const float a = fAngle0 + (fAngle1 - fAngle0) * u;
        const float ca = cosf(a);
        const float sa = sinf(a);
        // Path point on centerline
        const float px = fCenterX + ca * fRadius;
        const float py = fCenterY + sa * fRadius;
        // Radial normal (outward from arc center) — half-width along radius
        verts[i * 2] = {
            px - ca * half, py - sa * half, 0.0f, 1.0f, dwColor, 0.0f, 0.0f
        };
        verts[i * 2 + 1] = {
            px + ca * half, py + sa * half, 0.0f, 1.0f, dwColor, 0.0f, 0.0f
        };
    }

    m_pDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
    m_pDevice->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
    m_pDevice->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
    m_pDevice->SetRenderState(D3DRS_ZENABLE, FALSE);
    m_pDevice->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
    m_pDevice->SetRenderState(D3DRS_LIGHTING, FALSE);
    m_pDevice->SetTexture(0, nullptr);
    m_pDevice->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
    m_pDevice->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_DIFFUSE);
    m_pDevice->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
    m_pDevice->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_DIFFUSE);
    m_pDevice->SetFVF(SCREEN_FVF);
    m_pDevice->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, n * 2, verts, sizeof(ScreenVertex));
}

void Draw::DrawThickLine(float fX0, float fY0, float fX1, float fY1, float fThickness, DWORD dwColor)
{
    if (!m_bInitialized || !m_pDevice || fThickness < 0.5f)
        return;

    float dx = fX1 - fX0;
    float dy = fY1 - fY0;
    const float len = sqrtf(dx * dx + dy * dy);
    if (len < 0.001f)
        return;

    dx /= len;
    dy /= len;
    const float half = fThickness * 0.5f;
    // Perpendicular (screen space)
    const float nx = -dy * half;
    const float ny = dx * half;

    ScreenVertex verts[4] = {
        { fX0 - nx, fY0 - ny, 0.0f, 1.0f, dwColor, 0.0f, 0.0f },
        { fX0 + nx, fY0 + ny, 0.0f, 1.0f, dwColor, 0.0f, 0.0f },
        { fX1 - nx, fY1 - ny, 0.0f, 1.0f, dwColor, 0.0f, 0.0f },
        { fX1 + nx, fY1 + ny, 0.0f, 1.0f, dwColor, 0.0f, 0.0f },
    };

    m_pDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
    m_pDevice->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
    m_pDevice->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
    m_pDevice->SetRenderState(D3DRS_ZENABLE, FALSE);
    m_pDevice->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
    m_pDevice->SetRenderState(D3DRS_LIGHTING, FALSE);
    m_pDevice->SetTexture(0, nullptr);
    m_pDevice->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
    m_pDevice->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_DIFFUSE);
    m_pDevice->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
    m_pDevice->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_DIFFUSE);
    m_pDevice->SetFVF(SCREEN_FVF);
    m_pDevice->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, verts, sizeof(ScreenVertex));
}

void Draw::DrawChevronSolid(float fCenterX, float fCenterY, float fSize, bool pointLeft, DWORD dwColor)
{
    if (!m_bInitialized || !m_pDevice || fSize < 0.5f)
        return;

    const float dir = pointLeft ? -1.0f : 1.0f;
    const float tipX = fCenterX + dir * fSize * 0.52f;
    const float backX = fCenterX - dir * fSize * 0.48f;
    const float topY = fCenterY - fSize;
    const float botY = fCenterY + fSize;
    const float t = fSize * 0.22f;

    auto fillArm = [&](float x0, float y0, float x1, float y1, ScreenVertex* out) {
        float dx = x1 - x0;
        float dy = y1 - y0;
        const float len = sqrtf(dx * dx + dy * dy);
        if (len < 0.001f)
        {
            for (int i = 0; i < 6; ++i)
                out[i] = { x0, y0, 0.0f, 1.0f, dwColor, 0.0f, 0.0f };
            return;
        }
        dx /= len;
        dy /= len;
        const float nx = -dy * t;
        const float ny = dx * t;
        out[0] = { x0 + nx, y0 + ny, 0.0f, 1.0f, dwColor, 0.0f, 0.0f };
        out[1] = { x0 - nx, y0 - ny, 0.0f, 1.0f, dwColor, 0.0f, 0.0f };
        out[2] = { x1 + nx, y1 + ny, 0.0f, 1.0f, dwColor, 0.0f, 0.0f };
        out[3] = { x0 - nx, y0 - ny, 0.0f, 1.0f, dwColor, 0.0f, 0.0f };
        out[4] = { x1 - nx, y1 - ny, 0.0f, 1.0f, dwColor, 0.0f, 0.0f };
        out[5] = { x1 + nx, y1 + ny, 0.0f, 1.0f, dwColor, 0.0f, 0.0f };
    };

    ScreenVertex band[12];
    fillArm(tipX, fCenterY, backX, topY, band);
    fillArm(tipX, fCenterY, backX, botY, band + 6);

    m_pDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
    m_pDevice->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
    m_pDevice->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
    m_pDevice->SetRenderState(D3DRS_ZENABLE, FALSE);
    m_pDevice->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
    m_pDevice->SetRenderState(D3DRS_LIGHTING, FALSE);
    m_pDevice->SetTexture(0, nullptr);
    m_pDevice->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
    m_pDevice->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_DIFFUSE);
    m_pDevice->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
    m_pDevice->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_DIFFUSE);
    m_pDevice->SetFVF(SCREEN_FVF);
    m_pDevice->DrawPrimitiveUP(D3DPT_TRIANGLELIST, 4, band, sizeof(ScreenVertex));
}

void Draw::DrawArrowAA(float fCenterX, float fCenterY, float fSize, bool pointLeft, DWORD dwColor)
{
    if (!m_bInitialized || !m_pDevice || fSize < 0.5f)
        return;

    const DWORD rgb = dwColor & 0x00FFFFFF;
    const int baseA = static_cast<int>((dwColor >> 24) & 0xFF);
    const float fringes[] = { 1.6f, 1.0f, 0.45f };
    const float alphas[]  = { 0.20f, 0.42f, 0.72f };
    for (int i = 0; i < 3; ++i)
    {
        const int a = static_cast<int>(baseA * alphas[i] + 0.5f);
        DrawChevronSolid(fCenterX, fCenterY, fSize + fringes[i], pointLeft,
                         rgb | (static_cast<DWORD>(a) << 24));
    }
    DrawChevronSolid(fCenterX, fCenterY, fSize, pointLeft, dwColor);
}

void Draw::DrawRectCutoutText(float fX, float fY, float fWidth, float fHeight, DWORD dwColor,
                              float fTextLeft, float fTextTop, float fTextRight, float fTextBottom,
                              const char* szText, DWORD dwFormat)
{
    auto fallback = [&]() {
        DrawRect(fX, fY, fWidth, fHeight, dwColor);
        if (szText)
            DrawString(fTextLeft, fTextTop, fTextRight, fTextBottom, 0xFFFFFFFF, szText, 1.0f, 1.0f, dwFormat, false);
    };

    if (!m_bInitialized || !m_pDevice || !m_pFont || !szText)
    {
        if (m_bInitialized && m_pDevice)
            fallback();
        return;
    }

    if (fWidth < 1.0f || fHeight < 1.0f)
        return;

    wchar_t wide[512];
    if (Utf8ToWide(szText, wide, 512) <= 0)
        return;

    const UINT tw = static_cast<UINT>(fWidth + 0.5f);
    const UINT th = static_cast<UINT>(fHeight + 0.5f);
    if (!EnsureCutoutTarget(tw, th))
    {
        fallback();
        return;
    }

    SavedFb fb;
    if (!BindOffscreen(m_pDevice, m_pMaskSurf, tw, th, &fb))
    {
        ReleaseSavedFb(fb);
        fallback();
        return;
    }

    RECT textRc = {
        static_cast<LONG>((fTextLeft - fX) + 0.5f),
        static_cast<LONG>((fTextTop - fY) + 0.5f),
        static_cast<LONG>((fTextRight - fX) + 0.5f),
        static_cast<LONG>((fTextBottom - fY) + 0.5f)
    };

    // --- Pass A: glyph mask (white letters, transparent bg) ---
    m_pDevice->Clear(0, nullptr, D3DCLEAR_TARGET, 0x00000000, 1.0f, 0);

    m_pDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
    m_pDevice->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
    m_pDevice->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
    m_pDevice->SetRenderState(D3DRS_SEPARATEALPHABLENDENABLE, FALSE);
    m_pDevice->SetRenderState(D3DRS_ZENABLE, FALSE);
    m_pFont->DrawTextW(nullptr, wide, -1, &textRc, dwFormat, 0xFFFFFFFF);

    // --- Pass B: panel color, then punch with mask ---
    if (!SetOffscreenColorRt(m_pDevice, m_pCutoutSurf, tw, th))
    {
        RestoreFb(m_pDevice, fb);
        fallback();
        return;
    }
    m_pDevice->Clear(0, nullptr, D3DCLEAR_TARGET, 0x00000000, 1.0f, 0);
    FillRectRaw(0.0f, 0.0f, static_cast<float>(tw), static_cast<float>(th), dwColor);

    // dest *= (1 - maskAlpha)  -- opaque glyph => transparent hole
    const float u2 = static_cast<float>(tw) / static_cast<float>(m_nCutoutW);
    const float v2 = static_cast<float>(th) / static_cast<float>(m_nCutoutH);
    const float x2 = static_cast<float>(tw) - 0.5f;
    const float y2 = static_cast<float>(th) - 0.5f;

    ScreenVertex punch[6] = {
        { -0.5f, -0.5f, 0.0f, 1.0f, 0xFFFFFFFF, 0.0f, 0.0f },
        { x2,    -0.5f, 0.0f, 1.0f, 0xFFFFFFFF, u2,   0.0f },
        { -0.5f, y2,    0.0f, 1.0f, 0xFFFFFFFF, 0.0f, v2   },
        { x2,    -0.5f, 0.0f, 1.0f, 0xFFFFFFFF, u2,   0.0f },
        { x2,    y2,    0.0f, 1.0f, 0xFFFFFFFF, u2,   v2   },
        { -0.5f, y2,    0.0f, 1.0f, 0xFFFFFFFF, 0.0f, v2   },
    };

    m_pDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
    m_pDevice->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_ZERO);
    m_pDevice->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
    m_pDevice->SetRenderState(D3DRS_SEPARATEALPHABLENDENABLE, TRUE);
    m_pDevice->SetRenderState(D3DRS_SRCBLENDALPHA, D3DBLEND_ZERO);
    m_pDevice->SetRenderState(D3DRS_DESTBLENDALPHA, D3DBLEND_INVSRCALPHA);
    m_pDevice->SetRenderState(D3DRS_ZENABLE, FALSE);
    m_pDevice->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
    m_pDevice->SetRenderState(D3DRS_LIGHTING, FALSE);

    m_pDevice->SetVertexShader(nullptr);
    m_pDevice->SetPixelShader(nullptr);
    m_pDevice->SetTexture(0, m_pMaskTex);
    m_pDevice->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_POINT);
    m_pDevice->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_POINT);
    m_pDevice->SetSamplerState(0, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
    m_pDevice->SetSamplerState(0, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);
    m_pDevice->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
    m_pDevice->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
    m_pDevice->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
    m_pDevice->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
    m_pDevice->SetFVF(SCREEN_FVF);
    m_pDevice->DrawPrimitiveUP(D3DPT_TRIANGLELIST, 2, punch, sizeof(ScreenVertex));
    m_pDevice->SetTexture(0, nullptr);
    m_pDevice->SetRenderState(D3DRS_SEPARATEALPHABLENDENABLE, FALSE);
    m_pDevice->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
    m_pDevice->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);

    RestoreFb(m_pDevice, fb);
    BlitCutout(fX, fY, fWidth, fHeight, u2, v2);
}

void Draw::DrawRectCutoutCycleValue(float fX, float fY, float fWidth, float fHeight, DWORD dwColor,
                                    float labelL, float labelR, const char* label,
                                    float leftL, float leftR, const char* leftArrow, ID3DXFont* leftFont,
                                    float valueL, float valueR, const char* valueText,
                                    float rightL, float rightR, const char* rightArrow, ID3DXFont* rightFont,
                                    float textTop, float textBottom, DWORD valueFormat)
{
    auto fallback = [&]() {
        DrawRect(fX, fY, fWidth, fHeight, dwColor);
        const DWORD fmtC = DT_CENTER | DT_VCENTER | DT_NOCLIP | DT_SINGLELINE;
        const DWORD fmtL = DT_LEFT | DT_VCENTER | DT_NOCLIP | DT_SINGLELINE;
        if (label)
            DrawString(labelL, textTop, labelR, textBottom, 0xFFFFFFFF, label, 1.0f, 1.0f, fmtL, false);
        if (leftArrow && leftFont)
            DrawString(leftL, textTop, leftR, textBottom, 0xFFFFFFFF, leftArrow, 1.0f, 1.0f, fmtC, false);
        if (valueText)
            DrawString(valueL, textTop, valueR, textBottom, 0xFFFFFFFF, valueText, 1.0f, 1.0f, valueFormat, false);
        if (rightArrow && rightFont)
            DrawString(rightL, textTop, rightR, textBottom, 0xFFFFFFFF, rightArrow, 1.0f, 1.0f, fmtC, false);
    };

    if (!m_bInitialized || !m_pDevice || !m_pFont)
    {
        if (m_bInitialized && m_pDevice)
            fallback();
        return;
    }
    if (fWidth < 1.0f || fHeight < 1.0f)
        return;

    ID3DXFont* pLeft = leftFont ? leftFont : m_pFont;
    ID3DXFont* pValue = m_pFont;
    ID3DXFont* pRight = rightFont ? rightFont : m_pFont;

    const UINT tw = static_cast<UINT>(fWidth + 0.5f);
    const UINT th = static_cast<UINT>(fHeight + 0.5f);
    if (!EnsureCutoutTarget(tw, th))
    {
        fallback();
        return;
    }

    SavedFb fb;
    if (!BindOffscreen(m_pDevice, m_pMaskSurf, tw, th, &fb))
    {
        ReleaseSavedFb(fb);
        fallback();
        return;
    }

    auto toLocal = [&](float screenL, float screenT, float screenR, float screenB) -> RECT {
        return {
            static_cast<LONG>((screenL - fX) + 0.5f),
            static_cast<LONG>((screenT - fY) + 0.5f),
            static_cast<LONG>((screenR - fX) + 0.5f),
            static_cast<LONG>((screenB - fY) + 0.5f)
        };
    };

    m_pDevice->Clear(0, nullptr, D3DCLEAR_TARGET, 0x00000000, 1.0f, 0);
    m_pDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
    m_pDevice->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
    m_pDevice->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
    m_pDevice->SetRenderState(D3DRS_SEPARATEALPHABLENDENABLE, FALSE);
    m_pDevice->SetRenderState(D3DRS_ZENABLE, FALSE);

    const DWORD fmtC = DT_CENTER | DT_VCENTER | DT_NOCLIP | DT_SINGLELINE;
    const DWORD fmtL = DT_LEFT | DT_VCENTER | DT_NOCLIP | DT_SINGLELINE;
    wchar_t wide[256];

    if (label && Utf8ToWide(label, wide, 256) > 0)
    {
        RECT rc = toLocal(labelL, textTop, labelR, textBottom);
        m_pFont->DrawTextW(nullptr, wide, -1, &rc, fmtL, 0xFFFFFFFF);
    }
    if (leftArrow && Utf8ToWide(leftArrow, wide, 256) > 0)
    {
        RECT rc = toLocal(leftL, textTop, leftR, textBottom);
        pLeft->DrawTextW(nullptr, wide, -1, &rc, fmtC, 0xFFFFFFFF);
    }
    if (valueText && Utf8ToWide(valueText, wide, 256) > 0)
    {
        RECT rc = toLocal(valueL, textTop, valueR, textBottom);
        pValue->DrawTextW(nullptr, wide, -1, &rc, valueFormat, 0xFFFFFFFF);
    }
    if (rightArrow && Utf8ToWide(rightArrow, wide, 256) > 0)
    {
        RECT rc = toLocal(rightL, textTop, rightR, textBottom);
        pRight->DrawTextW(nullptr, wide, -1, &rc, fmtC, 0xFFFFFFFF);
    }

    if (!SetOffscreenColorRt(m_pDevice, m_pCutoutSurf, tw, th))
    {
        RestoreFb(m_pDevice, fb);
        fallback();
        return;
    }
    m_pDevice->Clear(0, nullptr, D3DCLEAR_TARGET, 0x00000000, 1.0f, 0);
    FillRectRaw(0.0f, 0.0f, static_cast<float>(tw), static_cast<float>(th), dwColor);

    const float u2 = static_cast<float>(tw) / static_cast<float>(m_nCutoutW);
    const float v2 = static_cast<float>(th) / static_cast<float>(m_nCutoutH);
    const float x2 = static_cast<float>(tw) - 0.5f;
    const float y2 = static_cast<float>(th) - 0.5f;

    ScreenVertex punch[6] = {
        { -0.5f, -0.5f, 0.0f, 1.0f, 0xFFFFFFFF, 0.0f, 0.0f },
        { x2,    -0.5f, 0.0f, 1.0f, 0xFFFFFFFF, u2,   0.0f },
        { -0.5f, y2,    0.0f, 1.0f, 0xFFFFFFFF, 0.0f, v2   },
        { x2,    -0.5f, 0.0f, 1.0f, 0xFFFFFFFF, u2,   0.0f },
        { x2,    y2,    0.0f, 1.0f, 0xFFFFFFFF, u2,   v2   },
        { -0.5f, y2,    0.0f, 1.0f, 0xFFFFFFFF, 0.0f, v2   },
    };

    m_pDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
    m_pDevice->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_ZERO);
    m_pDevice->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
    m_pDevice->SetRenderState(D3DRS_SEPARATEALPHABLENDENABLE, TRUE);
    m_pDevice->SetRenderState(D3DRS_SRCBLENDALPHA, D3DBLEND_ZERO);
    m_pDevice->SetRenderState(D3DRS_DESTBLENDALPHA, D3DBLEND_INVSRCALPHA);
    m_pDevice->SetRenderState(D3DRS_ZENABLE, FALSE);
    m_pDevice->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
    m_pDevice->SetRenderState(D3DRS_LIGHTING, FALSE);
    m_pDevice->SetVertexShader(nullptr);
    m_pDevice->SetPixelShader(nullptr);
    m_pDevice->SetTexture(0, m_pMaskTex);
    m_pDevice->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_POINT);
    m_pDevice->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_POINT);
    m_pDevice->SetSamplerState(0, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
    m_pDevice->SetSamplerState(0, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);
    m_pDevice->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
    m_pDevice->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
    m_pDevice->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
    m_pDevice->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
    m_pDevice->SetFVF(SCREEN_FVF);
    m_pDevice->DrawPrimitiveUP(D3DPT_TRIANGLELIST, 2, punch, sizeof(ScreenVertex));
    m_pDevice->SetTexture(0, nullptr);
    m_pDevice->SetRenderState(D3DRS_SEPARATEALPHABLENDENABLE, FALSE);
    m_pDevice->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
    m_pDevice->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);

    RestoreFb(m_pDevice, fb);
    BlitCutout(fX, fY, fWidth, fHeight, u2, v2);
}

void Draw::DrawString(float fX, float fY, DWORD dwColor, float fScale, const char* szText)
{
    if (!szText)
        return;

    RECT rect = {
        static_cast<LONG>(fX + 0.5f),
        static_cast<LONG>(fY + 0.5f),
        static_cast<LONG>(fX + 4096.0f),
        static_cast<LONG>(fY + 4096.0f)
    };
    DrawStringInternal(rect, dwColor, szText, fScale, fScale, DT_LEFT | DT_TOP | DT_NOCLIP, false,
                       0xFF588942, 3.0f, 3.0f, m_pFont);
}

void Draw::DrawString(float fLeft, float fTop, float fRight, float fBottom, DWORD dwColor, const char* szText,
                      float fScaleX, float fScaleY, DWORD dwFormat, bool bOutline,
                      DWORD dwOutlineColor, float fOutlineOffsetX, float fOutlineOffsetY)
{
    if (!szText)
        return;

    RECT rect = {
        static_cast<LONG>(fLeft + 0.5f),
        static_cast<LONG>(fTop + 0.5f),
        static_cast<LONG>(fRight + 0.5f),
        static_cast<LONG>(fBottom + 0.5f)
    };
    DrawStringInternal(rect, dwColor, szText, fScaleX, fScaleY, dwFormat, bOutline,
                       dwOutlineColor, fOutlineOffsetX, fOutlineOffsetY, m_pFont);
}

void Draw::DrawStringHover(float fLeft, float fTop, float fRight, float fBottom, DWORD dwColor, const char* szText,
                           DWORD dwFormat)
{
    if (!szText || !m_pFontHover)
        return;

    RECT rect = {
        static_cast<LONG>(fLeft + 0.5f),
        static_cast<LONG>(fTop + 0.5f),
        static_cast<LONG>(fRight + 0.5f),
        static_cast<LONG>(fBottom + 0.5f)
    };
    DrawStringInternal(rect, dwColor, szText, 1.0f, 1.0f, dwFormat, false,
                       0xFF588942, 3.0f, 3.0f, m_pFontHover);
}

void Draw::DrawStringInternal(RECT& rect, DWORD dwColor, const char* szText, float fScaleX, float fScaleY,
                              DWORD dwFormat, bool bOutline, DWORD dwOutlineColor,
                              float fOutlineOffsetX, float fOutlineOffsetY, ID3DXFont* pFont)
{
    if (!m_bInitialized || !pFont || !m_pSprite || !szText)
        return;

    wchar_t wide[512];
    if (Utf8ToWide(szText, wide, 512) <= 0)
        return;

    // Always 1:1 — size changes come from a separately created font (no blur)
    (void)fScaleX;
    (void)fScaleY;

    D3DXMATRIX identity;
    D3DXMatrixIdentity(&identity);

    if (FAILED(m_pSprite->Begin(D3DXSPRITE_ALPHABLEND | D3DXSPRITE_SORT_TEXTURE)))
        return;

    m_pSprite->SetTransform(&identity);

    // GTA often leaves POINT sampling — force linear so font AA is visible
    m_pDevice->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
    m_pDevice->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
    m_pDevice->SetSamplerState(0, D3DSAMP_MIPFILTER, D3DTEXF_NONE);

    if (bOutline)
    {
        // Classic SA metal: single offset shadow (not a fat 3×3 flood)
        const LONG ox = static_cast<LONG>(fOutlineOffsetX + (fOutlineOffsetX >= 0.0f ? 0.5f : -0.5f));
        const LONG oy = static_cast<LONG>(fOutlineOffsetY + (fOutlineOffsetY >= 0.0f ? 0.5f : -0.5f));
        RECT shadow = rect;
        OffsetRect(&shadow, ox, oy);
        pFont->DrawTextW(m_pSprite, wide, -1, &shadow, dwFormat, dwOutlineColor);
    }

    if ((dwColor & 0xFF000000) != 0)
        pFont->DrawTextW(m_pSprite, wide, -1, &rect, dwFormat, dwColor);
    m_pSprite->End();
}

float Draw::GetTextWidth(const char* szText, float fScale) const
{
    if (!m_pFont || !szText)
        return 0.0f;

    wchar_t wide[512];
    if (Utf8ToWide(szText, wide, 512) <= 0)
        return 0.0f;

    RECT rect = { 0, 0, 0, 0 };
    m_pFont->DrawTextW(nullptr, wide, -1, &rect, DT_CALCRECT | DT_LEFT | DT_TOP | DT_NOCLIP, 0xFFFFFFFF);
    return static_cast<float>(rect.right - rect.left) * fScale;
}

float Draw::GetFontHeight(float fScale) const
{
    if (!m_pFont)
        return 0.0f;

    TEXTMETRICW tm{};
    m_pFont->GetTextMetricsW(&tm);
    return static_cast<float>(tm.tmHeight) * fScale;
}
