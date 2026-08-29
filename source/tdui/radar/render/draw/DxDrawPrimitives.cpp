/*****************************************************************************
 *
 *  PROJECT:     The-Definitive-UI
 *  FILE:        source/Radar/render/draw/DxDrawPrimitives.cpp
 *  PURPOSE:     D3D9 primitives with full render-state save / restore
 *
 *****************************************************************************/

#include "DxDrawPrimitives.h"
#include "ColorUtils.h"
#include "MathUtils.h"
#include "Config.h"
#include <cmath>

DxDrawPrimitives::DxDrawPrimitives(LPDIRECT3DDEVICE9 pDevice)
    : m_pDevice(pDevice)
    , m_pResources(nullptr)
    , m_bInitialized(false)
{
}

DxDrawPrimitives::~DxDrawPrimitives()
{
    Shutdown();
}

bool DxDrawPrimitives::Initialize()
{
    if (!m_pDevice)
        return false;
    m_bInitialized = true;
    return true;
}

bool DxDrawPrimitives::Initialize(const DrawResources* pResources)
{
    if (!m_pDevice || !pResources)
        return false;
    m_pResources   = pResources;
    m_bInitialized = true;
    return true;
}

void DxDrawPrimitives::Shutdown()
{
    m_pDevice      = nullptr;
    m_pResources   = nullptr;
    m_bInitialized = false;
}

void DxDrawPrimitives::Setup2DSpriteStates()
{
    if (!m_pDevice || !m_pResources)
        return;

    float w = (float)m_pResources->screenWidth;
    float h = (float)m_pResources->screenHeight;
    Setup2DSpriteStatesForSurface(w, h);
}

void DxDrawPrimitives::Setup2DSpriteStatesForSurface(float surfaceWidth, float surfaceHeight)
{
    if (!m_pDevice)
        return;

    float w = surfaceWidth;
    float h = surfaceHeight;
    if (w <= 0.0f || h <= 0.0f)
        return;

    D3DXMATRIX matWorld, matView, matProj;
    D3DXMatrixIdentity(&matWorld);
    D3DXMatrixIdentity(&matView);
    D3DXMatrixOrthoOffCenterLH(&matProj, 0.0f, w, h, 0.0f, 0.0f, 1.0f);
    m_pDevice->SetTransform(D3DTS_WORLD, &matWorld);
    m_pDevice->SetTransform(D3DTS_VIEW, &matView);
    m_pDevice->SetTransform(D3DTS_PROJECTION, &matProj);
    m_pDevice->SetRenderState(D3DRS_ZENABLE, FALSE);
    m_pDevice->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
    m_pDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
    m_pDevice->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
    m_pDevice->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
    m_pDevice->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
    m_pDevice->SetRenderState(D3DRS_STENCILENABLE, FALSE);
    m_pDevice->SetRenderState(D3DRS_FOGENABLE, FALSE);
    m_pDevice->SetRenderState(D3DRS_LIGHTING, FALSE);
    m_pDevice->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
    m_pDevice->SetRenderState(D3DRS_COLORWRITEENABLE,
                              D3DCOLORWRITEENABLE_RED | D3DCOLORWRITEENABLE_GREEN |
                              D3DCOLORWRITEENABLE_BLUE | D3DCOLORWRITEENABLE_ALPHA);
    m_pDevice->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);
}

void DxDrawPrimitives::CreateScreenQuad(ScreenVertex* vertices, float x, float y, float width, float height, DWORD color)
{
    float left = x, right = x + width, top = y, bottom = y + height;
    vertices[0] = { left, top, 0.0f, color, 0.0f, 0.0f };
    vertices[1] = { left, bottom, 0.0f, color, 0.0f, 1.0f };
    vertices[2] = { right, top, 0.0f, color, 1.0f, 0.0f };
    vertices[3] = { left, bottom, 0.0f, color, 0.0f, 1.0f };
    vertices[4] = { right, bottom, 0.0f, color, 1.0f, 1.0f };
    vertices[5] = { right, top, 0.0f, color, 1.0f, 0.0f };
}

RenderStates DxDrawPrimitives::SaveRenderStates()
{
    RenderStates states = {};
    if (!m_pDevice)
        return states;

    m_pDevice->GetFVF(&states.fvf);
    m_pDevice->GetTexture(0, &states.texture);
    m_pDevice->GetVertexDeclaration(&states.vertexDecl);
    m_pDevice->GetSamplerState(0, D3DSAMP_ADDRESSU, &states.samplerStateU);
    m_pDevice->GetSamplerState(0, D3DSAMP_ADDRESSV, &states.samplerStateV);
    m_pDevice->GetSamplerState(0, D3DSAMP_MINFILTER, &states.samplerMinFilter);
    m_pDevice->GetSamplerState(0, D3DSAMP_MAGFILTER, &states.samplerMagFilter);
    m_pDevice->GetSamplerState(0, D3DSAMP_MIPFILTER, &states.samplerMipFilter);
    m_pDevice->GetVertexShader(&states.vertexShader);
    m_pDevice->GetPixelShader(&states.pixelShader);
    m_pDevice->GetTextureStageState(0, D3DTSS_COLOROP, &states.colorOp);
    m_pDevice->GetTextureStageState(0, D3DTSS_COLORARG1, &states.colorArg1);
    m_pDevice->GetTextureStageState(0, D3DTSS_COLORARG2, &states.colorArg2);
    m_pDevice->GetTextureStageState(0, D3DTSS_ALPHAOP, &states.alphaOp);
    m_pDevice->GetTextureStageState(0, D3DTSS_ALPHAARG1, &states.alphaArg1);
    m_pDevice->GetTextureStageState(0, D3DTSS_ALPHAARG2, &states.alphaArg2);
    m_pDevice->GetTextureStageState(1, D3DTSS_COLOROP, &states.colorOp1);
    m_pDevice->GetTextureStageState(1, D3DTSS_ALPHAOP, &states.alphaOp1);
    m_pDevice->GetRenderState(D3DRS_ALPHABLENDENABLE, &states.alphaBlendEnable);
    m_pDevice->GetRenderState(D3DRS_SRCBLEND, &states.srcBlend);
    m_pDevice->GetRenderState(D3DRS_DESTBLEND, &states.destBlend);
    m_pDevice->GetRenderState(D3DRS_ALPHATESTENABLE, &states.alphaTestEnable);
    m_pDevice->GetRenderState(D3DRS_ALPHAREF, &states.alphaRef);
    m_pDevice->GetRenderState(D3DRS_ALPHAFUNC, &states.alphaFunc);
    m_pDevice->GetRenderState(D3DRS_ZENABLE, &states.zEnable);
    m_pDevice->GetRenderState(D3DRS_ZWRITEENABLE, &states.zWriteEnable);
    m_pDevice->GetRenderState(D3DRS_LIGHTING, &states.lighting);
    m_pDevice->GetRenderState(D3DRS_CULLMODE, &states.cullMode);
    m_pDevice->GetRenderState(D3DRS_FOGENABLE, &states.fogEnable);
    m_pDevice->GetRenderState(D3DRS_STENCILENABLE, &states.stencilEnable);
    m_pDevice->GetRenderState(D3DRS_SCISSORTESTENABLE, &states.scissorEnable);
    m_pDevice->GetRenderState(D3DRS_COLORWRITEENABLE, &states.colorWriteEnable);
    m_pDevice->GetRenderState(D3DRS_CLIPPING, &states.clipping);
    m_pDevice->GetScissorRect(&states.scissorRect);
    m_pDevice->GetTransform(D3DTS_WORLD, &states.worldMatrix);
    m_pDevice->GetTransform(D3DTS_VIEW, &states.viewMatrix);
    m_pDevice->GetTransform(D3DTS_PROJECTION, &states.projMatrix);
    return states;
}

