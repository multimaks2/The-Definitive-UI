/*****************************************************************************
 *
 *  PROJECT:     The-Definitive-UI
 *  FILE:        source/Game/GameState.h
 *  PURPOSE:     Stock radar visibility rules - interiors, radar mode, HUD flash
 *
 *****************************************************************************/

#pragma once

#include "CVector.h"

class CPed;

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

    // Settings preview: map tiles only — no stock blip pipeline (needs loaded game).
    bool CanRenderRadarPreview();

    // Stock FindPlayerCentreOfWorld_NoInteriorShift — safe during enter/exit vehicle.
    CVector SafePlayerCentreForMap();

    bool IsPedInVehicleTransition(CPed* ped);

    // Guards vanilla stat/HUD paths during enter-exit vehicle (BMX cycling skill crash).
    void InstallGameplayGuards();
}
