/*****************************************************************************
 *
 *  PROJECT:     The-Definitive-UI
 *  FILE:        source/HookManager/HookManager.cpp
 *  PURPOSE:     Redirect FrontendIdle + Idle DrawFrontEnd → custom menus
 *
 *****************************************************************************/

#include "HookManager.h"
#include "HelpGxt.h"
#include "Config.h"

#include "plugin.h"
#include "Patch.h"
#include "CMenuManager.h"
#include "CGenericGameStorage.h"
#include "CGame.h"
#include "CTimer.h"
#include "CPad.h"
#include "CAudioEngine.h"
#include "eAudioEvents.h"
#include "InputManager.h"
#include "CCamera.h"
#include "CReplay.h"
#include "common.h"
#include "CAERadioTrackManager.h"
#include "WindowMode.h"

#include "safetyhook.hpp"

#include <windows.h>

HookManager::DrawFn HookManager::s_drawMain = nullptr;
HookManager::DrawFn HookManager::s_drawPause = nullptr;
HookManager::DrawFn HookManager::s_drawRadar = nullptr;
HookManager::DrawFn HookManager::s_drawRadio = nullptr;
HookManager::DrawFn HookManager::s_drawHelp = nullptr;
bool HookManager::s_installed = false;
bool HookManager::s_session = false;
bool HookManager::s_pauseEscClose = false;
bool HookManager::s_pauseWasActive = false;
bool HookManager::s_pauseMapFullscreen = false;
bool HookManager::s_pauseMapFsEsc = false;
DWORD HookManager::s_suppressAttackUntilMs = 0;

namespace
{
    constexpr DWORD kSuppressAttackMs = 555;
    SafetyHookInline s_helpTextHook;
    SafetyHookInline s_radioTextHook;
    SafetyHookInline s_radarHook;

    uintptr_t FindCallTo(uintptr_t fnStart, size_t cbSize, uintptr_t target)
    {
        const uintptr_t end = fnStart + cbSize;
        for (uintptr_t at = fnStart; at + 5 <= end; ++at)
        {
            if (plugin::patch::GetUChar(at) != 0xE8)
                continue;
            const auto rel = static_cast<int32_t>(plugin::patch::GetUInt(at + 1));
            if (static_cast<intptr_t>(at + 5) + rel == static_cast<intptr_t>(target))
                return at;
        }
        return 0;
    }

    void HideOsCursorNow()
    {
        if (auto* input = InputManager::GetInstance())
            input->SetCursorOverride(false);

        SetCursor(nullptr);
        while (ShowCursor(FALSE) >= 0)
        {
        }
    }

    void ZeroAttackButtons(CPad* pad)
    {
        if (!pad)
            return;

        pad->NewState.ButtonCircle = 0;
        pad->OldState.ButtonCircle = 0;
        pad->NewState.LeftShoulder1 = 0;
        pad->OldState.LeftShoulder1 = 0;
        pad->NewState.RightShoulder1 = 0;
        pad->OldState.RightShoulder1 = 0;

        CPad::NewMouseControllerState.lmb = 0;
        CPad::OldMouseControllerState.lmb = 0;
        CPad::PCTempMouseControllerState.lmb = 0;
        CPad::NewMouseControllerState.rmb = 0;
        CPad::OldMouseControllerState.rmb = 0;
        CPad::PCTempMouseControllerState.rmb = 0;
    }
}

void HookManager::ClearEscJustPressed()
{
    CPad::OldKeyState.esc = CPad::NewKeyState.esc;
}

void HookManager::SuppressAttackAfterPads()
{
    if (s_suppressAttackUntilMs == 0)
        return;

    if (GetTickCount() >= s_suppressAttackUntilMs)
    {
        s_suppressAttackUntilMs = 0;
        return;
    }

    ZeroAttackButtons(CPad::GetPad(0));
}

void __cdecl HookManager::UpdatePads_Detour()
{
    plugin::Call<Addr::UpdatePads>();
    SuppressAttackAfterPads();
}

void HookManager::FlushControlsAfterFrontend()
{
    // Stock CheckForMenuClosing exit path (without UnloadTextures)
    if (CPad* pad = CPad::GetPad(0))
    {
        pad->Clear(false, true);
        pad->JustOutOfFrontEnd = 5;
        pad->LastTimeTouched = 0;
    }
    if (CPad* pad = CPad::GetPad(1))
        pad->LastTimeTouched = 0;

    CPad::ClearMouseHistory();
    CPad::OldKeyState = CPad::NewKeyState;
    ZeroAttackButtons(CPad::GetPad(0));

    // Stock SA recreates DirectInput mouse only for exclusive fullscreen.
    // Recreating it in windowed/borderless can leave a non-null but unacquired
    // device, after which CPad never retries and all game mouse input is lost.
    if (WindowMode::Query() == 1)
    {
        plugin::Call<Addr::DIReleaseMouse>();
        plugin::Call<Addr::diMouseInit, bool>(true);
    }

    s_suppressAttackUntilMs = GetTickCount() + kSuppressAttackMs;
}