void DxDrawPrimitives::RestoreRenderStates(const RenderStates& states)
{
    if (!m_pDevice)
        return;

    m_pDevice->SetVertexDeclaration(states.vertexDecl);
    m_pDevice->SetFVF(states.fvf);
    m_pDevice->SetTexture(0, states.texture);
    m_pDevice->SetSamplerState(0, D3DSAMP_ADDRESSU, states.samplerStateU);
    m_pDevice->SetSamplerState(0, D3DSAMP_ADDRESSV, states.samplerStateV);
    m_pDevice->SetSamplerState(0, D3DSAMP_MINFILTER, states.samplerMinFilter);
    m_pDevice->SetSamplerState(0, D3DSAMP_MAGFILTER, states.samplerMagFilter);
    m_pDevice->SetSamplerState(0, D3DSAMP_MIPFILTER, states.samplerMipFilter);
    m_pDevice->SetVertexShader(states.vertexShader);
    m_pDevice->SetPixelShader(states.pixelShader);
    m_pDevice->SetTextureStageState(0, D3DTSS_COLOROP, states.colorOp);
    m_pDevice->SetTextureStageState(0, D3DTSS_COLORARG1, states.colorArg1);
    m_pDevice->SetTextureStageState(0, D3DTSS_COLORARG2, states.colorArg2);
    m_pDevice->SetTextureStageState(0, D3DTSS_ALPHAOP, states.alphaOp);
    m_pDevice->SetTextureStageState(0, D3DTSS_ALPHAARG1, states.alphaArg1);
    m_pDevice->SetTextureStageState(0, D3DTSS_ALPHAARG2, states.alphaArg2);
    m_pDevice->SetTextureStageState(1, D3DTSS_COLOROP, states.colorOp1);
    m_pDevice->SetTextureStageState(1, D3DTSS_ALPHAOP, states.alphaOp1);
    m_pDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, states.alphaBlendEnable);
    m_pDevice->SetRenderState(D3DRS_SRCBLEND, states.srcBlend);
    m_pDevice->SetRenderState(D3DRS_DESTBLEND, states.destBlend);
    m_pDevice->SetRenderState(D3DRS_ALPHATESTENABLE, states.alphaTestEnable);
    m_pDevice->SetRenderState(D3DRS_ALPHAREF, states.alphaRef);
    m_pDevice->SetRenderState(D3DRS_ALPHAFUNC, states.alphaFunc);
    m_pDevice->SetRenderState(D3DRS_ZENABLE, states.zEnable);
    m_pDevice->SetRenderState(D3DRS_ZWRITEENABLE, states.zWriteEnable);
    m_pDevice->SetRenderState(D3DRS_LIGHTING, states.lighting);
    m_pDevice->SetRenderState(D3DRS_CULLMODE, states.cullMode);
    m_pDevice->SetRenderState(D3DRS_FOGENABLE, states.fogEnable);
    m_pDevice->SetRenderState(D3DRS_STENCILENABLE, states.stencilEnable);
    m_pDevice->SetRenderState(D3DRS_SCISSORTESTENABLE, states.scissorEnable);
    m_pDevice->SetRenderState(D3DRS_COLORWRITEENABLE, states.colorWriteEnable);
    m_pDevice->SetRenderState(D3DRS_CLIPPING, states.clipping);
    m_pDevice->SetScissorRect(&states.scissorRect);
    m_pDevice->SetTransform(D3DTS_WORLD, &states.worldMatrix);
    m_pDevice->SetTransform(D3DTS_VIEW, &states.viewMatrix);
    m_pDevice->SetTransform(D3DTS_PROJECTION, &states.projMatrix);

    if (states.texture)
        states.texture->Release();
    if (states.vertexDecl)
        states.vertexDecl->Release();
    if (states.vertexShader)
        states.vertexShader->Release();
    if (states.pixelShader)
        states.pixelShader->Release();
}

void DxDrawPrimitives::dxDrawCircleShader(float x, float y, float width, float height, LPDIRECT3DTEXTURE9 texture, DWORD color, float alpha)
{
    if (!m_pDevice || !m_pResources || !m_pResources->pTriangleEffect)
        return;

    HRESULT hr = m_pDevice->TestCooperativeLevel();
    if (FAILED(hr) && hr != D3DERR_DEVICENOTRESET)
        return;

    DWORD finalColor = color;
    if (alpha < 1.0f)
    {
        BYTE alphaByte = (BYTE)(alpha * 255.0f);
        finalColor = (color & 0x00FFFFFF) | (alphaByte << 24);
    }

    ScreenVertex vertices[6];
    CreateScreenQuad(vertices, x, y, width, height, finalColor);

    RenderStates saved = SaveRenderStates();
    Setup2DSpriteStates();

    float w = (float)m_pResources->screenWidth;
    float h = (float)m_pResources->screenHeight;
    D3DXMATRIX matWorld, matView, matProj, matWorldViewProj;
    D3DXMatrixIdentity(&matWorld);
    D3DXMatrixIdentity(&matView);
    D3DXMatrixOrthoOffCenterLH(&matProj, 0.0f, w, h, 0.0f, 0.0f, 1.0f);
    D3DXMatrixMultiply(&matWorldViewProj, &matWorld, &matView);
    D3DXMatrixMultiply(&matWorldViewProj, &matWorldViewProj, &matProj);

    LPD3DXEFFECT eff = m_pResources->pTriangleEffect;
    D3DXHANDLE hWorldViewProj = eff->GetParameterByName(nullptr, "WorldViewProj");
    if (hWorldViewProj)
        eff->SetMatrix(hWorldViewProj, &matWorldViewProj);

    LPDIRECT3DTEXTURE9 textureToUse = texture ? texture : m_pResources->pCircleTexture;
    D3DXHANDLE hTexture = eff->GetParameterByName(nullptr, "sTexture");
    if (hTexture && textureToUse)
        eff->SetTexture(hTexture, textureToUse);

    const char* techniqueName = m_pResources->radarShapeCircle ? "Circle" : "SimpleTexture";
    D3DXHANDLE hTechnique = eff->GetTechniqueByName(techniqueName);
    if (hTechnique)
    {
        eff->SetTechnique(hTechnique);
        UINT numPasses = 0;
        eff->Begin(&numPasses, 0);
        for (UINT pass = 0; pass < numPasses; ++pass)
        {
            eff->BeginPass(pass);
            m_pDevice->SetFVF(D3DFVF_XYZ | D3DFVF_DIFFUSE | D3DFVF_TEX1);
            m_pDevice->SetStreamSource(0, nullptr, 0, 0);
            m_pDevice->DrawPrimitiveUP(D3DPT_TRIANGLELIST, 2, vertices, sizeof(ScreenVertex));
            eff->EndPass();
        }
        eff->End();
    }

    RestoreRenderStates(saved);
}

