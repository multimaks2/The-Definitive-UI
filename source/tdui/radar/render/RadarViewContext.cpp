/*****************************************************************************
 *
 *  PROJECT:     The-Definitive-UI
 *  FILE:        source/Radar/render/RadarViewContext.cpp
 *  PURPOSE:     Per-frame radar view state shared between renderer stages
 *
 *****************************************************************************/

#include "RadarViewContext.h"

float RadarViewContext::CalculateBlipSize(float baseBlipSize, float baseWidth) const
{
    float screenW = (float)screenWidth;
    float scaleX  = screenW / baseWidth;
    return baseBlipSize * scaleX;
}