void __fastcall HookManager::DrawFrontEnd_Main(CMenuManager* menu)
{
    if (!menu || menu->m_bShutDownFrontEndRequested)
        return;

    WindowMode::TickChrome();

    if (s_session && s_drawMain)
    {
        s_drawMain();
        return;
    }

    plugin::CallMethod<Addr::DrawFrontEnd>(menu);
}

void __fastcall HookManager::DrawFrontEnd_Pause(CMenuManager* menu)
{
    if (!menu || menu->m_bShutDownFrontEndRequested)
        return;

    WindowMode::TickChrome();

    if (menu->m_bGameNotLoaded)
    {
        plugin::CallMethod<Addr::DrawFrontEnd>(menu);
        return;
    }

    if (s_drawPause)
        s_drawPause();
}

int __fastcall HookManager::Process_Detour(CMenuManager* menu)
{
    WindowMode::Pump();
    // Own ESC-close while custom pause is up (stock needs SCREEN_PAUSE_MENU;
    // also UnloadTextures — we resume without that).
    const bool pause = menu && !menu->m_bGameNotLoaded && menu->m_bMenuActive;
    if (pause)
    {
        if (!s_pauseWasActive)
        {
            // Same ESC that opened the menu — don't resume this frame
            ClearEscJustPressed();
            s_pauseWasActive = true;
        }
        else if (CPad::NewKeyState.esc != 0 && CPad::OldKeyState.esc == 0)
        {
            ClearEscJustPressed(); // steal from CheckForMenuClosing

            // Fullscreen map owns ESC — leave pause open
            if (s_pauseMapFullscreen)
            {
                s_pauseMapFsEsc = true;
            }
            else
            {
                s_pauseEscClose = true;
                s_pauseWasActive = false;

                menu->m_bShutDownFrontEndRequested = false;
                menu->m_bMenuActive = false;
                menu->m_bSaveMenuActive = false;
                menu->m_bOnlySaveMenu = false;
                menu->m_bShowMouse = false;
                CTimer::EndUserPause();
                HideOsCursorNow();
                FlushControlsAfterFrontend();
                AudioEngine.ReportFrontendAudioEvent(AE_FRONTEND_BACK, 0.0f, 1.0f);
            }
        }
    }
    else
    {
        s_pauseWasActive = false;
    }

    return plugin::CallMethodAndReturn<int, Addr::Process>(menu);
}

void __fastcall HookManager::UserInput_SkipCustom(CMenuManager* menu)
{
    if (!menu)
        return;

    if (s_session && menu->m_bGameNotLoaded)
        return;

    if (!menu->m_bGameNotLoaded && menu->m_bMenuActive)
        return;

    plugin::CallMethod<Addr::UserInput>(menu);
}

void HookManager::Install(DrawFn drawMain, DrawFn drawPause)
{
    s_drawMain = drawMain;
    s_drawPause = drawPause;
    s_session = true;

    if (s_installed)
        return;

    plugin::patch::RedirectCall(Addr::Call_DrawFrontEnd_FrontendIdle, DrawFrontEnd_Main);
    plugin::patch::RedirectCall(Addr::Call_DrawFrontEnd_Idle, DrawFrontEnd_Pause);

    // Title: FrontendIdle may CALL Process directly
    if (const uintptr_t call = FindCallTo(Addr::FrontendIdle, 0x1B0, Addr::Process))
        plugin::patch::RedirectCall(call, Process_Detour);
    // In-game: Idle → CGame::Process → MenuManager::Process
    if (const uintptr_t call = FindCallTo(Addr::CGame_Process, 0x400, Addr::Process))
        plugin::patch::RedirectCall(call, Process_Detour);

    // After UpdatePads: strip leftover LMB→Circle (melee) until release
    if (const uintptr_t call = FindCallTo(Addr::CGame_Process, 0x80, Addr::UpdatePads))
        plugin::patch::RedirectCall(call, UpdatePads_Detour);

    if (const uintptr_t userInputCall = FindCallTo(Addr::Process, 0x80, Addr::UserInput))
        plugin::patch::RedirectCall(userInputCall, UserInput_SkipCustom);

    // Drain ESC-close UI (panels) when DrawFrontEnd is skipped after MenuActive=0
    plugin::Events::gameProcessEvent += []()
    {
        if (s_pauseEscClose && s_drawPause)
            s_drawPause();
    };

    s_installed = true;
}

void HookManager::InstallRadar(DrawFn drawRadar)
{
    s_drawRadar = drawRadar;
    if (s_drawRadar && !s_radarHook)
        s_radarHook = safetyhook::create_inline(
            reinterpret_cast<void*>(Addr::DrawRadar), DrawRadar_Detour);
}

void HookManager::InstallRadio(DrawFn drawRadio)
{
    s_drawRadio = drawRadio;
    if (s_drawRadio && !s_radioTextHook)
        s_radioTextHook = safetyhook::create_inline(
            reinterpret_cast<void*>(Addr::DisplayRadioStationName), DisplayRadioStationName_Detour);
}