void DxDrawPrimitives::dxDrawBorderShader(float x, float y, float width, float height, DWORD color, float alpha, float borderThicknessPixels)
{
    if (!m_pDevice || !m_pResources || !m_pResources->pBorderEffect)
        return;

    HRESULT hr = m_pDevice->TestCooperativeLevel();
    if (FAILED(hr) && hr != D3DERR_DEVICENOTRESET)
        return;

    DWORD finalColor = color;
    if (alpha < 1.0f)
    {
        BYTE alphaByte = (BYTE)(alpha * 255.0f);
        finalColor = (color & 0x00FFFFFF) | (alphaByte << 24);
    }

    ScreenVertex vertices[6];
    CreateScreenQuad(vertices, x, y, width, height, finalColor);

    RenderStates saved = SaveRenderStates();
    Setup2DSpriteStates();

    float w = (float)m_pResources->screenWidth;
    float h = (float)m_pResources->screenHeight;
    D3DXMATRIX matWorld, matView, matProj, matWorldViewProj;
    D3DXMatrixIdentity(&matWorld);
    D3DXMatrixIdentity(&matView);
    D3DXMatrixOrthoOffCenterLH(&matProj, 0.0f, w, h, 0.0f, 0.0f, 1.0f);
    D3DXMatrixMultiply(&matWorldViewProj, &matWorld, &matView);
    D3DXMatrixMultiply(&matWorldViewProj, &matWorldViewProj, &matProj);

    LPD3DXEFFECT eff = m_pResources->pBorderEffect;
    D3DXHANDLE hWorldViewProj = eff->GetParameterByName(nullptr, "WorldViewProj");
    if (hWorldViewProj)
        eff->SetMatrix(hWorldViewProj, &matWorldViewProj);

    D3DXHANDLE hBorderSize = eff->GetParameterByName(nullptr, "BorderSize");
    if (hBorderSize)
    {
        D3DXVECTOR4 borderSizeVec((float)width, (float)height, 0.0f, 0.0f);
        eff->SetVector(hBorderSize, &borderSizeVec);
    }
    float thickness = borderThicknessPixels > 0.0f ? borderThicknessPixels : (width < height ? width : height) * 0.01f;
    D3DXHANDLE hBorderThickness = eff->GetParameterByName(nullptr, "BorderThicknessPixels");
    if (hBorderThickness)
        eff->SetFloat(hBorderThickness, thickness);

    const char* borderTechniqueName = m_pResources->borderShapeCircle ? "Border" : "SquareBorder";
    D3DXHANDLE hBorderTechnique = eff->GetTechniqueByName(borderTechniqueName);
    if (hBorderTechnique)
    {
        eff->SetTechnique(hBorderTechnique);
        UINT numPasses = 0;
        eff->Begin(&numPasses, 0);
        for (UINT pass = 0; pass < numPasses; ++pass)
        {
            eff->BeginPass(pass);
            m_pDevice->SetFVF(D3DFVF_XYZ | D3DFVF_DIFFUSE | D3DFVF_TEX1);
            m_pDevice->SetStreamSource(0, nullptr, 0, 0);
            m_pDevice->DrawPrimitiveUP(D3DPT_TRIANGLELIST, 2, vertices, sizeof(ScreenVertex));
            eff->EndPass();
        }
        eff->End();
    }

    RestoreRenderStates(saved);
}

void DxDrawPrimitives::dxDrawImage2D(float x, float y, float width, float height, LPDIRECT3DTEXTURE9 texture, DWORD color)
{
    if (!m_pDevice || !texture)
        return;

    HRESULT hr = m_pDevice->TestCooperativeLevel();
    if (FAILED(hr) && hr != D3DERR_DEVICENOTRESET)
        return;

    RenderStates saved = SaveRenderStates();
    Setup2DSpriteStates();

    m_pDevice->SetTexture(0, texture);
    m_pDevice->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
    m_pDevice->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
    m_pDevice->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
    m_pDevice->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_MODULATE);
    m_pDevice->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
    m_pDevice->SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);
    m_pDevice->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
    m_pDevice->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);

    ScreenVertex vertices[6];
    CreateScreenQuad(vertices, x, y, width, height, color);

    m_pDevice->SetFVF(D3DFVF_XYZ | D3DFVF_DIFFUSE | D3DFVF_TEX1);
    m_pDevice->DrawPrimitiveUP(D3DPT_TRIANGLELIST, 2, vertices, sizeof(ScreenVertex));

    RestoreRenderStates(saved);
}

