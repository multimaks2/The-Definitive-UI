/*****************************************************************************
 *
 *  PROJECT:     The-Definitive-UI
 *  FILE:        source/Radar/render/draw/DrawResources.h
 *  PURPOSE:     Shared D3D resources handed to the radar draw layer
 *
 *****************************************************************************/

#pragma once

#include <d3d9.h>
#include <d3dx9.h>

struct DrawResources
{
    LPDIRECT3DDEVICE9  pDevice;
    LPD3DXEFFECT       pTriangleEffect;
    LPD3DXEFFECT       pBorderEffect;
    LPD3DXEFFECT       pImage3DEffect;
    LPD3DXEFFECT       pLineEffect;
    LPD3DXEFFECT       pLineSmoothEffect;
    LPD3DXEFFECT       pGreenSquareEffect;

    LPDIRECT3DTEXTURE9  pCircleTexture;
    LPDIRECT3DTEXTURE9  pNorthTexture;
    LPDIRECT3DTEXTURE9  pLineTexture;
    LPDIRECT3DTEXTURE9  pRadarRingPlaneTexture;

    LPD3DXFONT          pFont;
    LPD3DXSPRITE        pFontSprite;

    int screenWidth;
    int screenHeight;
    int renderTargetWidth;
    int renderTargetHeight;

    bool radarShapeCircle;
    bool borderShapeCircle;
};
