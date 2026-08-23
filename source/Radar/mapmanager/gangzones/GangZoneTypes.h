/*****************************************************************************
 *
 *  PROJECT:     The-Definitive-UI
 *  FILE:        source/Radar/mapmanager/gangzones/GangZoneTypes.h
 *  PURPOSE:     Gang zone rect and colour data shared by the renderer
 *
 *****************************************************************************/

#pragma once

#include <d3d9.h>

struct GangZone
{
    float         x1, y1, x2, y2;
    DWORD         color;
    unsigned char radarMode;
};