void DxDrawPrimitives::dxDrawImage2DRotated(float x, float y, float width, float height, LPDIRECT3DTEXTURE9 texture, float rotationAngle, DWORD color)
{
    if (!m_pDevice || !texture)
        return;

    HRESULT hr = m_pDevice->TestCooperativeLevel();
    if (FAILED(hr) && hr != D3DERR_DEVICENOTRESET)
        return;

    RenderStates saved = SaveRenderStates();
    Setup2DSpriteStates();

    m_pDevice->SetTexture(0, texture);
    m_pDevice->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
    m_pDevice->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
    m_pDevice->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
    m_pDevice->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_MODULATE);
    m_pDevice->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
    m_pDevice->SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);
    m_pDevice->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
    m_pDevice->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);

    float centerX = x + width * 0.5f;
    float centerY = y + height * 0.5f;
    D3DXMATRIX matRotation, matTranslateToCenter, matTranslateBack, matWorld;
    D3DXMatrixRotationZ(&matRotation, rotationAngle);
    D3DXMatrixTranslation(&matTranslateToCenter, -centerX, -centerY, 0.0f);
    D3DXMatrixTranslation(&matTranslateBack, centerX, centerY, 0.0f);
    D3DXMatrixMultiply(&matWorld, &matTranslateToCenter, &matRotation);
    D3DXMatrixMultiply(&matWorld, &matWorld, &matTranslateBack);
    m_pDevice->SetTransform(D3DTS_WORLD, &matWorld);

    ScreenVertex vertices[6];
    CreateScreenQuad(vertices, x, y, width, height, color);

    m_pDevice->SetFVF(D3DFVF_XYZ | D3DFVF_DIFFUSE | D3DFVF_TEX1);
    m_pDevice->DrawPrimitiveUP(D3DPT_TRIANGLELIST, 2, vertices, sizeof(ScreenVertex));

    RestoreRenderStates(saved);
}

void DxDrawPrimitives::dxDrawImage2DRotatedSurface(float x, float y, float width, float height,
                                                   LPDIRECT3DTEXTURE9 texture, float rotationAngle,
                                                   DWORD color, float surfaceWidth, float surfaceHeight)
{
    if (!m_pDevice || !texture || surfaceWidth < 1.0f || surfaceHeight < 1.0f)
        return;

    HRESULT hr = m_pDevice->TestCooperativeLevel();
    if (FAILED(hr) && hr != D3DERR_DEVICENOTRESET)
        return;

    RenderStates saved = SaveRenderStates();
    Setup2DSpriteStatesForSurface(surfaceWidth, surfaceHeight);

    m_pDevice->SetTexture(0, texture);
    m_pDevice->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
    m_pDevice->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
    m_pDevice->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
    m_pDevice->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_MODULATE);
    m_pDevice->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
    m_pDevice->SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);
    m_pDevice->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
    m_pDevice->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);

    float centerX = x + width * 0.5f;
    float centerY = y + height * 0.5f;
    D3DXMATRIX matRotation, matTranslateToCenter, matTranslateBack, matWorld;
    D3DXMatrixRotationZ(&matRotation, rotationAngle);
    D3DXMatrixTranslation(&matTranslateToCenter, -centerX, -centerY, 0.0f);
    D3DXMatrixTranslation(&matTranslateBack, centerX, centerY, 0.0f);
    D3DXMatrixMultiply(&matWorld, &matTranslateToCenter, &matRotation);
    D3DXMatrixMultiply(&matWorld, &matWorld, &matTranslateBack);
    m_pDevice->SetTransform(D3DTS_WORLD, &matWorld);

    ScreenVertex vertices[6];
    CreateScreenQuad(vertices, x, y, width, height, color);

    m_pDevice->SetFVF(D3DFVF_XYZ | D3DFVF_DIFFUSE | D3DFVF_TEX1);
    m_pDevice->DrawPrimitiveUP(D3DPT_TRIANGLELIST, 2, vertices, sizeof(ScreenVertex));

    RestoreRenderStates(saved);
}

void DxDrawPrimitives::dxDrawGTAIndicatorBlipSurface(float screenX, float screenY, float size, DWORD color,
                                                     eHeightIndicatorType type, float surfaceWidth, float surfaceHeight)
{
    if (!m_pDevice || surfaceWidth < 1.0f || surfaceHeight < 1.0f)
        return;

    struct SimpleVertex { float x, y, z; DWORD color; };
    static const struct { float dx[4], dy[4]; int n; D3DPRIMITIVETYPE prim; UINT count; } shapes[] = {
        {{ -1, 1, 0 }, { -1, -1, 1 }, 3, D3DPT_TRIANGLELIST, 1 },
        {{ 0, -1, 1 }, { -1, 1, 1 }, 3, D3DPT_TRIANGLELIST, 1 },
        {{ -1, 1, -1, 1 }, { -1, -1, 1, 1 }, 4, D3DPT_TRIANGLESTRIP, 2 }
    };

    RenderStates saved = SaveRenderStates();
    Setup2DSpriteStatesForSurface(surfaceWidth, surfaceHeight);
    m_pDevice->SetVertexShader(nullptr);
    m_pDevice->SetPixelShader(nullptr);
    m_pDevice->SetTexture(0, nullptr);
    m_pDevice->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
    m_pDevice->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_DIFFUSE);
    m_pDevice->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
    m_pDevice->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_DIFFUSE);
    m_pDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
    m_pDevice->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
    m_pDevice->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);

    const float borderThickness = 3.0f;
    const BYTE alpha = (color >> 24) & 0xFF;
    const DWORD borderColor = tocolor(0, 0, 0, alpha);
    const auto& s = shapes[type];
    SimpleVertex v[4];

    for (int pass = 0; pass < 2; pass++)
    {
        float halfSize = (pass == 0 ? size + borderThickness * 2.0f : size) * 0.5f;
        DWORD curColor = pass == 0 ? borderColor : color;
        for (int i = 0; i < s.n; i++)
        {
            v[i].x     = screenX + s.dx[i] * halfSize;
            v[i].y     = screenY + s.dy[i] * halfSize;
            v[i].z     = 0.0f;
            v[i].color = curColor;
        }
        m_pDevice->SetFVF(D3DFVF_XYZ | D3DFVF_DIFFUSE);
        m_pDevice->DrawPrimitiveUP(s.prim, s.count, v, sizeof(SimpleVertex));
    }

    RestoreRenderStates(saved);
}

void DxDrawPrimitives::dxDrawRectangle(float x, float y, float width, float height, DWORD color)
{
    if (!m_pDevice)
        return;

    HRESULT hr = m_pDevice->TestCooperativeLevel();
    if (FAILED(hr) && hr != D3DERR_DEVICENOTRESET)
        return;

    RenderStates saved = SaveRenderStates();
    Setup2DSpriteStates();

    m_pDevice->SetTexture(0, nullptr);
    m_pDevice->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
    m_pDevice->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_DIFFUSE);
    m_pDevice->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
    m_pDevice->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_DIFFUSE);

    struct SimpleVertex { float x, y, z; DWORD color; };
    SimpleVertex vertices[6];
    float left = x, right = x + width, top = y, bottom = y + height;
    vertices[0] = { left, top, 0.0f, color };
    vertices[1] = { left, bottom, 0.0f, color };
    vertices[2] = { right, top, 0.0f, color };
    vertices[3] = { left, bottom, 0.0f, color };
    vertices[4] = { right, bottom, 0.0f, color };
    vertices[5] = { right, top, 0.0f, color };

    m_pDevice->SetFVF(D3DFVF_XYZ | D3DFVF_DIFFUSE);
    m_pDevice->DrawPrimitiveUP(D3DPT_TRIANGLELIST, 2, vertices, sizeof(SimpleVertex));

    RestoreRenderStates(saved);
}

