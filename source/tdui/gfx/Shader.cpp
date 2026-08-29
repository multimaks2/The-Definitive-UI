/*****************************************************************************
 *
 *  PROJECT:     The-Definitive-UI
 *  FILE:        source/Shader/Shader.cpp
 *  PURPOSE:     Map fog pixel shaders - ZonesVisited arcs and prebuilt mask
 *
 *****************************************************************************/

#include "Shader.h"
#include "ModPaths.h"

#include <d3dx9.h>
#include <cstring>
#include <fstream>
#include <vector>

namespace
{
    struct ScreenVertex
    {
        float x, y, z, rhw;
        float u, v;
    };

    const DWORD kFogFVF = D3DFVF_XYZRHW | D3DFVF_TEX1;

    std::vector<BYTE> g_fogPsBlob;
    std::vector<BYTE> g_fogMaskPsBlob;

    bool ReadBinaryFile(const char* path, std::vector<BYTE>& out)
    {
        if (!path || !path[0])
            return false;
        std::ifstream in(path, std::ios::binary | std::ios::ate);
        if (!in)
            return false;
        const std::streamsize size = in.tellg();
        if (size <= 0)
            return false;
        in.seekg(0, std::ios::beg);
        out.resize(static_cast<size_t>(size));
        return static_cast<bool>(in.read(reinterpret_cast<char*>(out.data()), size));
    }

    bool EnsureFogBlob(const char* fileName, std::vector<BYTE>& out)
    {
        if (!out.empty())
            return true;
        char path[MAX_PATH]{};
        if (!ModPaths::BuildShaderPath(path, sizeof(path), fileName))
            return false;
        return ReadBinaryFile(path, out);
    }

    const char kFogMaskPsAsm[] =
        "ps_2_0\n"
        "dcl t0.xy\n"
        "dcl_2d s0\n"
        "texld r0, t0, s0\n"
        "mov r1, c0\n"
        "mul r1.w, r1.w, r0.x\n"
        "mov oC0, r1\n";

