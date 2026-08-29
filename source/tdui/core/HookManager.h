/*****************************************************************************
 *
 *  PROJECT:     The-Definitive-UI
 *  FILE:        source/HookManager/HookManager.h
 *  PURPOSE:     Frontend + pause DrawFrontEnd redirects
 *
 *****************************************************************************/

#pragma once

#include <cstdint>

class HookManager
{
public:
    using DrawFn = void (*)();

    struct Addr
    {
        static constexpr uintptr_t DrawFrontEnd                  = 0x57C290;
        static constexpr uintptr_t Call_DrawFrontEnd_FrontendIdle = 0x53E82D;
        static constexpr uintptr_t Call_DrawFrontEnd_Idle         = 0x53EB8C;
        static constexpr uintptr_t Process                        = 0x57B440;
        static constexpr uintptr_t UserInput                      = 0x57FD70;
        static constexpr uintptr_t GameState                      = 0xC8D4C0;
        static constexpr uintptr_t FrontendIdle                   = 0x53E770;
        static constexpr uintptr_t Idle                           = 0x53E920;
        // Idle → CGame::Process → FrontEndMenuManager.Process (not a direct CALL in Idle)
        static constexpr uintptr_t CGame_Process                  = 0x53BEE0;
        static constexpr uintptr_t UpdatePads                     = 0x541DD0;
        static constexpr uintptr_t DIReleaseMouse                 = 0x746F70;
        static constexpr uintptr_t diMouseInit                    = 0x7469A0;
        static constexpr uintptr_t DrawRadar                      = 0x58A330;
        // CAERadioTrackManager::DisplayRadioStationName (CFont HUD name)
        static constexpr uintptr_t DisplayRadioStationName        = 0x4E9E50;
        // CHud::DrawHelpText — top-left PRINT_HELP / pickups
        static constexpr uintptr_t DrawHelpText                   = 0x58B6E0;
    };

    struct Offset
    {
        static constexpr uintptr_t m_bDontDrawFrontEnd = 0x32;
        static constexpr uintptr_t m_bMenuActive       = 0x5C;
        static constexpr uintptr_t m_bGameNotLoaded    = 0xE9;
    };

    HookManager() = default;
    ~HookManager() = default;

    void Install(DrawFn drawMain, DrawFn drawPause);
    void InstallRadar(DrawFn drawRadar);
    void InstallRadio(DrawFn drawRadio);
    void InstallHelp(DrawFn drawHelp);

    bool IsCustomMainMenuSession() const;
    bool IsCustomPauseSession() const;
    bool IsSuppressingStockFrontEnd() const { return IsCustomMainMenuSession(); }

    // ESC JustPressed while pause was open — PauseMenu should resume (one-shot)
    bool ConsumePauseEscClose();

    // Fullscreen map: ESC exits FS instead of resume
    void SetPauseMapFullscreen(bool enabled);
    bool ConsumePauseMapFullscreenEsc();

    // Exit confirm dialog: ESC cancels confirm instead of resume
    void SetPauseExitConfirm(bool enabled);
    bool ConsumePauseExitConfirmEsc();

    void DismissCustomMainMenu();
    void RequestResumeGame();
    void RequestResumeGameFromEsc();
    void RequestNewGame();
    void RequestLoadGame(int slot);
    void TickSession() const;
    int  GetGameState() const;

private:
    static void ClearEscJustPressed();
    static void FlushControlsAfterFrontend(); // stock exit + attack suppress until release
    static void SuppressAttackAfterPads();
    static void __cdecl UpdatePads_Detour();
    static void __fastcall DrawFrontEnd_Main(class CMenuManager* menu);
    static void __fastcall DrawFrontEnd_Pause(class CMenuManager* menu);
    static int  __fastcall Process_Detour(class CMenuManager* menu);
    static void __fastcall UserInput_SkipCustom(class CMenuManager* menu);

    static DrawFn s_drawMain;
    static DrawFn s_drawPause;
    static DrawFn s_drawRadar;
    static DrawFn s_drawRadio;
    static DrawFn s_drawHelp;
    static void __cdecl DrawHelpText_Detour();
    static void __cdecl DrawRadar_Detour();
    static void __fastcall DisplayRadioStationName_Detour(class CAERadioTrackManager* self, void* unused);
    static bool   s_installed;
    static bool   s_session;
    static bool   s_pauseEscClose;  // set in Process, consumed by PauseMenu
    static bool   s_pauseWasActive; // edge: open ESC must not resume same frame
    static bool   s_pauseMapFullscreen;
    static bool   s_pauseMapFsEsc;  // ESC while map FS — consume in PauseMenu
    static bool   s_pauseExitConfirm;
    static bool   s_pauseExitConfirmEsc;
    static DWORD  s_suppressAttackUntilMs; // GetTickCount cutoff; 0 = off
};