void HookManager::InstallHelp(DrawFn drawHelp)
{
    HelpGxt::Install();
    s_drawHelp = drawHelp;
    if (s_drawHelp && !s_helpTextHook)
        s_helpTextHook = safetyhook::create_inline(
            reinterpret_cast<void*>(Addr::DrawHelpText), DrawHelpText_Detour);
}

void __cdecl HookManager::DrawHelpText_Detour()
{
    if (RadarConfig::GetUpdatedHelp() && s_drawHelp)
        s_drawHelp();
    else if (s_helpTextHook)
        s_helpTextHook.ccall<void>();
}

void __cdecl HookManager::DrawRadar_Detour()
{
#ifdef _DEBUG
    // Debug: stock mini-map (left) + custom 3D radar (offset X). Transforms stay
    // separate — StockRadarDraw hooks only remap while s_active.
    if (s_radarHook)
        s_radarHook.ccall<void>();
#endif
    if (s_drawRadar)
        s_drawRadar();
}

void __fastcall HookManager::DisplayRadioStationName_Detour(CAERadioTrackManager* self, void*)
{
    if (!RadarConfig::GetRadioText() || !s_drawRadio)
    {
        if (s_radioTextHook)
            s_radioTextHook.thiscall<void>(self);
        return;
    }

    if (!self)
        return;
    if (CTimer::m_UserPause || CTimer::m_CodePause)
        return;
    if (TheCamera.m_bWideScreenOn)
        return;
    if (!FindPlayerVehicle())
        return;
    if (CReplay::Mode == 1) // MODE_PLAYBACK
        return;

    // field_1 == m_bDisplayStationName — arm 2.5s window on retune
    if (self->field_1 && self->IsVehicleRadioActive())
    {
        self->m_nTimeToDisplayRadioName = CTimer::m_snTimeInMilliseconds + 2500;
        self->field_1 = 0;
    }

    if (CTimer::m_snTimeInMilliseconds >= self->m_nTimeToDisplayRadioName)
        return;

    s_drawRadio();
}

bool HookManager::IsCustomMainMenuSession() const
{
    return s_session && FrontEndMenuManager.m_bGameNotLoaded;
}

bool HookManager::IsCustomPauseSession() const
{
    if (FrontEndMenuManager.m_bGameNotLoaded || !FrontEndMenuManager.m_bMenuActive)
        return false;

    // In-game load keeps MenuActive for stock LOAD_FIRST_SAVE — custom pause
    // must not ShowOsCursor across that screen or the cursor sticks in-game.
    if (FrontEndMenuManager.m_bWantToLoad)
        return false;

    const char page = FrontEndMenuManager.m_nCurrentMenuPage;
    if (page == MENUPAGE_LOAD_FIRST_SAVE || page == MENUPAGE_GAME_LOADED)
        return false;

    return true;
}

bool HookManager::ConsumePauseEscClose()
{
    if (!s_pauseEscClose)
        return false;
    s_pauseEscClose = false;
    return true;
}

void HookManager::SetPauseMapFullscreen(bool enabled)
{
    s_pauseMapFullscreen = enabled;
    if (!enabled)
        s_pauseMapFsEsc = false;
}

bool HookManager::ConsumePauseMapFullscreenEsc()
{
    if (!s_pauseMapFsEsc)
        return false;
    s_pauseMapFsEsc = false;
    return true;
}

void HookManager::DismissCustomMainMenu()
{
    s_session = false;
    FrontEndMenuManager.m_bShowMouse = false;
    HideOsCursorNow();
}

void HookManager::RequestResumeGame()
{
    s_pauseEscClose = false;
    s_pauseWasActive = false;
    s_pauseMapFullscreen = false;
    s_pauseMapFsEsc = false;
    FrontEndMenuManager.m_bShutDownFrontEndRequested = false;
    FrontEndMenuManager.m_bMenuActive = false;
    FrontEndMenuManager.m_bSaveMenuActive = false;
    FrontEndMenuManager.m_bOnlySaveMenu = false;
    FrontEndMenuManager.m_bShowMouse = false;
    CTimer::EndUserPause();
    HideOsCursorNow();
    FlushControlsAfterFrontend();
}

void HookManager::RequestResumeGameFromEsc()
{
    RequestResumeGame();
    ClearEscJustPressed();
}

void HookManager::RequestNewGame()
{
    DismissCustomMainMenu();
    FrontEndMenuManager.DoSettingsBeforeStartingAGame();
}

void HookManager::RequestLoadGame(int slot)
{
    if (slot < 0 || slot >= 8)
        return;
    if (!CGenericGameStorage::CheckSlotDataValid(slot, false))
        return;

    DismissCustomMainMenu();
    FrontEndMenuManager.m_nSelectedSaveGame = static_cast<char>(slot);
    FrontEndMenuManager.SwitchToNewScreen(13);
}

void HookManager::TickSession() const
{
    if (!IsCustomMainMenuSession())
        return;

    FrontEndMenuManager.m_bMenuActive = true;
}

int HookManager::GetGameState() const
{
    return gGameState;
}