    void DrawFogQuad(LPDIRECT3DDEVICE9 dev, IDirect3DPixelShader9* ps, LPDIRECT3DTEXTURE9 tex,
                     float fX, float fY, float fWidth, float fHeight,
                     const float* c0, const float* c1,
                     float u0 = 0.0f, float v0 = 0.0f, float u1 = 1.0f, float v1 = 1.0f)
    {
        const float x1 = fX - 0.5f;
        const float y1 = fY - 0.5f;
        const float x2 = fX + fWidth - 0.5f;
        const float y2 = fY + fHeight - 0.5f;

        ScreenVertex verts[6] = {
            { x1, y1, 0.0f, 1.0f, u0, v0 },
            { x2, y1, 0.0f, 1.0f, u1, v0 },
            { x1, y2, 0.0f, 1.0f, u0, v1 },
            { x2, y1, 0.0f, 1.0f, u1, v0 },
            { x2, y2, 0.0f, 1.0f, u1, v1 },
            { x1, y2, 0.0f, 1.0f, u0, v1 },
        };

        IDirect3DPixelShader9*  pOldPs = nullptr;
        IDirect3DVertexShader9* pOldVs = nullptr;
        IDirect3DBaseTexture9*  pOldTex = nullptr;
        DWORD fvf = 0;
        DWORD alphaBlend = 0, srcBlend = 0, destBlend = 0, zEnable = 0, cull = 0, lighting = 0;
        DWORD fog = 0, alphaTest = 0, colorWrite = 0;
        DWORD sampMin = 0, sampMag = 0, sampMip = 0, addrU = 0, addrV = 0;

        dev->GetPixelShader(&pOldPs);
        dev->GetVertexShader(&pOldVs);
        dev->GetTexture(0, &pOldTex);
        dev->GetFVF(&fvf);
        dev->GetRenderState(D3DRS_ALPHABLENDENABLE, &alphaBlend);
        dev->GetRenderState(D3DRS_SRCBLEND, &srcBlend);
        dev->GetRenderState(D3DRS_DESTBLEND, &destBlend);
        dev->GetRenderState(D3DRS_ZENABLE, &zEnable);
        dev->GetRenderState(D3DRS_CULLMODE, &cull);
        dev->GetRenderState(D3DRS_LIGHTING, &lighting);
        dev->GetRenderState(D3DRS_FOGENABLE, &fog);
        dev->GetRenderState(D3DRS_ALPHATESTENABLE, &alphaTest);
        dev->GetRenderState(D3DRS_COLORWRITEENABLE, &colorWrite);
        dev->GetSamplerState(0, D3DSAMP_MINFILTER, &sampMin);
        dev->GetSamplerState(0, D3DSAMP_MAGFILTER, &sampMag);
        dev->GetSamplerState(0, D3DSAMP_MIPFILTER, &sampMip);
        dev->GetSamplerState(0, D3DSAMP_ADDRESSU, &addrU);
        dev->GetSamplerState(0, D3DSAMP_ADDRESSV, &addrV);

        dev->SetPixelShader(ps);
        dev->SetVertexShader(nullptr);
        dev->SetPixelShaderConstantF(0, c0, 1);
        if (c1)
            dev->SetPixelShaderConstantF(1, c1, 1);

        dev->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
        dev->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
        dev->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
        dev->SetRenderState(D3DRS_ZENABLE, FALSE);
        dev->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
        dev->SetRenderState(D3DRS_LIGHTING, FALSE);
        dev->SetRenderState(D3DRS_FOGENABLE, FALSE);
        dev->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
        dev->SetRenderState(D3DRS_COLORWRITEENABLE,
                            D3DCOLORWRITEENABLE_RED | D3DCOLORWRITEENABLE_GREEN |
                            D3DCOLORWRITEENABLE_BLUE | D3DCOLORWRITEENABLE_ALPHA);

        dev->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_POINT);
        dev->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_POINT);
        dev->SetSamplerState(0, D3DSAMP_MIPFILTER, D3DTEXF_NONE);
        dev->SetSamplerState(0, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
        dev->SetSamplerState(0, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);

        dev->SetTexture(0, tex);
        dev->SetFVF(kFogFVF);
        dev->DrawPrimitiveUP(D3DPT_TRIANGLELIST, 2, verts, sizeof(ScreenVertex));

        dev->SetTexture(0, pOldTex);
        dev->SetPixelShader(pOldPs);
        dev->SetVertexShader(pOldVs);
        dev->SetFVF(fvf);
        dev->SetRenderState(D3DRS_ALPHABLENDENABLE, alphaBlend);
        dev->SetRenderState(D3DRS_SRCBLEND, srcBlend);
        dev->SetRenderState(D3DRS_DESTBLEND, destBlend);
        dev->SetRenderState(D3DRS_ZENABLE, zEnable);
        dev->SetRenderState(D3DRS_CULLMODE, cull);
        dev->SetRenderState(D3DRS_LIGHTING, lighting);
        dev->SetRenderState(D3DRS_FOGENABLE, fog);
        dev->SetRenderState(D3DRS_ALPHATESTENABLE, alphaTest);
        dev->SetRenderState(D3DRS_COLORWRITEENABLE, colorWrite);
        dev->SetSamplerState(0, D3DSAMP_MINFILTER, sampMin);
        dev->SetSamplerState(0, D3DSAMP_MAGFILTER, sampMag);
        dev->SetSamplerState(0, D3DSAMP_MIPFILTER, sampMip);
        dev->SetSamplerState(0, D3DSAMP_ADDRESSU, addrU);
        dev->SetSamplerState(0, D3DSAMP_ADDRESSV, addrV);

        if (pOldTex) pOldTex->Release();
        if (pOldPs)  pOldPs->Release();
        if (pOldVs)  pOldVs->Release();
    }
}

Shader::Shader() = default;

Shader::~Shader()
{
    Shutdown();
}

bool Shader::Initialize(LPDIRECT3DDEVICE9 pDevice)
{
    if (m_bInitialized)
        return true;
    if (!pDevice)
        return false;

    m_pDevice = pDevice;
    if (!CreateFogPixelShader())
    {
        Shutdown();
        return false;
    }

    m_bInitialized = true;
    return true;
}

void Shader::Shutdown()
{
    if (m_pFogPS)
    {
        m_pFogPS->Release();
        m_pFogPS = nullptr;
    }
    if (m_pFogMaskPS)
    {
        m_pFogMaskPS->Release();
        m_pFogMaskPS = nullptr;
    }
    m_pDevice = nullptr;
    m_bInitialized = false;
}

