/*****************************************************************************
 *  PROJECT:     The-Definitive-UI
 *  FILE:        source/Radar/mapmanager/RadarOverlayCompat.h
 *  PURPOSE:     Dispatch third-party plugin-sdk radar overlay / blips callbacks
 *****************************************************************************/

#pragma once

namespace RadarOverlayCompat
{
    // drawRadarOverlayEvent — GPS Redux (0x5869BF / 0x5759E4).
    void InvokeHudOverlay();
    void InvokePauseMapOverlay(bool drawStockGangOverlay);

    // drawBlipsEvent — CopNThreat (0x58AA2D H_JUMP / 0x575B44 H_CALL).
    // Runs stock DrawBlips, then foreign AFTER callbacks if the site is patched.
    void InvokeHudBlips();
    void InvokePauseMapBlips();

    bool IsInvokingHudOverlay();
    bool IsInvokingPauseMapOverlay();
    bool IsInvokingHudBlips();
    bool IsInvokingPauseMapBlips();
}
