/*****************************************************************************
 *
 *  PROJECT:     The-Definitive-UI
 *  FILE:        source/Game/WindowMode.h
 *  PURPOSE:     Safe windowed / exclusive / borderless apply (last-good snapshot)
 *
 *****************************************************************************/

#pragma once

struct IDirect3DDevice9;

namespace WindowMode
{
    // 0 windowed, 1 exclusive fullscreen, 2 borderless
    void Install();
    void Init();
    void SetGraphicsFlush(void (*fn)());
    void SetDeviceLost(void (*fn)());

    IDirect3DDevice9* Device();
    int  Query(); // from HWND + Present, not ini

    // Exclusive-list index for WxH, or -1. Prefs/display follow saved ini.
    int  FindVideoMode(int width, int height);
    void SyncMenuFromConfig();

    // Last click wins. Applied on Flush (next Idle / Process), never mid-draw.
    void Request(int windowMode, int videoModeIndex);
    void Flush(); // device Reset only — call from Idle after AA
    void Pump();  // graphics flush callback (AA / postfx)

    void OnDeviceReset();
    void SyncRsFromBackbuffer();
    void TickChrome(); // caption + work-area if windowed style was stripped

    // Windowed/borderless: release DI mouse + OS clip on alt-tab / minimize
    void OnLostFocus();
    void OnGotFocus();
}