void Shader::OnDeviceLost()
{
    if (m_pFogPS)
    {
        m_pFogPS->Release();
        m_pFogPS = nullptr;
    }
    if (m_pFogMaskPS)
    {
        m_pFogMaskPS->Release();
        m_pFogMaskPS = nullptr;
    }
    m_bInitialized = false;
}

bool Shader::CreateFogPixelShader()
{
    if (!m_pDevice)
        return false;

    if (!m_pFogPS)
    {
        EnsureFogBlob("fog_zones.pso", g_fogPsBlob);

        if (!g_fogPsBlob.empty())
        {
            if (FAILED(m_pDevice->CreatePixelShader(
                    reinterpret_cast<const DWORD*>(g_fogPsBlob.data()), &m_pFogPS)))
                m_pFogPS = nullptr;
        }
    }

    if (!m_pFogMaskPS)
    {
        if (!EnsureFogBlob("fog_mask.pso", g_fogMaskPsBlob) && g_fogMaskPsBlob.empty())
        {
            ID3DXBuffer* pCode = nullptr;
            ID3DXBuffer* pErr = nullptr;
            HRESULT hr = D3DXAssembleShader(kFogMaskPsAsm, static_cast<UINT>(std::strlen(kFogMaskPsAsm)),
                                            nullptr, nullptr, 0, &pCode, &pErr);
            if (SUCCEEDED(hr) && pCode)
            {
                const BYTE* data = static_cast<const BYTE*>(pCode->GetBufferPointer());
                g_fogMaskPsBlob.assign(data, data + pCode->GetBufferSize());
            }
            if (pCode)
                pCode->Release();
            if (pErr)
                pErr->Release();
        }

        if (!g_fogMaskPsBlob.empty())
        {
            if (FAILED(m_pDevice->CreatePixelShader(
                    reinterpret_cast<const DWORD*>(g_fogMaskPsBlob.data()), &m_pFogMaskPS)))
                m_pFogMaskPS = nullptr;
        }
    }

    return m_pFogPS != nullptr || m_pFogMaskPS != nullptr;
}

void Shader::DrawFogFromMask(LPDIRECT3DTEXTURE9 pMask, float fX, float fY, float fWidth, float fHeight,
                             DWORD dwFogColor)
{
    if (!m_pDevice || !pMask)
        return;
    if (!m_pFogMaskPS && !CreateFogPixelShader())
        return;
    if (!m_pFogMaskPS)
        return;

    const float a = static_cast<float>((dwFogColor >> 24) & 0xFF) / 255.0f;
    const float r = static_cast<float>((dwFogColor >> 16) & 0xFF) / 255.0f;
    const float g = static_cast<float>((dwFogColor >> 8) & 0xFF) / 255.0f;
    const float b = static_cast<float>(dwFogColor & 0xFF) / 255.0f;
    const float fogConst[4] = { r, g, b, a };
    DrawFogQuad(m_pDevice, m_pFogMaskPS, pMask, fX, fY, fWidth, fHeight, fogConst, nullptr);
}

void Shader::DrawFogFromZones(LPDIRECT3DTEXTURE9 pZones, float fX, float fY, float fWidth, float fHeight,
                              int nCells, float cornerFrac, DWORD dwFogColor, float softW,
                              float u0, float v0, float u1, float v1)
{
    if (!m_pDevice || !pZones || nCells < 1)
        return;
    if (!m_pFogPS && !CreateFogPixelShader())
        return;
    if (!m_pFogPS)
        return;

    const float a = static_cast<float>((dwFogColor >> 24) & 0xFF) / 255.0f;
    const float r = static_cast<float>((dwFogColor >> 16) & 0xFF) / 255.0f;
    const float g = static_cast<float>((dwFogColor >> 8) & 0xFF) / 255.0f;
    const float b = static_cast<float>(dwFogColor & 0xFF) / 255.0f;
    const float fogConst[4] = { r, g, b, a };
    float R = cornerFrac;
    if (R < 0.05f)
        R = 0.05f;
    if (R > 0.45f)
        R = 0.45f;
    if (softW < 0.0f)
        softW = 0.0f;
    const float params[4] = {
        static_cast<float>(nCells),
        R,
        1.0f / static_cast<float>(nCells),
        softW
    };
    DrawFogQuad(m_pDevice, m_pFogPS, pZones, fX, fY, fWidth, fHeight, fogConst, params,
                u0, v0, u1, v1);
}
