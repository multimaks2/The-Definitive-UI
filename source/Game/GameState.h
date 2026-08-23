/*****************************************************************************
 *
 *  PROJECT:     The-Definitive-UI
 *  FILE:        source/Game/GameState.h
 *  PURPOSE:     Stock radar visibility rules - interiors, radar mode, HUD flash
 *
 *****************************************************************************/

#pragma once

namespace GameState
{
    // currArea != 0 or ped areaCode != NORMAL_WORLD (stock DrawMap test)
    bool IsPlayerInInterior();

    // RadarMode off, enter/exit, or ITEM_RADAR HUD flash (stock CHud::DrawRadar early-out)
    bool ShouldDrawRadar();

    // disc / rim / plane ring / altimeter — stock skips these when RadarMode == BLIPS_ONLY
    bool ShouldDrawRadarRim();

    // 3D map tiles: not interior, RadarMode != blips-only
    bool ShouldDrawRadarMap();
}