void DxDrawPrimitives::dxDrawGTAIndicatorBlip(float screenX, float screenY, float size, DWORD color, eHeightIndicatorType type)
{
    if (!m_pDevice)
        return;

    struct SimpleVertex { float x, y, z; DWORD color; };
    static const struct { float dx[4], dy[4]; int n; D3DPRIMITIVETYPE prim; UINT count; } shapes[] = {
        {{ -1, 1, 0 }, { -1, -1, 1 }, 3, D3DPT_TRIANGLELIST, 1 },
        {{ 0, -1, 1 }, { -1, 1, 1 }, 3, D3DPT_TRIANGLELIST, 1 },
        {{ -1, 1, -1, 1 }, { -1, -1, 1, 1 }, 4, D3DPT_TRIANGLESTRIP, 2 }
    };

    RenderStates saved = SaveRenderStates();
    Setup2DSpriteStates();
    m_pDevice->SetVertexShader(nullptr);
    m_pDevice->SetPixelShader(nullptr);
    m_pDevice->SetTexture(0, nullptr);
    m_pDevice->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
    m_pDevice->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_DIFFUSE);
    m_pDevice->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
    m_pDevice->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_DIFFUSE);
    m_pDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
    m_pDevice->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
    m_pDevice->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);

    const float borderThickness = 3.0f;
    const BYTE alpha = (color >> 24) & 0xFF;
    const DWORD borderColor = tocolor(0, 0, 0, alpha);
    const auto& s = shapes[type];
    SimpleVertex v[4];

    for (int pass = 0; pass < 2; pass++)
    {
        float halfSize = (pass == 0 ? size + borderThickness * 2.0f : size) * 0.5f;
        DWORD curColor = pass == 0 ? borderColor : color;
        for (int i = 0; i < s.n; i++)
        {
            v[i].x     = screenX + s.dx[i] * halfSize;
            v[i].y     = screenY + s.dy[i] * halfSize;
            v[i].z     = 0.0f;
            v[i].color = curColor;
        }
        m_pDevice->SetFVF(D3DFVF_XYZ | D3DFVF_DIFFUSE);
        m_pDevice->DrawPrimitiveUP(s.prim, s.count, v, sizeof(SimpleVertex));
    }

    RestoreRenderStates(saved);
}

void DxDrawPrimitives::dxDrawGreenSquareFill(float x, float y, float width, float height, LPDIRECT3DTEXTURE9 texture, DWORD color, float fillLevel)
{
    if (!m_pDevice || !m_pResources || !m_pResources->pGreenSquareEffect)
        return;

    HRESULT hr = m_pDevice->TestCooperativeLevel();
    if (FAILED(hr) && hr != D3DERR_DEVICENOTRESET)
        return;

    ScreenVertex vertices[6];
    float left = x, right = x + width, top = y, bottom = y + height;
    vertices[0] = { left, top, 0.0f, color, 0.0f, 0.0f };
    vertices[1] = { left, bottom, 0.0f, color, 0.0f, 1.0f };
    vertices[2] = { right, top, 0.0f, color, 1.0f, 0.0f };
    vertices[3] = { left, bottom, 0.0f, color, 0.0f, 1.0f };
    vertices[4] = { right, bottom, 0.0f, color, 1.0f, 1.0f };
    vertices[5] = { right, top, 0.0f, color, 1.0f, 0.0f };

    RenderStates saved = SaveRenderStates();
    Setup2DSpriteStates();

    DWORD oldFVF;
    LPDIRECT3DVERTEXBUFFER9 oldVB = nullptr;
    UINT oldStreamOffset, oldStreamStride;

    m_pDevice->GetFVF(&oldFVF);
    m_pDevice->GetStreamSource(0, &oldVB, &oldStreamOffset, &oldStreamStride);

    float w = (float)m_pResources->screenWidth;
    float h = (float)m_pResources->screenHeight;
    D3DXMATRIX matWorld, matView, matProj, matWorldViewProj;
    D3DXMatrixIdentity(&matWorld);
    D3DXMatrixIdentity(&matView);
    D3DXMatrixOrthoOffCenterLH(&matProj, 0.0f, w, h, 0.0f, 0.0f, 1.0f);
    D3DXMatrixMultiply(&matWorldViewProj, &matWorld, &matView);
    D3DXMatrixMultiply(&matWorldViewProj, &matWorldViewProj, &matProj);

    LPD3DXEFFECT eff = m_pResources->pGreenSquareEffect;
    D3DXHANDLE hWorldViewProj = eff->GetParameterByName(nullptr, "WorldViewProj");
    if (hWorldViewProj)
        eff->SetMatrix(hWorldViewProj, &matWorldViewProj);
    D3DXHANDLE hFillLevel = eff->GetParameterByName(nullptr, "FillLevel");
    if (hFillLevel)
        eff->SetFloat(hFillLevel, fillLevel);
    D3DXHANDLE hTexture = eff->GetParameterByName(nullptr, "sTexture");
    if (hTexture && texture)
        eff->SetTexture(hTexture, texture);

    D3DXHANDLE hTechnique = eff->GetTechniqueByName("GreenSquareFill");
    if (hTechnique)
    {
        eff->SetTechnique(hTechnique);
        UINT numPasses = 0;
        eff->Begin(&numPasses, 0);
        for (UINT pass = 0; pass < numPasses; ++pass)
        {
            eff->BeginPass(pass);
            m_pDevice->SetFVF(D3DFVF_XYZ | D3DFVF_DIFFUSE | D3DFVF_TEX1);
            m_pDevice->SetStreamSource(0, nullptr, 0, 0);
            m_pDevice->DrawPrimitiveUP(D3DPT_TRIANGLELIST, 2, vertices, sizeof(ScreenVertex));
            eff->EndPass();
        }
        eff->End();
    }

    if (hTexture)
        eff->SetTexture(hTexture, nullptr);
    m_pDevice->SetTexture(0, nullptr);
    m_pDevice->SetFVF(oldFVF);
    m_pDevice->SetStreamSource(0, oldVB, oldStreamOffset, oldStreamStride);
    if (oldVB)
        oldVB->Release();
    RestoreRenderStates(saved);
}

