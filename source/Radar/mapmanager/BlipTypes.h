/*****************************************************************************
 *
 *  PROJECT:     The-Definitive-UI
 *  FILE:        source/Radar/mapmanager/BlipTypes.h
 *  PURPOSE:     Blip and height-indicator data shared by radar renderers
 *
 *****************************************************************************/

#pragma once

#include <d3d9.h>
#include <d3dx9.h>

enum eHeightIndicatorType
{
    HEIGHT_INDICATOR_BELOW = 0,
    HEIGHT_INDICATOR_ABOVE = 1,
    HEIGHT_INDICATOR_SAME  = 2
};

struct Blip
{
    D3DXVECTOR3 position;
    int         iconId;
    float       size;
    DWORD       color;
    bool        enabled;
    bool        shortRange;  // m_bShortRange: only visible when player is within radar range
};
