/*****************************************************************************
 *
 *  PROJECT:     The-Definitive-UI
 *  FILE:        source/Radar/render/RadarTypes.h
 *  PURPOSE:     Vertex and render-state structs used by the radar draw layer
 *
 *****************************************************************************/

#pragma once

#include <d3d9.h>
#include <d3dx9.h>

struct ScreenVertex
{
    float x, y, z;
    DWORD color;
    float u, v;
};

struct RenderStates
{
    DWORD                        fvf = 0;
    IDirect3DBaseTexture9*       texture = nullptr;
    IDirect3DVertexDeclaration9* vertexDecl = nullptr;
    DWORD                        samplerStateU = 0;
    DWORD                        samplerStateV = 0;
    DWORD                        samplerMinFilter = 0;
    DWORD                        samplerMagFilter = 0;
    DWORD                        samplerMipFilter = 0;
    IDirect3DVertexShader9*      vertexShader = nullptr;
    IDirect3DPixelShader9*       pixelShader = nullptr;
    DWORD                        colorOp = 0;
    DWORD                        colorArg1 = 0;
    DWORD                        colorArg2 = 0;
    DWORD                        alphaOp = 0;
    DWORD                        alphaArg1 = 0;
    DWORD                        alphaArg2 = 0;
    DWORD                        colorOp1 = 0;
    DWORD                        alphaOp1 = 0;
    DWORD                        alphaBlendEnable = 0;
    DWORD                        srcBlend = 0;
    DWORD                        destBlend = 0;
    DWORD                        alphaTestEnable = 0;
    DWORD                        alphaRef = 0;
    DWORD                        alphaFunc = 0;
    DWORD                        zEnable = 0;
    DWORD                        zWriteEnable = 0;
    DWORD                        lighting = 0;
    DWORD                        cullMode = 0;
    DWORD                        fogEnable = 0;
    DWORD                        stencilEnable = 0;
    DWORD                        scissorEnable = 0;
    DWORD                        colorWriteEnable = 0;
    DWORD                        clipping = 0;
    RECT                         scissorRect{};
    D3DXMATRIX                   worldMatrix{};
    D3DXMATRIX                   viewMatrix{};
    D3DXMATRIX                   projMatrix{};
};