void DxDrawPrimitives::dxDrawImage3D(const D3DXVECTOR3& elementPos, const D3DXVECTOR3& elementRot, const D3DXVECTOR2& elementSize,
                                     const D3DXVECTOR3& cameraPos, const D3DXVECTOR3& cameraRot,
                                     float fov, float nearPlane, float farPlane,
                                     LPDIRECT3DTEXTURE9 texture, DWORD color)
{
    if (!m_pDevice || !m_pResources || !m_pResources->pImage3DEffect || !texture)
        return;

    HRESULT hr = m_pDevice->TestCooperativeLevel();
    if (FAILED(hr) && hr != D3DERR_DEVICENOTRESET)
        return;

    struct Image3DVertex { float x, y, z; DWORD color; float u, v; };

    float renderWidth  = (m_pResources->renderTargetWidth > 0)  ? (float)m_pResources->renderTargetWidth  : (float)m_pResources->screenWidth;
    float renderHeight = (m_pResources->renderTargetHeight > 0) ? (float)m_pResources->renderTargetHeight : (float)m_pResources->screenHeight;

    Image3DVertex vertices[6] = {
        { 0.0f, 0.0f, 0.0f, color, 0.0f, 0.0f },
        { 0.0f, renderHeight, 0.0f, color, 0.0f, 1.0f },
        { renderWidth, 0.0f, 0.0f, color, 1.0f, 0.0f },
        { 0.0f, renderHeight, 0.0f, color, 0.0f, 1.0f },
        { renderWidth, renderHeight, 0.0f, color, 1.0f, 1.0f },
        { renderWidth, 0.0f, 0.0f, color, 1.0f, 0.0f }
    };

    float pos[3] = { elementPos.x, elementPos.y, elementPos.z };
    float rot[3] = { elementRot.x, elementRot.y, elementRot.z };
    float size[2] = { elementSize.x, elementSize.y };
    float res[2] = { renderWidth, renderHeight };
    float camPos[3] = { cameraPos.x, cameraPos.y, cameraPos.z };
    float camRot[3] = { cameraRot.x, cameraRot.y, cameraRot.z };
    float clip[2] = { nearPlane, farPlane };

    LPD3DXEFFECT eff = m_pResources->pImage3DEffect;
    D3DXHANDLE h;
    h = eff->GetParameterByName(nullptr, "sElementPosition");
    if (h) eff->SetFloatArray(h, pos, 3);
    h = eff->GetParameterByName(nullptr, "sElementRotation");
    if (h) eff->SetFloatArray(h, rot, 3);
    h = eff->GetParameterByName(nullptr, "sElementSize");
    if (h) eff->SetFloatArray(h, size, 2);
    h = eff->GetParameterByName(nullptr, "sScrRes");
    if (h) eff->SetFloatArray(h, res, 2);
    h = eff->GetParameterByName(nullptr, "sCameraInputPosition");
    if (h) eff->SetFloatArray(h, camPos, 3);
    h = eff->GetParameterByName(nullptr, "sCameraInputRotation");
    if (h) eff->SetFloatArray(h, camRot, 3);
    h = eff->GetParameterByName(nullptr, "sFov");
    if (h) eff->SetFloat(h, fov);
    h = eff->GetParameterByName(nullptr, "sClip");
    if (h) eff->SetFloatArray(h, clip, 2);

    float projectionAspect = MathUtils::GetRadarProjectionAspect();
    h = eff->GetParameterByName(nullptr, "sProjectionAspect");
    if (h) eff->SetFloat(h, projectionAspect);

    const float flat = RadarConfig::GetRadar3D() ? 0.0f : 1.0f;
    h = eff->GetParameterByName(nullptr, "sFlatProjection");
    if (h) eff->SetFloat(h, flat);

    h = eff->GetParameterByName(nullptr, "sTexColor");
    if (h) eff->SetTexture(h, texture);

    D3DXHANDLE hTechnique = eff->GetTechniqueByName("Image3D");
    if (!hTechnique)
        return;

    RenderStates saved = SaveRenderStates();
    m_pDevice->SetRenderState(D3DRS_ZENABLE, FALSE);
    m_pDevice->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
    m_pDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
    m_pDevice->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
    m_pDevice->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
    m_pDevice->SetRenderState(D3DRS_STENCILENABLE, FALSE);
    m_pDevice->SetRenderState(D3DRS_LIGHTING, FALSE);
    m_pDevice->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
    m_pDevice->SetRenderState(D3DRS_COLORWRITEENABLE,
                              D3DCOLORWRITEENABLE_RED | D3DCOLORWRITEENABLE_GREEN |
                              D3DCOLORWRITEENABLE_BLUE | D3DCOLORWRITEENABLE_ALPHA);

    eff->SetTechnique(hTechnique);
    UINT numPasses = 0;
    if (SUCCEEDED(eff->Begin(&numPasses, D3DXFX_DONOTSAVESTATE)))
    {
        for (UINT pass = 0; pass < numPasses; ++pass)
        {
            if (FAILED(eff->BeginPass(pass)))
                continue;
            m_pDevice->SetFVF(D3DFVF_XYZ | D3DFVF_TEX1 | D3DFVF_DIFFUSE);
            m_pDevice->SetStreamSource(0, nullptr, 0, 0);
            m_pDevice->DrawPrimitiveUP(D3DPT_TRIANGLELIST, 2, vertices, sizeof(Image3DVertex));
            eff->EndPass();
        }
        eff->End();
    }

    m_pDevice->SetVertexShader(nullptr);
    m_pDevice->SetPixelShader(nullptr);
    RestoreRenderStates(saved);
}

void DxDrawPrimitives::dxDrawText(const char* text, float x, float y, float sx, float sy, float rotation, DWORD color)
{
    if (!m_pDevice || !m_pResources || !m_pResources->pFont || !text)
        return;

    RECT rect = { (LONG)x, (LONG)y, (LONG)(x + sx), (LONG)(y + sy) };
    UINT format = DT_LEFT | DT_TOP | DT_NOCLIP;

    if (rotation != 0.0f && m_pResources->pFontSprite)
    {
        D3DXVECTOR2 center((x + sx) * 0.5f, (y + sy) * 0.5f);
        D3DXVECTOR2 scale(1.0f, 1.0f);
        D3DXVECTOR2 trans(0.0f, 0.0f);
        D3DXMATRIX mat;
        D3DXMatrixTransformation2D(&mat, &center, 0.0f, &scale, &center, rotation, &trans);
        if (SUCCEEDED(m_pResources->pFontSprite->Begin(D3DXSPRITE_ALPHABLEND)))
        {
            m_pResources->pFontSprite->SetTransform(&mat);
            m_pResources->pFont->DrawTextA(m_pResources->pFontSprite, text, -1, &rect, format, color);
            m_pResources->pFontSprite->End();
        }
    }
    else
    {
        m_pResources->pFont->DrawTextA(nullptr, text, -1, &rect, format, color);
    }
}

