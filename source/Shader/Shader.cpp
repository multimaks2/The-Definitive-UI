/*****************************************************************************
 *
 *  PROJECT:     The-Definitive-UI
 *  FILE:        source/Shader/Shader.cpp
 *  PURPOSE:     Map fog pixel shaders - ZonesVisited arcs and prebuilt mask
 *
 *****************************************************************************/

#include "Shader.h"

#include <d3dx9.h>
#include <cstring>
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

    const char kFogMaskPsAsm[] =
        "ps_2_0\n"
        "dcl t0.xy\n"
        "dcl_2d s0\n"
        "texld r0, t0, s0\n"
        "mov r1, c0\n"
        "mul r1.w, r1.w, r0.x\n"
        "mov oC0, r1\n";

    // Exact port of fog_preview.py:
    //   hard = fog_at (tip AABB disk cut)
    //   soft = smoothstep(dist to contour) — quarter-arcs + shortened edges, always min, never full-circle SDF.
    // Params: c1 = { n, R, 1/n, softW }
    const char kFogZonesHlsl[] =
        "sampler Zones : register(s0);\n"
        "float4 FogColor : register(c0);\n"
        "float4 Params : register(c1);\n"
        "float explor(float2 cell)\n"
        "{\n"
        "  float n = Params.x;\n"
        "  if (cell.x < 0 || cell.y < 0 || cell.x >= n || cell.y >= n) return 0;\n"
        "  return tex2D(Zones, (cell + 0.5) * Params.z).r;\n"
        "}\n"
        "float isRound(float2 vtx)\n"
        "{\n"
        "  float e00 = explor(vtx + float2(-1,-1));\n"
        "  float e10 = explor(vtx + float2(0,-1));\n"
        "  float e01 = explor(vtx + float2(-1,0));\n"
        "  float e11 = explor(vtx);\n"
        "  float cnt = e00 + e10 + e01 + e11;\n"
        "  return ((cnt > 0.75 && cnt < 1.25) || (cnt > 2.75 && cnt < 3.25)) ? 1.0 : 0.0;\n"
        "}\n"
        "float distSeg(float2 p, float2 a, float2 b)\n"
        "{\n"
        "  float2 ab = b - a;\n"
        "  float den = dot(ab, ab);\n"
        "  if (den < 1e-8) return length(p - a);\n"
        "  float t = saturate(dot(p - a, ab) / den);\n"
        "  return length(p - (a + ab * t));\n"
        "}\n"
        "float distQArc(float2 p, float2 c, float2 s, float R)\n"
        "{\n"
        "  float2 d = p - c;\n"
        "  float2 q = d * s;\n"
        "  if (q.x >= 0.0 && q.y >= 0.0)\n"
        "    return abs(length(d) - R);\n"
        "  return min(length(d - s * float2(R, 0)), length(d - s * float2(0, R)));\n"
        "}\n"
        "float4 main(float2 uv : TEXCOORD0) : COLOR\n"
        "{\n"
        "  float n = Params.x;\n"
        "  float R = Params.y;\n"
        "  float softW = Params.w;\n"
        "  float2 p = uv * n;\n"
        "  float2 cell = floor(p);\n"
        "  float fogHard = 1.0 - explor(cell);\n"
        "  float sd = 1000.0;\n"
        "  for (int i = 0; i < 4; i++)\n"
        "  {\n"
        "    float2 vtx = cell + float2(i == 1 || i == 3, i == 2 || i == 3);\n"
        "    float e00 = explor(vtx + float2(-1,-1));\n"
        "    float e10 = explor(vtx + float2(0,-1));\n"
        "    float e01 = explor(vtx + float2(-1,0));\n"
        "    float e11 = explor(vtx);\n"
        "    float cnt = e00 + e10 + e01 + e11;\n"
        "    float2 center;\n"
        "    float2 tip0;\n"
        "    float2 s;\n"
        "    float active = 0;\n"
        "    float convex = 0;\n"
        "    if (cnt > 0.75 && cnt < 1.25)\n"
        "    {\n"
        "      active = 1; convex = 1;\n"
        "      if (e00 > 0.5) { center = vtx + float2(-R,-R); tip0 = center; s = float2(1,1); }\n"
        "      else if (e10 > 0.5) { center = vtx + float2(R,-R); tip0 = vtx + float2(0,-R); s = float2(-1,1); }\n"
        "      else if (e01 > 0.5) { center = vtx + float2(-R,R); tip0 = vtx + float2(-R,0); s = float2(1,-1); }\n"
        "      else { center = vtx + float2(R,R); tip0 = vtx; s = float2(-1,-1); }\n"
        "    }\n"
        "    else if (cnt > 2.75 && cnt < 3.25)\n"
        "    {\n"
        "      active = 1;\n"
        "      if (e00 < 0.5) { center = vtx + float2(-R,-R); tip0 = center; s = float2(1,1); }\n"
        "      else if (e10 < 0.5) { center = vtx + float2(R,-R); tip0 = vtx + float2(0,-R); s = float2(-1,1); }\n"
        "      else if (e01 < 0.5) { center = vtx + float2(-R,R); tip0 = vtx + float2(-R,0); s = float2(1,-1); }\n"
        "      else { center = vtx + float2(R,R); tip0 = vtx; s = float2(-1,-1); }\n"
        "    }\n"
        "    if (active < 0.5) continue;\n"
        "    sd = min(sd, distQArc(p, center, s, R));\n"
        "    float2 tip1 = tip0 + float2(R,R);\n"
        "    if (p.x < tip0.x || p.y < tip0.y || p.x > tip1.x || p.y > tip1.y) continue;\n"
        "    float dArc = length(p - center) - R;\n"
        "    if (convex > 0.5)\n"
        "      fogHard = (dArc > 0.0) ? 1.0 : 0.0;\n"
        "    else\n"
        "      fogHard = (dArc < 0.0) ? 1.0 : 0.0;\n"
        "  }\n"
        "  {\n"
        "    float2 c0 = cell;\n"
        "    float2 c1 = cell + float2(1,1);\n"
        "    float eC = explor(cell);\n"
        "    float eL = explor(cell + float2(-1,0));\n"
        "    float eR = explor(cell + float2(1,0));\n"
        "    float eT = explor(cell + float2(0,-1));\n"
        "    float eB = explor(cell + float2(0,1));\n"
        "    if ((eC > 0.5) != (eL > 0.5))\n"
        "    {\n"
        "      float y0 = c0.y + (isRound(c0) > 0.5 ? R : 0.0);\n"
        "      float y1 = c1.y - (isRound(float2(c0.x, c1.y)) > 0.5 ? R : 0.0);\n"
        "      if (y1 > y0) sd = min(sd, distSeg(p, float2(c0.x, y0), float2(c0.x, y1)));\n"
        "    }\n"
        "    if ((eC > 0.5) != (eR > 0.5))\n"
        "    {\n"
        "      float y0 = c0.y + (isRound(float2(c1.x, c0.y)) > 0.5 ? R : 0.0);\n"
        "      float y1 = c1.y - (isRound(c1) > 0.5 ? R : 0.0);\n"
        "      if (y1 > y0) sd = min(sd, distSeg(p, float2(c1.x, y0), float2(c1.x, y1)));\n"
        "    }\n"
        "    if ((eC > 0.5) != (eT > 0.5))\n"
        "    {\n"
        "      float x0 = c0.x + (isRound(c0) > 0.5 ? R : 0.0);\n"
        "      float x1 = c1.x - (isRound(float2(c1.x, c0.y)) > 0.5 ? R : 0.0);\n"
        "      if (x1 > x0) sd = min(sd, distSeg(p, float2(x0, c0.y), float2(x1, c0.y)));\n"
        "    }\n"
        "    if ((eC > 0.5) != (eB > 0.5))\n"
        "    {\n"
        "      float x0 = c0.x + (isRound(float2(c0.x, c1.y)) > 0.5 ? R : 0.0);\n"
        "      float x1 = c1.x - (isRound(c1) > 0.5 ? R : 0.0);\n"
        "      if (x1 > x0) sd = min(sd, distSeg(p, float2(x0, c1.y), float2(x1, c1.y)));\n"
        "    }\n"
        "  }\n"
        "  float fog = 0.0;\n"
        "  if (fogHard > 0.5)\n"
        "  {\n"
        "    if (softW < 0.0001) fog = 1.0;\n"
        "    else fog = smoothstep(0.0, softW, sd);\n"
        "  }\n"
        "  return float4(FogColor.rgb, FogColor.a * saturate(fog));\n"
        "}\n";

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
        if (g_fogPsBlob.empty())
        {
            const char* profiles[] = { "ps_3_0", "ps_2_a", "ps_2_b", "ps_2_0" };
            for (const char* profile : profiles)
            {
                ID3DXBuffer* pCode = nullptr;
                ID3DXBuffer* pErr = nullptr;
                HRESULT hr = D3DXCompileShader(kFogZonesHlsl, static_cast<UINT>(std::strlen(kFogZonesHlsl)),
                                               nullptr, nullptr, "main", profile, 0, &pCode, &pErr, nullptr);
                if (pErr)
                {
                    OutputDebugStringA(static_cast<const char*>(pErr->GetBufferPointer()));
                    pErr->Release();
                }
                if (SUCCEEDED(hr) && pCode)
                {
                    const BYTE* data = static_cast<const BYTE*>(pCode->GetBufferPointer());
                    g_fogPsBlob.assign(data, data + pCode->GetBufferSize());
                    pCode->Release();
                    break;
                }
                if (pCode)
                    pCode->Release();
            }
        }

        if (!g_fogPsBlob.empty())
        {
            if (FAILED(m_pDevice->CreatePixelShader(
                    reinterpret_cast<const DWORD*>(g_fogPsBlob.data()), &m_pFogPS)))
                m_pFogPS = nullptr;
        }
    }

    if (!m_pFogMaskPS)
    {
        if (g_fogMaskPsBlob.empty())
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