static void BuildRadarViewProjMatrix(const D3DXVECTOR3& cameraPos, const D3DXVECTOR3& cameraRot,
    float fov, float nearPlane, float farPlane, float aspect,
    D3DXMATRIX& outView, D3DXMATRIX& outProj)
{
    const bool flat2D = !RadarConfig::GetRadar3D();
    float useAspect = aspect;
    if (flat2D)
        useAspect = 1.0f;
    MathUtils::BuildRadarViewProj(cameraPos, cameraRot, fov, nearPlane, farPlane, useAspect, flat2D, outView, outProj);
}

void DxDrawPrimitives::dxDrawRoute3D(const std::vector<RoutePoint3D>& route, const D3DXVECTOR3& cameraPos, const D3DXVECTOR3& cameraRot,
                                     float fov, float nearPlane, float farPlane, float aspect, float lineWidth)
{
    if (!m_pDevice || !m_pResources || !m_pResources->pLineSmoothEffect || route.size() < 2)
        return;

    HRESULT hr = m_pDevice->TestCooperativeLevel();
    if (FAILED(hr) && hr != D3DERR_DEVICENOTRESET)
        return;

    D3DXMATRIX view, proj;
    BuildRadarViewProjMatrix(cameraPos, cameraRot, fov, nearPlane, farPlane, aspect, view, proj);

    D3DXMATRIX world;
    D3DXMatrixIdentity(&world);
    D3DXMATRIX viewProj, worldViewProj;
    D3DXMatrixMultiply(&viewProj, &view, &proj);
    D3DXMatrixMultiply(&worldViewProj, &world, &viewProj);

    RenderStates saved = SaveRenderStates();

    m_pDevice->SetRenderState(D3DRS_ZENABLE, FALSE);
    m_pDevice->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
    m_pDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
    m_pDevice->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
    m_pDevice->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
    m_pDevice->SetRenderState(D3DRS_LIGHTING, FALSE);
    m_pDevice->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
    m_pDevice->SetRenderState(D3DRS_STENCILENABLE, FALSE);
    m_pDevice->SetTexture(0, nullptr);

    LPD3DXEFFECT eff = m_pResources->pLineSmoothEffect;
    D3DXHANDLE hWorldViewProj = eff->GetParameterByName(nullptr, "WorldViewProj");
    if (hWorldViewProj)
        eff->SetMatrix(hWorldViewProj, &worldViewProj);

    struct LineSmoothVertex
    {
        float x, y, z;
        DWORD color;
        float u, v;
    };
    const D3DVERTEXELEMENT9 decl[] = {
        { 0, 0, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },
        { 0, 12, D3DDECLTYPE_D3DCOLOR, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_COLOR, 0 },
        { 0, 16, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0 },
        D3DDECL_END()
    };
    LPDIRECT3DVERTEXDECLARATION9 pDecl = nullptr;
    m_pDevice->CreateVertexDeclaration(decl, &pDecl);
    m_pDevice->SetVertexDeclaration(pDecl);

    size_t numSegments = route.size() - 1;
    float halfWidth = lineWidth * 0.5f;
    float circleRadius = halfWidth * 0.9f;
    const int circleSegments = 8;

    D3DXHANDLE hTech = eff->GetTechniqueByName("LineSmooth");
    if (hTech && SUCCEEDED(eff->SetTechnique(hTech)))
    {
        UINT numPasses = 0;
        if (SUCCEEDED(eff->Begin(&numPasses, 0)))
        {
            for (UINT pass = 0; pass < numPasses; ++pass)
            {
                if (SUCCEEDED(eff->BeginPass(pass)))
                {
                    // Technique defaults used to force ZEnable; radar RT has no
                    // depth stencil, so any Z test drops the entire GPS line.
                    m_pDevice->SetRenderState(D3DRS_ZENABLE, FALSE);
                    m_pDevice->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);

                    for (size_t i = 0; i < numSegments; ++i)
                    {
                        float cx = route[i].x, cy = route[i].y, cz = route[i].z;
                        DWORD color = (route[i].color & 0x00FFFFFF) | 0xFF000000;

                        for (int j = 0; j < circleSegments; ++j)
                        {
                            float a1 = (float)j / (float)circleSegments * 2.0f * D3DX_PI;
                            float a2 = (float)(j + 1) / (float)circleSegments * 2.0f * D3DX_PI;

                            LineSmoothVertex tri[3] = {
                                { cx, cy, cz, color, 0.5f, 0.0f },
                                { cx + cosf(a1) * circleRadius, cy + sinf(a1) * circleRadius, cz, color, 0.5f, 1.0f },
                                { cx + cosf(a2) * circleRadius, cy + sinf(a2) * circleRadius, cz, color, 0.5f, 1.0f }
                            };
                            m_pDevice->DrawPrimitiveUP(D3DPT_TRIANGLELIST, 1, tri, sizeof(LineSmoothVertex));
                        }
                    }

                    for (size_t i = 0; i < numSegments; ++i)
                    {
                        D3DXVECTOR3 start(route[i].x, route[i].y, route[i].z);
                        D3DXVECTOR3 end(route[i + 1].x, route[i + 1].y, route[i + 1].z);

                        float dx = end.x - start.x;
                        float dy = end.y - start.y;
                        float length2D = sqrtf(dx * dx + dy * dy);
                        if (length2D < 0.001f)
                            continue;

                        dx /= length2D;
                        dy /= length2D;
                        float perpX = -dy * halfWidth;
                        float perpY = dx * halfWidth;

                        D3DXVECTOR3 p1(start.x - perpX, start.y - perpY, start.z);
                        D3DXVECTOR3 p2(start.x + perpX, start.y + perpY, start.z);
                        D3DXVECTOR3 p3(end.x - perpX, end.y - perpY, end.z);
                        D3DXVECTOR3 p4(end.x + perpX, end.y + perpY, end.z);

                        DWORD cStart = (route[i].color & 0x00FFFFFF) | 0xFF000000;
                        DWORD cEnd = (route[i + 1].color & 0x00FFFFFF) | 0xFF000000;

                        LineSmoothVertex quad[6] = {
                            { p1.x, p1.y, p1.z, cStart, 0.0f, -1.0f },
                            { p2.x, p2.y, p2.z, cStart, 0.0f, 1.0f },
                            { p3.x, p3.y, p3.z, cEnd, 1.0f, -1.0f },
                            { p2.x, p2.y, p2.z, cStart, 0.0f, 1.0f },
                            { p4.x, p4.y, p4.z, cEnd, 1.0f, 1.0f },
                            { p3.x, p3.y, p3.z, cEnd, 1.0f, -1.0f }
                        };
                        m_pDevice->DrawPrimitiveUP(D3DPT_TRIANGLELIST, 2, quad, sizeof(LineSmoothVertex));
                    }

                    eff->EndPass();
                }
            }
            eff->End();
        }
    }

    if (pDecl)
        pDecl->Release();

    m_pDevice->SetVertexShader(nullptr);
    m_pDevice->SetPixelShader(nullptr);
    RestoreRenderStates(saved);
}

void DxDrawPrimitives::dxDrawLine3D(const D3DXVECTOR3& start, const D3DXVECTOR3& end, float width, DWORD color,
                                    const D3DXVECTOR3& cameraPos, const D3DXVECTOR3& cameraRot,
                                    float fov, float nearPlane, float farPlane, float aspect)
{
    if (!m_pDevice || !m_pResources || !m_pResources->pLineEffect)
        return;

    D3DXVECTOR3 dir = end - start;
    float length = D3DXVec3Length(&dir);
    if (length < 0.01f)
        return;

    D3DXMATRIX view, proj;
    BuildRadarViewProjMatrix(cameraPos, cameraRot, fov, nearPlane, farPlane, aspect, view, proj);

    float cp = cosf(cameraRot.x);
    float sp = sinf(cameraRot.x);
    float cy = cosf(cameraRot.z);
    float sy = sinf(cameraRot.z);
    D3DXMATRIX camWorld;
    camWorld._11 = cy; camWorld._12 = sy; camWorld._13 = 0.0f; camWorld._14 = 0.0f;
    camWorld._21 = -cp * sy; camWorld._22 = cy * cp; camWorld._23 = sp; camWorld._24 = 0.0f;
    camWorld._31 = sy * sp; camWorld._32 = -cy * sp; camWorld._33 = cp; camWorld._34 = 0.0f;
    camWorld._41 = cameraPos.x; camWorld._42 = cameraPos.y; camWorld._43 = cameraPos.z; camWorld._44 = 1.0f;

    D3DXVECTOR3 forwardVec(camWorld._21, camWorld._22, camWorld._23);
    D3DXVECTOR3 xaxis;
    D3DXVECTOR3 downVec(0.0f, 0.0f, -1.0f);
    float dotProduct = D3DXVec3Dot(&downVec, &forwardVec);
    if (dotProduct > 1.0f) dotProduct = 1.0f;
    if (dotProduct < -1.0f) dotProduct = -1.0f;
    float rotOff = 600.0f * acosf(dotProduct) / (0.5f * D3DX_PI);
    D3DXVECTOR3 offX(camWorld._11 + camWorld._21 - rotOff * camWorld._31,
        camWorld._12 + camWorld._22 - rotOff * camWorld._32,
        camWorld._13 + camWorld._23 - rotOff * camWorld._33);
    D3DXVECTOR3 viewPos(camWorld._41 + offX.x, camWorld._42 + offX.y, camWorld._43 + offX.z);
    D3DXVECTOR3 zaxis = forwardVec;
    D3DXVec3Normalize(&zaxis, &zaxis);
    D3DXVECTOR3 upVec(camWorld._31, camWorld._32, camWorld._33);
    D3DXVECTOR3 negUpVec(-upVec.x, -upVec.y, -upVec.z);
    D3DXVec3Cross(&xaxis, &negUpVec, &zaxis);
    D3DXVec3Normalize(&xaxis, &xaxis);

    D3DXVECTOR3 lineDir = dir;
    D3DXVec3Normalize(&lineDir, &lineDir);
    D3DXVECTOR3 perpDir;
    D3DXVec3Cross(&perpDir, &lineDir, &forwardVec);
    float perpLen = D3DXVec3Length(&perpDir);
    if (perpLen < 0.01f)
        perpDir = xaxis;
    else
        D3DXVec3Normalize(&perpDir, &perpDir);

    D3DXVECTOR3 halfWidthVec = perpDir * (width * 0.5f);

    struct LineVertex { float x, y, z; DWORD color; };
    LineVertex vertices[4] = {
        { start.x + halfWidthVec.x, start.y + halfWidthVec.y, start.z + halfWidthVec.z, color },
        { start.x - halfWidthVec.x, start.y - halfWidthVec.y, start.z - halfWidthVec.z, color },
        { end.x + halfWidthVec.x, end.y + halfWidthVec.y, end.z + halfWidthVec.z, color },
        { end.x - halfWidthVec.x, end.y - halfWidthVec.y, end.z - halfWidthVec.z, color }
    };

    D3DXMATRIX world;
    D3DXMatrixIdentity(&world);
    D3DXMATRIX viewProj, worldViewProj;
    D3DXMatrixMultiply(&viewProj, &view, &proj);
    D3DXMatrixMultiply(&worldViewProj, &world, &viewProj);

    RenderStates saved = SaveRenderStates();

    m_pDevice->SetRenderState(D3DRS_ZENABLE, FALSE);
    m_pDevice->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
    m_pDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
    m_pDevice->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
    m_pDevice->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
    m_pDevice->SetRenderState(D3DRS_LIGHTING, FALSE);
    m_pDevice->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
    m_pDevice->SetRenderState(D3DRS_STENCILENABLE, FALSE);

    LPD3DXEFFECT eff = m_pResources->pLineEffect;
    D3DXHANDLE hWorldViewProj = eff->GetParameterByName(nullptr, "WorldViewProj");
    if (hWorldViewProj)
        eff->SetMatrix(hWorldViewProj, &worldViewProj);
    D3DXHANDLE hTech = eff->GetTechniqueByName("Line");
    if (hTech)
    {
        eff->SetTechnique(hTech);
        UINT numPasses = 0;
        eff->Begin(&numPasses, 0);
        for (UINT pass = 0; pass < numPasses; ++pass)
        {
            eff->BeginPass(pass);
            m_pDevice->SetFVF(D3DFVF_XYZ | D3DFVF_DIFFUSE);
            m_pDevice->SetStreamSource(0, nullptr, 0, 0);
            m_pDevice->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, vertices, sizeof(LineVertex));
            eff->EndPass();
        }
        eff->End();
    }

    m_pDevice->SetVertexShader(nullptr);
    m_pDevice->SetPixelShader(nullptr);
    RestoreRenderStates(saved);
}
