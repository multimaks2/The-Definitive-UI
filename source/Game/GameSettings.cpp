/*****************************************************************************
 *
 *  PROJECT:     The-Definitive-UI
 *  FILE:        source/GameSettings/GameSettings.cpp
 *  PURPOSE:     Settings + control remap (shared by MainMenu and pMainMenu)
 *
 *****************************************************************************/

#include "GameSettings.h"

#include "plugin.h"
#include "RenderWare.h"
#include "Draw/Draw.h"
#include "InputManager.h"
#include "LanguageManager.h"
#include "Config.h"
#include "GpsRender.h"
#include "WindowMode.h"
#include "CMenuManager.h"
#include "CCamera.h"
#include "CVehicle.h"
#include "CAudioEngine.h"
#include "eAudioEvents.h"
#include "CControllerConfigManager.h"
#include "CRenderer.h"
#include "Fx_c.h"

#include <windows.h>
#include <cmath>
#include <cstring>
#include <string>

#ifndef GET_WHEEL_DELTA_WPARAM
#define GET_WHEEL_DELTA_WPARAM(wParam) ((short)HIWORD(wParam))
#endif

namespace
{
    const char* SettingsTabKeys[GameSettings::Layout::SettingsTabCount] = {
        "UI_TAB_CONTROLS", "UI_TAB_GAME", "UI_TAB_GRAPHICS", "UI_TAB_SOUND", "UI_TAB_OPTIONS"
    };

    const char* RadarModeKeys[] = {
        "RADAR_MAP_BLIPS", "RADAR_BLIPS", "RADAR_OFF"
    };

    const char* FxQualityKeys[] = {
        "FX_LOW", "FX_MED", "FX_HIGH", "FX_VERY"
    };

    const char* WindowModeKeys[] = {
        "WIN_WINDOWED", "WIN_FULLSCREEN", "WIN_BORDERLESS"
    };

    const char* AaNames[] = {
        "Off", "1x", "2x", "3x"
    };

    int CurrentWindowMode()
    {
        if (RadarConfig::HasWindowModeOverride())
            return RadarConfig::GetWindowMode();
        const int q = WindowMode::Query();
        if (q < 0 || q > 2)
            return 1;
        return q;
    }

    char** GameGetVideoModeList()
    {
        return plugin::CallAndReturn<char**, 0x745AF0>();
    }

    int NextValidVideoMode(int current, int dir)
    {
        const int n = RwEngineGetNumVideoModes();
        char** list = GameGetVideoModeList();
        if (!list || n <= 0)
            return current;

        int mode = current;
        if (mode < 0 || mode >= n)
            mode = 0;

        for (int i = 0; i < n; ++i)
        {
            mode += dir;
            if (mode >= n) mode = 0;
            if (mode < 0) mode = n - 1;
            if (list[mode] && list[mode][0])
                return mode;
        }
        return current;
    }

    bool SplitResDepth(const std::string& value, std::string& prefix, std::string& depth)
    {
        const auto pos = value.rfind(" X ");
        if (pos == std::string::npos)
            return false;
        prefix = value.substr(0, pos + 3);
        depth = value.substr(pos + 3);
        while (!depth.empty() && (depth.back() == ' ' || depth.back() == '\t'))
            depth.pop_back();
        return !depth.empty();
    }

    bool ParseModeString(const char* s, int& outW, int& outH, int& outDepth)
    {
        if (!s || !s[0])
            return false;
        int w = 0, h = 0, d = 0;
        // Stock: "1920 X 1080 X 32" — also accept x/× and tight forms
        if (sscanf_s(s, "%d X %d X %d", &w, &h, &d) != 3
            && sscanf_s(s, "%d x %d x %d", &w, &h, &d) != 3
            && sscanf_s(s, "%dx%dx%d", &w, &h, &d) != 3)
            return false;
        if (w < 640 || h < 480 || (d != 16 && d != 32))
            return false;
        outW = w;
        outH = h;
        outDepth = d;
        return true;
    }

    int ListedModeDepth(int mode)
    {
        char** list = GameGetVideoModeList();
        int w = 0, h = 0, d = 0;
        if (list && mode >= 0 && list[mode] && ParseModeString(list[mode], w, h, d))
            return d;
        RwVideoMode info{};
        if (!RwEngineGetVideoModeInfo(&info, mode))
            return 0;
        return static_cast<int>(info.depth);
    }

    int FindSameSizeDepth(int current, int wantDepth)
    {
        const int n = RwEngineGetNumVideoModes();
        char** list = GameGetVideoModeList();
        if (!list || n <= 0 || current < 0 || current >= n)
            return -1;

        int cw = 0, ch = 0, cd = 0;
        if (!list[current] || !ParseModeString(list[current], cw, ch, cd))
        {
            RwVideoMode info{};
            if (!RwEngineGetVideoModeInfo(&info, current))
                return -1;
            cw = info.width;
            ch = info.height;
            cd = static_cast<int>(info.depth);
        }
        if (wantDepth != 16 && wantDepth != 32)
            wantDepth = (cd >= 32) ? 16 : 32;

        for (int i = 0; i < n; ++i)
        {
            if (!list[i] || !list[i][0])
                continue;
            int w = 0, h = 0, d = 0;
            if (!ParseModeString(list[i], w, h, d))
                continue;
            if (w == cw && h == ch && d == wantDepth)
                return i;
        }
        return -1;
    }

    constexpr float kMouseSensMin = 1.0f / 3200.0f;
    constexpr float kMouseSensMax = 1.0f / 200.0f;
    constexpr float kMouseSensDefault = 1.0f / 200.0f * 0.5f;

    float Clampf(float v, float lo, float hi)
    {
        return (v < lo) ? lo : (v > hi) ? hi : v;
    }

    float SensTo01(float v)
    {
        return Clampf((v - kMouseSensMin) / (kMouseSensMax - kMouseSensMin), 0.0f, 1.0f);
    }

    float SensFrom01(float t)
    {
        return kMouseSensMin + Clampf(t, 0.0f, 1.0f) * (kMouseSensMax - kMouseSensMin);
    }

    void PlayFe(int ev)
    {
        AudioEngine.ReportFrontendAudioEvent(ev, 0.0f, 1.0f);
    }
}

void GameSettings::Bind(Draw* draw, LPDIRECT3DDEVICE9 device, bool* pendingVideo)
{
    m_pDraw = draw;
    m_pDevice = device;
    m_pendingVideo = pendingVideo;
}

void GameSettings::OnDeviceLost()
{
    m_pDraw = nullptr;
    m_pDevice = nullptr;
    CancelDrag();
}

void GameSettings::Open()
{
    m_settingsTab = SettingsTab::Controls;
    m_dragSetting = SettingId::Count;
    m_bControlsRemap = false;
    CancelRebind();
}

void GameSettings::Close()
{
    m_dragSetting = SettingId::Count;
    m_bControlsRemap = false;
    CancelRebind();
}

void GameSettings::CancelDrag()
{
    m_dragSetting = SettingId::Count;
}

void GameSettings::QueueVideoModeApply()
{
    int vid = FrontEndMenuManager.m_nDisplayVideoMode;
    if (vid < 0)
        vid = FrontEndMenuManager.m_nPrefsVideoMode;
    WindowMode::Request(CurrentWindowMode(), vid);
    if (m_pendingVideo)
        *m_pendingVideo = true;
}

bool GameSettings::GetCursorPosClient(float screenW, float screenH, float& outX, float& outY) const
{
    return Ui::GetCursorPosClient(screenW, screenH, outX, outY);
}

void GameSettings::DrawUiText(float left, float top, float right, float bottom, const char* text,
                              DWORD format, bool hovered, float screenW, float screenH,
                              bool onActivePlate, bool solidIdle)
{
    Ui::DrawMenuText(m_pDraw, left, top, right, bottom, text, format, hovered,
                     screenW, screenH, onActivePlate, solidIdle);
}

bool GameSettings::HandleClick(float cx, float cy, float screenW, float screenH, bool& outClosePanel)
{
    outClosePanel = false;

    for (int t = 0; t < Layout::SettingsTabCount; ++t)
    {
        if (GetTabRect(t, Layout::SettingsTabW, screenW, screenH).Contains(cx, cy))
        {
            PlayFe(AE_FRONTEND_SELECT);
            m_settingsTab = static_cast<SettingsTab>(t);
            m_dragSetting = SettingId::Count;
            m_bControlsRemap = false;
            CancelRebind();
            return true;
        }
    }

    if (m_bControlsRemap && m_settingsTab == SettingsTab::Controls)
    {
        if (IsRebindWaiting())
            return true;

        if (GetRemapFooterRect(0, screenW, screenH).Contains(cx, cy))
        {
            PlayFe(AE_FRONTEND_BACK);
            CancelRebind();
            m_bControlsRemap = false;
            return true;
        }
        if (GetRemapFooterRect(1, screenW, screenH).Contains(cx, cy))
        {
            PlayFe(AE_FRONTEND_SELECT);
            ResetControlBindings();
            return true;
        }
        return true;
    }

    int count = 0;
    const SettingDef* rows = GetSettingsForTab(m_settingsTab, count);
    for (int i = 0; i < count; ++i)
    {
        if (GetSlotRect(i, screenW, screenH).Contains(cx, cy))
        {
            OnSettingActivated(rows[i].id, cx, cy, screenW, screenH);
            return true;
        }
    }

    if (!GetPanelRect(screenW, screenH).Contains(cx, cy))
    {
        PlayFe(AE_FRONTEND_BACK);
        Close();
        outClosePanel = true;
        return true;
    }
    return false;
}

int GameSettings::HitTestHoverSoundId(float cursorX, float cursorY, float screenW, float screenH) const
{
    if (IsRebindWaiting())
        return -1;

    for (int t = 0; t < Layout::SettingsTabCount; ++t)
    {
        if (GetTabRect(t, Layout::SettingsTabW, screenW, screenH).Contains(cursorX, cursorY))
            return 400 + t;
    }
    if (m_bControlsRemap && m_settingsTab == SettingsTab::Controls)
    {
        if (GetRemapFooterRect(0, screenW, screenH).Contains(cursorX, cursorY))
            return 700;
        if (GetRemapFooterRect(1, screenW, screenH).Contains(cursorX, cursorY))
            return 701;
        return -1;
    }
    int count = 0;
    GetSettingsForTab(m_settingsTab, count);
    for (int i = 0; i < count; ++i)
    {
        if (GetSlotRect(i, screenW, screenH).Contains(cursorX, cursorY))
            return 500 + i;
    }
    return -1;
}

std::string GameSettings::KeyCodeToName(unsigned key)
{
    if (!key)
        return "—";

    switch (key)
    {
    case rsMOUSELEFTBUTTON: return LanguageManager::Get("FEC_MSL");
    case rsMOUSERIGHTBUTTON: return LanguageManager::Get("FEC_MSR");
    case rsMOUSEMIDDLEBUTTON: return LanguageManager::Get("FEC_MSM");
    case rsMOUSEWHEELUPBUTTON: return LanguageManager::Get("FEC_MWF");
    case rsMOUSEWHEELDOWNBUTTON: return LanguageManager::Get("FEC_MWB");
    case rsMOUSEX1BUTTON: return LanguageManager::Get("UI_KEY_MOUSE4");
    case rsMOUSEX2BUTTON: return LanguageManager::Get("UI_KEY_MOUSE5");
    case rsSPACE: return LanguageManager::Get("UI_KEY_SPACE");
    case rsTAB: return "TAB";
    case rsLSHIFT: return LanguageManager::Get("UI_KEY_LSHIFT");
    case rsRSHIFT: return LanguageManager::Get("UI_KEY_RSHIFT");
    case rsSHIFT: return "SHIFT";
    case rsLCTRL: return LanguageManager::Get("UI_KEY_LCTRL");
    case rsRCTRL: return LanguageManager::Get("UI_KEY_RCTRL");
    case rsLALT: return LanguageManager::Get("UI_KEY_LALT");
    case rsRALT: return LanguageManager::Get("UI_KEY_RALT");
    case rsENTER: return "ENTER";
    case rsBACKSP: return "BACKSPACE";
    case rsESC: return "ESC";
    case rsINS: return "INS";
    case rsDEL: return "DEL";
    case rsHOME: return "HOME";
    case rsEND: return "END";
    case rsPGUP: return "PGUP";
    case rsPGDN: return "PGDN";
    case rsUP: return "↑";
    case rsDOWN: return "↓";
    case rsLEFT: return "←";
    case rsRIGHT: return "→";
    case rsCAPSLK: return "CAPS";
    case rsNUMLOCK: return "NUM LOCK";
    case rsSCROLL: return "SCROLL";
    case rsPAUSE: return "PAUSE";
    case rsLWIN: return "LWIN";
    case rsRWIN: return "RWIN";
    case rsAPPS: return "MENU";
    case rsPADENTER: return "NUM ENTER";
    case rsDIVIDE: return "NUM /";
    case rsTIMES: return "NUM *";
    case rsPLUS: return "NUM +";
    case rsMINUS: return "NUM -";
    case rsPADDEL: return "NUM .";
    case rsPADINS: return "NUM 0";
    case rsPADEND: return "NUM 1";
    case rsPADDOWN: return "NUM 2";
    case rsPADPGDN: return "NUM 3";
    case rsPADLEFT: return "NUM 4";
    case rsPAD5: return "NUM 5";
    case rsPADRIGHT: return "NUM 6";
    case rsPADHOME: return "NUM 7";
    case rsPADUP: return "NUM 8";
    case rsPADPGUP: return "NUM 9";
    default:
        if (key >= rsF1 && key <= rsF12)
            return std::string("F") + std::to_string(key - rsF1 + 1);
        if (key >= 'A' && key <= 'Z')
            return std::string(1, static_cast<char>(key));
        if (key >= 'a' && key <= 'z')
            return std::string(1, static_cast<char>(key - 32));
        if (key >= '0' && key <= '9')
            return std::string(1, static_cast<char>(key));
        if (key > 32 && key < 127)
            return std::string(1, static_cast<char>(key));
        return "—";
    }
}

std::string GameSettings::FormatActionKey(int action) const
{
    if (action < 0 || action >= 59)
        return "—";

    // Own names only — GXT is not UTF-16 (RU GXT → Chinese garbage)
    const CControllerAction& act = ControlsManager.m_actions[action];
    unsigned key = act.keys[CONTROLLER_MOUSE].keyCode;
    if (!key) key = act.keys[CONTROLLER_KEYBOARD1].keyCode;
    if (!key) key = act.keys[CONTROLLER_KEYBOARD2].keyCode;
    return KeyCodeToName(key);
}

const GameSettings::RemapBind* GameSettings::GetRemapBinds(int col, int& outCount)
{
    // DE order, filtered to vanilla SA actions (plugin-sdk e_ControllerAction).
    // Skipped DE-only: weapon wheel, quick slots 1–3, map zoom, stats, radio wheel, nitro, hydraulics lock.
    static constexpr int kRadioTrackSkip = 28; // gap in plugin-sdk enum (= VEHICLE_RADIO_TRACK_SKIP)

    static const RemapBind kFoot[] = {
        { GO_FORWARD, "FEC_FOR" },
        { GO_LEFT, "FEC_LEF" },
        { GO_BACK, "FEC_BAC" },
        { GO_RIGHT, "FEC_RIG" },
        { PED_FIREWEAPON, "FEC_FIR" },
        { VEHICLE_ENTER_EXIT, "FEC_EEX" },
        { PED_SPRINT, "FEC_SPN" },
        { PED_JUMPING, "FEC_JMP" },
        { PED_CYCLE_WEAPON_RIGHT, "FEC_NWE" },
        { PED_CYCLE_WEAPON_LEFT, "FEC_PWE" },
        { PED_DUCK, "FEC_CRO" },
        { PED_FIREWEAPON_ALT, "FEC_FIA" },
        { PED_SNIPER_ZOOM_IN, "FEC_ZIN" },
        { PED_SNIPER_ZOOM_OUT, "FEC_ZOT" },
        { PED_LOOKBEHIND, "FEC_LBA" },
        { CAMERA_CHANGE_VIEW_ALL_SITUATIONS, "FEC_CMR" },
        { PED_LOCK_TARGET, "FEC_TAR" },
        { PED_CENTER_CAMERA_BEHIND_PLAYER, "FEC_CEN" },
        { CONVERSATION_YES, "FEC_COY" },
        { CONVERSATION_NO, "FEC_CON" },
        { SNEAK_ABOUT, "FEC_PDW" },
        { GROUP_CONTROL_FWD, "FEC_GPF" },
        { GROUP_CONTROL_BWD, "FEC_GPB" },
        { VEHICLE_STEERUP, "FEC_HCA" },
        { VEHICLE_STEERDOWN, "FEC_HCD" },
        { PED_ANSWER_PHONE, "FEC_ANP" },
        { PED_CYCLE_TARGET_LEFT, "FEC_PTT" },
        { PED_CYCLE_TARGET_RIGHT, "FEC_NTR" },
        { PED_1RST_PERSON_LOOK_LEFT, "FEC_TFL" },
        { PED_1RST_PERSON_LOOK_RIGHT, "FEC_TFR" },
        { PED_1RST_PERSON_LOOK_UP, "FEC_LUD" },
        { PED_1RST_PERSON_LOOK_DOWN, "FEC_LDU" },
    };

    static const RemapBind kVeh[] = {
        { VEHICLE_ACCELERATE, "FEC_ACC" },
        { VEHICLE_STEERLEFT, "FEC_LEF" },
        { VEHICLE_BRAKE, "FEC_BRA" },
        { VEHICLE_STEERRIGHT, "FEC_RIG" },
        { VEHICLE_ENTER_EXIT, "FEC_EEX" },
        { PED_SNIPER_ZOOM_IN, "FEC_ZIN" },
        { PED_SNIPER_ZOOM_OUT, "FEC_ZOT" },
        { VEHICLE_RADIO_STATION_UP, "FEC_RSC" },
        { VEHICLE_RADIO_STATION_DOWN, "FEC_RSP" },
        { kRadioTrackSkip, "FEC_RTS" },
        { VEHICLE_HANDBRAKE, "FEC_HND" },
        { VEHICLE_HORN, "FEC_HRN" },
        { VEHICLE_LOOKLEFT, "FEC_LOL" },
        { VEHICLE_LOOKRIGHT, "FEC_LOR" },
        { VEHICLE_LOOKBEHIND, "FEC_LBA" },
        { VEHICLE_FIREWEAPON, "FEC_FIR" },
        { VEHICLE_FIREWEAPON_ALT, "FEC_FIA" },
        { TOGGLE_SUBMISSIONS, "FEC_SUB" },
        { CAMERA_CHANGE_VIEW_ALL_SITUATIONS, "FEC_CMR" },
        { VEHICLE_TURRETUP, "FEC_TFU" },
        { VEHICLE_TURRETDOWN, "FEC_TFD" },
        { VEHICLE_TURRETLEFT, "FEC_TFL" },
        { VEHICLE_TURRETRIGHT, "FEC_TFR" },
        { VEHICLE_STEERUP, "FEC_PLU" },
        { VEHICLE_STEERDOWN, "FEC_PLD" },
        { VEHICLE_MOUSELOOK, "FEC_VML" },
        { PED_JUMPING, "FEC_JMP" },
        { PED_1RST_PERSON_LOOK_LEFT, "FEC_RL" },
        { PED_1RST_PERSON_LOOK_RIGHT, "FEC_RR" },
        { PED_1RST_PERSON_LOOK_UP, "FEC_HCA" },
        { PED_1RST_PERSON_LOOK_DOWN, "FEC_HCD" },
    };

    if (col == 0)
    {
        outCount = static_cast<int>(sizeof(kFoot) / sizeof(kFoot[0]));
        return kFoot;
    }
    outCount = static_cast<int>(sizeof(kVeh) / sizeof(kVeh[0]));
    return kVeh;
}

void GameSettings::ResetControlBindings()
{
    plugin::CallMethod<0x531F20, CControllerConfigManager*>(&ControlsManager);
    PersistSettings();
}

void GameSettings::GetRemapColumnRect(int col, float screenW, float screenH, float& outLeft, float& outRight) const
{
    const float sx = screenW / Layout::RefW;
    const float gap = Layout::RemapColGap * sx;
    const float panelL = Layout::PanelX * sx;
    const float panelR = (Layout::PanelX + Layout::PanelW) * sx;
    const float mid = (panelL + panelR) * 0.5f;
    if (col == 0)
    {
        outLeft = panelL;
        outRight = mid - gap * 0.5f;
    }
    else
    {
        outLeft = mid + gap * 0.5f;
        outRight = panelR;
    }
}

UiRect GameSettings::GetRemapHeaderRect(int col, float screenW, float screenH) const
{
    const float sy = screenH / Layout::RefH;
    float l = 0.0f, r = 0.0f;
    GetRemapColumnRect(col, screenW, screenH, l, r);
    const float top = Layout::ListTopY * sy;
    const float h = Layout::RemapHeaderH * sy;
    return { l, top, r, top + h };
}

UiRect GameSettings::GetRemapListClipRect(int col, float screenW, float screenH) const
{
    const float sy = screenH / Layout::RefH;
    float l = 0.0f, r = 0.0f;
    GetRemapColumnRect(col, screenW, screenH, l, r);
    const float listTop = Layout::ListTopY * sy;
    const float headerH = Layout::RemapHeaderH * sy;
    const float footerH = Layout::RemapFooterH * sy;
    const float footerGap = Layout::RemapFooterGap * sy;
    const float bodyTop = listTop + headerH;
    const float bodyBottom = (Layout::ListTopY + Layout::PanelH) * sy - footerH - footerGap;
    return { l, bodyTop, r, bodyBottom };
}

UiRect GameSettings::GetRemapRowRect(int col, int row, float screenW, float screenH) const
{
    const float sy = screenH / Layout::RefH;
    const ButtonRect clip = GetRemapListClipRect(col, screenW, screenH);
    const float rowH = Layout::RemapRowH * sy;
    const float scroll = (col >= 0 && col < 2) ? m_remapScrollDisplay[col] : 0.0f;
    const float top = clip.top + rowH * static_cast<float>(row) - scroll;
    return { clip.left, top, clip.right, top + rowH };
}

UiRect GameSettings::GetRemapFooterRect(int which, float screenW, float screenH) const
{
    const float sy = screenH / Layout::RefH;
    float l = 0.0f, r = 0.0f;
    GetRemapColumnRect(which, screenW, screenH, l, r);
    const float h = Layout::RemapFooterH * sy;
    const float bottom = (Layout::ListTopY + Layout::PanelH) * sy;
    return { l, bottom - h, r, bottom };
}

float GameSettings::GetRemapMaxScroll(int col, float screenW, float screenH) const
{
    int count = 0;
    GetRemapBinds(col, count);
    const float sy = screenH / Layout::RefH;
    const ButtonRect clip = GetRemapListClipRect(col, screenW, screenH);
    const float contentH = Layout::RemapRowH * sy * static_cast<float>(count);
    const float viewH = clip.bottom - clip.top;
    const float maxScroll = contentH - viewH;
    return (maxScroll > 0.0f) ? maxScroll : 0.0f;
}

void GameSettings::ClampRemapScroll(int col, float screenW, float screenH)
{
    if (col < 0 || col > 1)
        return;
    const float maxScroll = GetRemapMaxScroll(col, screenW, screenH);
    if (m_remapScroll[col] < 0.0f)
        m_remapScroll[col] = 0.0f;
    else if (m_remapScroll[col] > maxScroll)
        m_remapScroll[col] = maxScroll;
    if (m_remapScrollDisplay[col] < 0.0f)
        m_remapScrollDisplay[col] = 0.0f;
    else if (m_remapScrollDisplay[col] > maxScroll)
        m_remapScrollDisplay[col] = maxScroll;
}

int GameSettings::PollMouseWheelDelta() const
{
    // Prefer WndProc accumulator (same idea as ImGui/AZ2) — PeekMessage races the game pump
    if (InputManager* input = InputManager::GetInstance())
        return input->ConsumeMouseWheelDelta();

    int delta = 0;
    HWND hwnd = (RsGlobal.ps && RsGlobal.ps->window) ? RsGlobal.ps->window : nullptr;
    MSG msg{};
    while (PeekMessageW(&msg, hwnd, WM_MOUSEWHEEL, WM_MOUSEWHEEL, PM_REMOVE))
        delta += GET_WHEEL_DELTA_WPARAM(msg.wParam);
    return delta;
}

void GameSettings::UpdateRemapScroll(float screenW, float screenH)
{
    const bool lmb = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;

    if (!m_bControlsRemap || IsRebindWaiting())
    {
        m_remapLmbWas = lmb;
        return;
    }

    // Click that opened this tab must not start a rebind / drag
    if (m_remapSwallowLmb)
    {
        m_remapLmbWas = lmb;
        if (!lmb)
            m_remapSwallowLmb = false;
        // still allow wheel + inertia below — fall through without edges
    }

    const DWORD now = GetTickCount();
    float dt = 0.016f;
    if (m_remapLastTick != 0)
        dt = Clampf(static_cast<float>(now - m_remapLastTick) * 0.001f, 0.001f, 0.05f);
    m_remapLastTick = now;

    float cx = 0.0f, cy = 0.0f;
    GetCursorPosClient(screenW, screenH, cx, cy);
    const float sy = screenH / Layout::RefH;

    // Wheel always stacks (AZ2): rebase target to what's on screen so mid-coast input isn't blocked
    const int wheel = PollMouseWheelDelta();
    if (wheel != 0)
    {
        for (int col = 0; col < 2; ++col)
        {
            const ButtonRect clip = GetRemapListClipRect(col, screenW, screenH);
            if (!clip.Contains(cx, cy))
                continue;
            const float notches = static_cast<float>(wheel) / 120.0f;
            m_remapScroll[col] = m_remapScrollDisplay[col];
            m_remapVel[col] -= notches * Layout::RemapWheelImpulse * Layout::RemapWheelMult * sy;
            m_remapDragCol = -1;
        }
    }

    const bool lmbDown = !m_remapSwallowLmb && lmb && !m_remapLmbWas;
    const bool lmbUp = !m_remapSwallowLmb && !lmb && m_remapLmbWas;
    m_remapLmbWas = lmb;

    if (lmbDown)
    {
        m_remapPressCol = -1;
        m_remapPressRow = -1;
        m_remapPressMoved = false;
        m_remapPressY = cy;
        for (int col = 0; col < 2; ++col)
        {
            const ButtonRect clip = GetRemapListClipRect(col, screenW, screenH);
            if (!clip.Contains(cx, cy)
                || GetRemapFooterRect(col, screenW, screenH).Contains(cx, cy)
                || GetRemapHeaderRect(col, screenW, screenH).Contains(cx, cy))
                continue;

            m_remapDragCol = col;
            m_remapDragLastY = cy;
            m_remapVel[col] = 0.0f;
            // Snap display to target so drag starts from what's on screen
            m_remapScroll[col] = m_remapScrollDisplay[col];

            int bindCount = 0;
            const RemapBind* binds = GetRemapBinds(col, bindCount);
            for (int row = 0; row < bindCount; ++row)
            {
                if (GetRemapRowRect(col, row, screenW, screenH).Contains(cx, cy))
                {
                    m_remapPressCol = col;
                    m_remapPressRow = row;
                    break;
                }
            }
            break;
        }
    }

    if (lmb && m_remapDragCol >= 0 && m_remapDragCol < 2)
    {
        const float dy = m_remapDragLastY - cy;
        if (fabsf(cy - m_remapPressY) > 6.0f)
            m_remapPressMoved = true;
        if (m_remapPressMoved)
        {
            const int col = m_remapDragCol;
            const float maxScroll = GetRemapMaxScroll(col, screenW, screenH);
            m_remapScroll[col] += dy;
            if (m_remapScroll[col] < 0.0f)
                m_remapScroll[col] = 0.0f;
            else if (m_remapScroll[col] > maxScroll)
                m_remapScroll[col] = maxScroll;
            m_remapScrollDisplay[col] = m_remapScroll[col];
            const float instant = (dt > 0.0f) ? (dy / dt) : 0.0f;
            m_remapVel[col] = m_remapVel[col] * 0.55f + instant * 0.45f;
        }
        m_remapDragLastY = cy;
    }

    if (lmbUp)
    {
        if (!m_remapPressMoved && m_remapPressCol >= 0 && m_remapPressRow >= 0)
        {
            int bindCount = 0;
            const RemapBind* binds = GetRemapBinds(m_remapPressCol, bindCount);
            if (m_remapPressRow < bindCount)
                BeginRebind(binds[m_remapPressRow].action);
        }
        m_remapDragCol = -1;
        m_remapPressCol = -1;
        m_remapPressRow = -1;
        m_remapPressMoved = false;
    }

    for (int col = 0; col < 2; ++col)
    {
        if (m_remapDragCol == col)
            continue;

        const float maxScroll = GetRemapMaxScroll(col, screenW, screenH);

        if (fabsf(m_remapVel[col]) > 0.5f)
        {
            m_remapScroll[col] += m_remapVel[col] * dt;
            m_remapVel[col] *= expf(-Layout::RemapVelDamp * dt);
        }
        else
        {
            m_remapVel[col] = 0.0f;
        }

        if (m_remapScroll[col] < 0.0f)
        {
            m_remapScroll[col] = 0.0f;
            m_remapVel[col] = 0.0f;
        }
        else if (m_remapScroll[col] > maxScroll)
        {
            m_remapScroll[col] = maxScroll;
            m_remapVel[col] = 0.0f;
        }

        const float smoothK = 1.0f - expf(-Layout::RemapDisplaySmooth * dt);
        m_remapScrollDisplay[col] += (m_remapScroll[col] - m_remapScrollDisplay[col]) * smoothK;
        if (fabsf(m_remapScroll[col] - m_remapScrollDisplay[col]) < 0.25f)
            m_remapScrollDisplay[col] = m_remapScroll[col];
        if (m_remapScrollDisplay[col] < 0.0f)
            m_remapScrollDisplay[col] = 0.0f;
        else if (m_remapScrollDisplay[col] > maxScroll)
            m_remapScrollDisplay[col] = maxScroll;
    }
}

void GameSettings::BeginRebind(int action)
{
    m_rebindAction = action;
    m_rebindArmed = false;
    m_remapDragCol = -1;
    m_remapVel[0] = m_remapVel[1] = 0.0f;
}

void GameSettings::CancelRebind()
{
    m_rebindAction = -1;
    m_rebindArmed = false;
}

void GameSettings::ApplyRebindKey(unsigned rsKey, bool isMouse)
{
    if (m_rebindAction < 0 || !rsKey)
        return;

    const e_ControllerAction action = static_cast<e_ControllerAction>(m_rebindAction);
    if (isMouse)
    {
        plugin::CallMethod<0x531C90, CControllerConfigManager*, e_ControllerAction, unsigned, eControllerType>(
            &ControlsManager, action, rsKey, CONTROLLER_MOUSE);
        plugin::CallMethod<0x52F590, CControllerConfigManager*, e_ControllerAction, unsigned>(
            &ControlsManager, action, rsKey);
    }
    else
    {
        plugin::CallMethod<0x531C90, CControllerConfigManager*, e_ControllerAction, unsigned, eControllerType>(
            &ControlsManager, action, rsKey, CONTROLLER_KEYBOARD1);
        plugin::CallMethod<0x530490, CControllerConfigManager*, e_ControllerAction, unsigned, eControllerType>(
            &ControlsManager, action, rsKey, CONTROLLER_KEYBOARD1);
    }

    PersistSettings();
    CancelRebind();
}

void GameSettings::UpdateRebindCapture()
{
    if (!IsRebindWaiting())
        return;

    const bool anyMouse =
        (GetAsyncKeyState(VK_LBUTTON) & 0x8000) ||
        (GetAsyncKeyState(VK_RBUTTON) & 0x8000) ||
        (GetAsyncKeyState(VK_MBUTTON) & 0x8000) ||
        (GetAsyncKeyState(VK_XBUTTON1) & 0x8000) ||
        (GetAsyncKeyState(VK_XBUTTON2) & 0x8000);

    if (!m_rebindArmed)
    {
        // Wait until the click that opened rebind is fully released
        if (!anyMouse && !(GetAsyncKeyState(VK_ESCAPE) & 0x8000))
            m_rebindArmed = true;
        return;
    }

    if (GetAsyncKeyState(VK_ESCAPE) & 0x8000)
    {
        CancelRebind();
        return;
    }

    // Mouse buttons (edge: was up last frame stored simply via armed + current down)
    static bool s_prevL = false, s_prevR = false, s_prevM = false, s_prevX1 = false, s_prevX2 = false;
    const bool l = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
    const bool r = (GetAsyncKeyState(VK_RBUTTON) & 0x8000) != 0;
    const bool m = (GetAsyncKeyState(VK_MBUTTON) & 0x8000) != 0;
    const bool x1 = (GetAsyncKeyState(VK_XBUTTON1) & 0x8000) != 0;
    const bool x2 = (GetAsyncKeyState(VK_XBUTTON2) & 0x8000) != 0;

    auto edgeMouse = [&](bool now, bool& prev, unsigned code) {
        if (now && !prev)
        {
            ApplyRebindKey(code, true);
            prev = now;
            return true;
        }
        prev = now;
        return false;
    };

    if (edgeMouse(l, s_prevL, rsMOUSELEFTBUTTON)) return;
    if (edgeMouse(r, s_prevR, rsMOUSERIGHTBUTTON)) return;
    if (edgeMouse(m, s_prevM, rsMOUSEMIDDLEBUTTON)) return;
    if (edgeMouse(x1, s_prevX1, rsMOUSEX1BUTTON)) return;
    if (edgeMouse(x2, s_prevX2, rsMOUSEX2BUTTON)) return;

    // Mouse wheel as bind
    const int wheel = PollMouseWheelDelta();
    if (wheel > 0)
    {
        ApplyRebindKey(rsMOUSEWHEELUPBUTTON, true);
        return;
    }
    if (wheel < 0)
    {
        ApplyRebindKey(rsMOUSEWHEELDOWNBUTTON, true);
        return;
    }

    struct VkMap { int vk; unsigned rs; };
    static const VkMap kMap[] = {
        { VK_SPACE, rsSPACE }, { VK_TAB, rsTAB }, { VK_RETURN, rsENTER },
        { VK_BACK, rsBACKSP }, { VK_INSERT, rsINS }, { VK_DELETE, rsDEL },
        { VK_HOME, rsHOME }, { VK_END, rsEND }, { VK_PRIOR, rsPGUP }, { VK_NEXT, rsPGDN },
        { VK_UP, rsUP }, { VK_DOWN, rsDOWN }, { VK_LEFT, rsLEFT }, { VK_RIGHT, rsRIGHT },
        { VK_LSHIFT, rsLSHIFT }, { VK_RSHIFT, rsRSHIFT },
        { VK_LCONTROL, rsLCTRL }, { VK_RCONTROL, rsRCTRL },
        { VK_LMENU, rsLALT }, { VK_RMENU, rsRALT },
        { VK_CAPITAL, rsCAPSLK }, { VK_NUMLOCK, rsNUMLOCK }, { VK_SCROLL, rsSCROLL },
        { VK_PAUSE, rsPAUSE },
        { VK_NUMPAD0, rsPADINS }, { VK_NUMPAD1, rsPADEND }, { VK_NUMPAD2, rsPADDOWN },
        { VK_NUMPAD3, rsPADPGDN }, { VK_NUMPAD4, rsPADLEFT }, { VK_NUMPAD5, rsPAD5 },
        { VK_NUMPAD6, rsPADRIGHT }, { VK_NUMPAD7, rsPADHOME }, { VK_NUMPAD8, rsPADUP },
        { VK_NUMPAD9, rsPADPGUP }, { VK_DECIMAL, rsPADDEL },
        { VK_DIVIDE, rsDIVIDE }, { VK_MULTIPLY, rsTIMES }, { VK_ADD, rsPLUS }, { VK_SUBTRACT, rsMINUS },
        { VK_F1, rsF1 }, { VK_F2, rsF2 }, { VK_F3, rsF3 }, { VK_F4, rsF4 },
        { VK_F5, rsF5 }, { VK_F6, rsF6 }, { VK_F7, rsF7 }, { VK_F8, rsF8 },
        { VK_F9, rsF9 }, { VK_F10, rsF10 }, { VK_F11, rsF11 }, { VK_F12, rsF12 },
    };

    for (const auto& e : kMap)
    {
        if (GetAsyncKeyState(e.vk) & 0x8000)
        {
            ApplyRebindKey(e.rs, false);
            return;
        }
    }

    for (int vk = 'A'; vk <= 'Z'; ++vk)
    {
        if (GetAsyncKeyState(vk) & 0x8000)
        {
            ApplyRebindKey(static_cast<unsigned>(vk), false);
            return;
        }
    }
    for (int vk = '0'; vk <= '9'; ++vk)
    {
        if (GetAsyncKeyState(vk) & 0x8000)
        {
            ApplyRebindKey(static_cast<unsigned>(vk), false);
            return;
        }
    }
}

UiRect GameSettings::GetTabRect(int tab, float tabW, float screenW, float screenH) const
{
    const float sx = screenW / Layout::RefW;
    const float sy = screenH / Layout::RefH;
    const float x = (Layout::PanelX + (tabW + Layout::TabGapX) * static_cast<float>(tab)) * sx;
    const float y = Layout::TabTopY * sy;
    return { x, y, x + tabW * sx, y + Layout::TabH * sy };
}

UiRect GameSettings::GetSlotRect(int slot, float screenW, float screenH) const
{
    const float sx = screenW / Layout::RefW;
    const float sy = screenH / Layout::RefH;
    const float x = Layout::PanelX * sx;
    const float y = (Layout::ListTopY + Layout::RowH() * static_cast<float>(slot)) * sy;
    const float w = Layout::PanelW * sx;
    const float h = Layout::RowH() * sy;
    return { x, y, x + w, y + h };
}

UiRect GameSettings::GetPanelRect(float screenW, float screenH) const
{
    const float sx = screenW / Layout::RefW;
    const float sy = screenH / Layout::RefH;
    return {
        Layout::PanelX * sx,
        Layout::TabTopY * sy,
        (Layout::PanelX + Layout::PanelW) * sx,
        (Layout::ListTopY + Layout::PanelH) * sy
    };
}

UiRect GameSettings::GetSliderTrackRect(const ButtonRect& row, float screenW, float screenH) const
{
    const float sx = screenW / Layout::RefW;
    const float sy = screenH / Layout::RefH;
    const float pad = Layout::PanelPadX * sx;
    const float trackW = Layout::SliderTrackW * sx;
    const float trackH = Layout::SliderTrackH * sy;
    const float right = row.right - pad;
    const float left = right - trackW;
    const float cy = (row.top + row.bottom) * 0.5f;
    return { left, cy - trackH * 0.5f, right, cy + trackH * 0.5f };
}

const GameSettings::SettingDef* GameSettings::GetSettingsForTab(SettingsTab tab, int& outCount) const
{
    static const SettingDef kControlsRows[] = {
        { SettingId::RedefineControls, SettingKind::Action, "SET_REDEFINE" },
        { SettingId::InvertLookY,      SettingKind::Toggle, "SET_INVERT_Y" },
        { SettingId::InvertLookX,      SettingKind::Toggle, "SET_INVERT_X" },
        { SettingId::MouseSensX,       SettingKind::Slider, "SET_MOUSE_X" },
        { SettingId::MouseSensY,       SettingKind::Slider, "SET_MOUSE_Y" },
        { SettingId::MouseSteer,       SettingKind::Toggle, "SET_MOUSE_STEER" },
        { SettingId::MouseFly,         SettingKind::Toggle, "SET_MOUSE_FLY" },
        { SettingId::RestoreControls,  SettingKind::Action, "SET_RESTORE" },
    };
    static const SettingDef kGameRows[] = {
        { SettingId::ShowHud,     SettingKind::Toggle, "SET_SHOW_HUD" },
        { SettingId::RadarMode,   SettingKind::Cycle,  "SET_RADAR" },
        { SettingId::Widescreen,  SettingKind::Toggle, "SET_WIDESCREEN" },
        { SettingId::RestoreGame, SettingKind::Action, "SET_RESTORE" },
    };
    static const SettingDef kGraphicsRows[] = {
        { SettingId::Brightness,      SettingKind::Slider, "SET_BRIGHTNESS" },
        { SettingId::DrawDistance,    SettingKind::Slider, "SET_DRAW_DIST" },
        { SettingId::FrameLimiter,    SettingKind::Toggle, "SET_FRAME_LIMIT" },
        { SettingId::FxQuality,       SettingKind::Cycle,  "SET_FX_QUALITY" },
        { SettingId::MipMapping,      SettingKind::Toggle, "SET_MIP" },
        { SettingId::AntiAlias,       SettingKind::Cycle,  "SET_AA" },
        { SettingId::WindowMode,      SettingKind::Cycle,  "SET_WINDOW_MODE" },
        { SettingId::Resolution,      SettingKind::Cycle,  "SET_RESOLUTION" },
        { SettingId::HeatHaze,        SettingKind::Toggle, "SET_HEATHAZE" },
        { SettingId::SpeedBlur,       SettingKind::Toggle, "SET_SPEEDBLUR" },
        { SettingId::RestoreGraphics, SettingKind::Action, "SET_RESTORE" },
    };
    static const SettingDef kSoundRows[] = {
        { SettingId::SfxVolume,    SettingKind::Slider, "SET_SFX" },
        { SettingId::MusicVolume,  SettingKind::Slider, "SET_MUSIC" },
        { SettingId::RadioAuto,    SettingKind::Toggle, "SET_RADIO_AUTO" },
        { SettingId::RadioEq,      SettingKind::Toggle, "SET_RADIO_EQ" },
        { SettingId::RestoreSound, SettingKind::Action, "SET_RESTORE" },
    };
    static const SettingDef kOptionsRows[] = {
        { SettingId::Language,       SettingKind::Cycle,  "SET_LANGUAGE" },
        { SettingId::Subtitles,      SettingKind::Toggle, "SET_SUBTITLES" },
        { SettingId::UpdatedHelp,    SettingKind::Toggle, "SET_UPDATED_HELP" },
        { SettingId::RadioText,      SettingKind::Toggle, "SET_RADIO_TEXT" },
        { SettingId::Gps,            SettingKind::Toggle, "SET_GPS" },
        { SettingId::SavePhotos,     SettingKind::Toggle, "SET_PHOTOS" },
        { SettingId::DeIcons,        SettingKind::Toggle, "SET_DE_ICONS" },
        { SettingId::CustomRadarTxd, SettingKind::Toggle, "SET_CUSTOM_RADAR" },
        { SettingId::ShowGangZones,  SettingKind::Toggle, "SET_GANG_ZONES" },
        { SettingId::RestoreOptions, SettingKind::Action, "SET_RESTORE" },
    };

    switch (tab)
    {
    case SettingsTab::Controls:
        outCount = static_cast<int>(sizeof(kControlsRows) / sizeof(kControlsRows[0]));
        return kControlsRows;
    case SettingsTab::Game:
        outCount = static_cast<int>(sizeof(kGameRows) / sizeof(kGameRows[0]));
        return kGameRows;
    case SettingsTab::Graphics:
        outCount = static_cast<int>(sizeof(kGraphicsRows) / sizeof(kGraphicsRows[0]));
        return kGraphicsRows;
    case SettingsTab::Sound:
        outCount = static_cast<int>(sizeof(kSoundRows) / sizeof(kSoundRows[0]));
        return kSoundRows;
    case SettingsTab::Options:
        outCount = static_cast<int>(sizeof(kOptionsRows) / sizeof(kOptionsRows[0]));
        return kOptionsRows;
    default:
        outCount = 0;
        return nullptr;
    }
}

void GameSettings::PersistSettings()
{
    FrontEndMenuManager.SaveSettings();
}

float GameSettings::GetSlider01(SettingId id) const
{
    switch (id)
    {
    case SettingId::MouseSensX:
        return SensTo01(CCamera::m_fMouseAccelHorzntal);
    case SettingId::MouseSensY:
        return SensTo01(CCamera::m_fMouseAccelVertical);
    case SettingId::SfxVolume:
        return Clampf(FrontEndMenuManager.m_nPrefsSfxVolume / 64.0f, 0.0f, 1.0f);
    case SettingId::MusicVolume:
        return Clampf(FrontEndMenuManager.m_nPrefsMusicVolume / 64.0f, 0.0f, 1.0f);
    case SettingId::Brightness:
        return Clampf(FrontEndMenuManager.m_nPrefsBrightness / 384.0f, 0.0f, 1.0f);
    case SettingId::DrawDistance:
        return Clampf((FrontEndMenuManager.m_fPrefsLOD - 0.8f) / 1.0f, 0.0f, 1.0f);
    default:
        return 0.0f;
    }
}

void GameSettings::ApplySlider(SettingId id, float t01, bool persist)
{
    t01 = Clampf(t01, 0.0f, 1.0f);
    switch (id)
    {
    case SettingId::MouseSensX:
        CCamera::m_fMouseAccelHorzntal = SensFrom01(t01);
        break;
    case SettingId::MouseSensY:
        CCamera::m_fMouseAccelVertical = SensFrom01(t01);
        break;
    case SettingId::SfxVolume:
        FrontEndMenuManager.m_nPrefsSfxVolume = static_cast<char>(static_cast<int>(t01 * 64.0f + 0.5f));
        AudioEngine.SetEffectsMasterVolume(FrontEndMenuManager.m_nPrefsSfxVolume);
        break;
    case SettingId::MusicVolume:
        FrontEndMenuManager.m_nPrefsMusicVolume = static_cast<char>(static_cast<int>(t01 * 64.0f + 0.5f));
        AudioEngine.SetMusicMasterVolume(FrontEndMenuManager.m_nPrefsMusicVolume);
        break;
    case SettingId::Brightness:
        FrontEndMenuManager.m_nPrefsBrightness = static_cast<int>(t01 * 384.0f + 0.5f);
        break;
    case SettingId::DrawDistance:
        FrontEndMenuManager.m_fPrefsLOD = 0.8f + t01 * 1.0f;
        CRenderer::ms_lodDistScale = FrontEndMenuManager.m_fPrefsLOD;
        break;
    default:
        return;
    }
    if (persist)
        PersistSettings();
}

void GameSettings::ToggleSetting(SettingId id)
{
    switch (id)
    {
    case SettingId::InvertLookY:
        CMenuManager::bInvertMouseY = !CMenuManager::bInvertMouseY;
        break;
    case SettingId::InvertLookX:
        CMenuManager::bInvertMouseX = !CMenuManager::bInvertMouseX;
        break;
    case SettingId::MouseSteer:
        CVehicle::m_bEnableMouseSteering = !CVehicle::m_bEnableMouseSteering;
        break;
    case SettingId::MouseFly:
        CVehicle::m_bEnableMouseFlying = !CVehicle::m_bEnableMouseFlying;
        break;
    case SettingId::ShowHud:
        FrontEndMenuManager.m_bPrefsShowHud = !FrontEndMenuManager.m_bPrefsShowHud;
        break;
    case SettingId::Widescreen:
        FrontEndMenuManager.m_bPrefsUseWideScreen = !FrontEndMenuManager.m_bPrefsUseWideScreen;
        break;
    case SettingId::RadioAuto:
        FrontEndMenuManager.m_bPrefsRadioAutoSelect = !FrontEndMenuManager.m_bPrefsRadioAutoSelect;
        break;
    case SettingId::RadioEq:
        FrontEndMenuManager.m_bPrefsRadioEq = !FrontEndMenuManager.m_bPrefsRadioEq;
        break;
    case SettingId::Subtitles:
        FrontEndMenuManager.m_bPrefsShowSubtitles = !FrontEndMenuManager.m_bPrefsShowSubtitles;
        break;
    case SettingId::UpdatedHelp:
        RadarConfig::SetUpdatedHelp(!RadarConfig::GetUpdatedHelp());
        break;
    case SettingId::RadioText:
        RadarConfig::SetRadioText(!RadarConfig::GetRadioText());
        break;
    case SettingId::Gps:
    {
        const bool enabled = !RadarConfig::GetGps();
        RadarConfig::SetGps(enabled);
        GpsRenderer::SetPathfindingPatchesEnabled(enabled);
        break;
    }
    case SettingId::SavePhotos:
        FrontEndMenuManager.m_bPrefsSavePhotos = !FrontEndMenuManager.m_bPrefsSavePhotos;
        break;
    case SettingId::DeIcons:
        RadarConfig::SetDeIcons(!RadarConfig::GetDeIcons());
        break;
    case SettingId::CustomRadarTxd:
        RadarConfig::SetCustomRadarTxd(!RadarConfig::GetCustomRadarTxd());
        break;
    case SettingId::ShowGangZones:
        RadarConfig::SetShowGangZones(!RadarConfig::GetShowGangZones());
        break;
    case SettingId::MipMapping:
        FrontEndMenuManager.m_bPrefsMipMapping = !FrontEndMenuManager.m_bPrefsMipMapping;
        RwTextureSetMipmapping(FrontEndMenuManager.m_bPrefsMipMapping);
        break;
    case SettingId::FrameLimiter:
        FrontEndMenuManager.m_bPrefsVsync = !FrontEndMenuManager.m_bPrefsVsync;
        break;
    case SettingId::HeatHaze:
        RadarConfig::SetHeatHaze(!RadarConfig::GetHeatHaze());
        break;
    case SettingId::SpeedBlur:
        RadarConfig::SetSpeedBlur(!RadarConfig::GetSpeedBlur());
        break;
    default:
        return;
    }
    PersistSettings();
}

void GameSettings::CycleSetting(SettingId id, int dir)
{
    if (dir == 0)
        dir = 1;

    switch (id)
    {
    case SettingId::RadarMode:
    {
        int mode = FrontEndMenuManager.m_nPrefsRadarMode + dir;
        if (mode < 0) mode = 2;
        if (mode > 2) mode = 0;
        FrontEndMenuManager.m_nPrefsRadarMode = mode;
        break;
    }
    case SettingId::Language:
    {
        LanguageManager::CycleLanguage(dir);
        break;
    }
    case SettingId::FxQuality:
    {
        int q = static_cast<int>(g_fx.GetFxQuality()) + dir;
        if (q < 0) q = 3;
        if (q > 3) q = 0;
        g_fx.SetFxQuality(static_cast<FxQuality_e>(q));
        break;
    }
    case SettingId::AntiAlias:
    {
        int maxLevels = static_cast<int>(RwD3D9EngineGetMaxMultiSamplingLevels());
        if (maxLevels < 1) maxLevels = 1;
        if (maxLevels > 4) maxLevels = 4;
        int aa = FrontEndMenuManager.m_nPrefsAntiAliasing + dir;
        if (aa < 1) aa = maxLevels;
        if (aa > maxLevels) aa = 1;
        FrontEndMenuManager.m_nPrefsAntiAliasing = aa;
        FrontEndMenuManager.m_nPrefsAntiAliasingDisp = aa;
        QueueVideoModeApply();
        break;
    }
    case SettingId::Resolution:
    {
        int mode = FrontEndMenuManager.m_nDisplayVideoMode;
        if (mode < 0)
            mode = FrontEndMenuManager.m_nPrefsVideoMode;
        mode = NextValidVideoMode(mode, dir);
        if (mode != FrontEndMenuManager.m_nPrefsVideoMode)
        {
            FrontEndMenuManager.m_nDisplayVideoMode = mode;
            FrontEndMenuManager.m_nPrefsVideoMode = mode;
            const int d = ListedModeDepth(mode);
            if (d == 16 || d == 32)
                RadarConfig::SetColorDepth(d);
            QueueVideoModeApply();
        }
        break;
    }
    case SettingId::WindowMode:
    {
        int mode = CurrentWindowMode() + dir;
        if (mode < 0) mode = 2;
        if (mode > 2) mode = 0;
        RadarConfig::SetWindowMode(mode);
        QueueVideoModeApply();
        break;
    }
    default:
        return;
    }
    PersistSettings();
}

void GameSettings::RestoreTabDefaults(SettingsTab tab)
{
    switch (tab)
    {
    case SettingsTab::Controls:
        CMenuManager::bInvertMouseX = false;
        CMenuManager::bInvertMouseY = false;
        CCamera::m_fMouseAccelHorzntal = kMouseSensDefault;
        CCamera::m_fMouseAccelVertical = kMouseSensDefault;
        CVehicle::m_bEnableMouseSteering = false;
        CVehicle::m_bEnableMouseFlying = false;
        break;
    case SettingsTab::Game:
        FrontEndMenuManager.m_bPrefsShowHud = true;
        FrontEndMenuManager.m_nPrefsRadarMode = 0;
        FrontEndMenuManager.m_bPrefsUseWideScreen = false;
        break;
    case SettingsTab::Graphics:
        FrontEndMenuManager.m_nPrefsBrightness = 256;
        FrontEndMenuManager.m_fPrefsLOD = 1.2f;
        CRenderer::ms_lodDistScale = 1.2f;
        g_fx.SetFxQuality(FXQUALITY_HIGH);
        FrontEndMenuManager.m_nPrefsAntiAliasing = 1;
        FrontEndMenuManager.m_nPrefsAntiAliasingDisp = 1;
        FrontEndMenuManager.m_bPrefsMipMapping = true;
        RwTextureSetMipmapping(true);
        FrontEndMenuManager.m_bPrefsVsync = true;
        FrontEndMenuManager.m_nDisplayVideoMode = FrontEndMenuManager.m_nPrefsVideoMode;
        RadarConfig::SetHeatHaze(true);
        RadarConfig::SetSpeedBlur(true);
        RadarConfig::ClearWindowMode();
        QueueVideoModeApply();
        break;
    case SettingsTab::Sound:
        FrontEndMenuManager.m_nPrefsSfxVolume = 64;
        FrontEndMenuManager.m_nPrefsMusicVolume = 64;
        FrontEndMenuManager.m_bPrefsRadioAutoSelect = true;
        FrontEndMenuManager.m_bPrefsRadioEq = false;
        AudioEngine.SetEffectsMasterVolume(64);
        AudioEngine.SetMusicMasterVolume(64);
        break;
    case SettingsTab::Options:
        FrontEndMenuManager.m_nPrefsLanguage = 0;
        FrontEndMenuManager.m_bPrefsShowSubtitles = true;
        FrontEndMenuManager.m_bPrefsSavePhotos = true;
        LanguageManager::SetLanguage(LanguageManager::Lang::Russian);
        RadarConfig::SetUpdatedHelp(true);
        RadarConfig::SetRadioText(true);
        RadarConfig::SetGps(true);
        GpsRenderer::SetPathfindingPatchesEnabled(true);
        RadarConfig::SetDeIcons(true);
        RadarConfig::SetCustomRadarTxd(true);
        RadarConfig::SetShowGangZones(true);
        break;
    default:
        return;
    }
    PersistSettings();
}

std::string GameSettings::FormatSettingValue(SettingId id) const
{
    auto onOff = [](bool on) -> const char* { return on ? LanguageManager::Get("UI_ON") : LanguageManager::Get("UI_OFF"); };

    switch (id)
    {
    case SettingId::InvertLookY: return onOff(CMenuManager::bInvertMouseY);
    case SettingId::InvertLookX: return onOff(CMenuManager::bInvertMouseX);
    case SettingId::MouseSteer:  return onOff(CVehicle::m_bEnableMouseSteering);
    case SettingId::MouseFly:    return onOff(CVehicle::m_bEnableMouseFlying);
    case SettingId::ShowHud:     return onOff(FrontEndMenuManager.m_bPrefsShowHud);
    case SettingId::Widescreen:  return onOff(FrontEndMenuManager.m_bPrefsUseWideScreen);
    case SettingId::RadioAuto:   return onOff(FrontEndMenuManager.m_bPrefsRadioAutoSelect);
    case SettingId::RadioEq:     return onOff(FrontEndMenuManager.m_bPrefsRadioEq);
    case SettingId::Subtitles:   return onOff(FrontEndMenuManager.m_bPrefsShowSubtitles);
    case SettingId::UpdatedHelp: return onOff(RadarConfig::GetUpdatedHelp());
    case SettingId::RadioText:   return onOff(RadarConfig::GetRadioText());
    case SettingId::Gps:         return onOff(RadarConfig::GetGps());
    case SettingId::SavePhotos:  return onOff(FrontEndMenuManager.m_bPrefsSavePhotos);
    case SettingId::DeIcons:         return onOff(RadarConfig::GetDeIcons());
    case SettingId::CustomRadarTxd:  return onOff(RadarConfig::GetCustomRadarTxd());
    case SettingId::ShowGangZones:   return onOff(RadarConfig::GetShowGangZones());
    case SettingId::MipMapping:    return onOff(FrontEndMenuManager.m_bPrefsMipMapping);
    case SettingId::FrameLimiter:  return onOff(FrontEndMenuManager.m_bPrefsVsync);
    case SettingId::HeatHaze:      return onOff(RadarConfig::GetHeatHaze());
    case SettingId::SpeedBlur:     return onOff(RadarConfig::GetSpeedBlur());
    case SettingId::RadarMode:
    {
        int m = FrontEndMenuManager.m_nPrefsRadarMode;
        if (m < 0 || m > 2) m = 0;
        return LanguageManager::Get(RadarModeKeys[m]);
    }
    case SettingId::Language:
    {
        return LanguageManager::GetLanguageName(LanguageManager::GetLanguage());
    }
    case SettingId::FxQuality:
    {
        int q = static_cast<int>(g_fx.GetFxQuality());
        if (q < 0 || q > 3) q = 0;
        return LanguageManager::Get(FxQualityKeys[q]);
    }
    case SettingId::AntiAlias:
    {
        int aa = FrontEndMenuManager.m_nPrefsAntiAliasing;
        if (aa < 1) aa = 1;
        if (aa > 4) aa = 4;
        if (aa == 1)
            return LanguageManager::Get("UI_OFF");
        return AaNames[aa - 1];
    }
    case SettingId::Resolution:
    {
        char** modes = GameGetVideoModeList();
        int idx = FrontEndMenuManager.m_nDisplayVideoMode;
        if (idx < 0)
            idx = FrontEndMenuManager.m_nPrefsVideoMode;
        if ((!modes || idx < 0 || !modes[idx] || !modes[idx][0]) && FrontEndMenuManager.m_nPrefsVideoMode >= 0)
            idx = FrontEndMenuManager.m_nPrefsVideoMode;

        int w = 0, h = 0, d = 0;
        if (modes && idx >= 0 && modes[idx] && ParseModeString(modes[idx], w, h, d))
        {
            if (RadarConfig::HasColorDepth())
                d = RadarConfig::GetColorDepth();
            char buf[64];
            sprintf_s(buf, "%d X %d X %d", w, h, d);
            return buf;
        }
        if (modes && idx >= 0 && modes[idx] && modes[idx][0])
            return modes[idx];
        return "—";
    }
    case SettingId::WindowMode:
    {
        int m = CurrentWindowMode();
        if (m < 0 || m > 2) m = 1;
        return LanguageManager::Get(WindowModeKeys[m]);
    }
    default:
        return {};
    }
}

void GameSettings::GetCycleArrowRects(const ButtonRect& row, float screenW, float screenH,
                                  const char* valueText,
                                  ButtonRect& outLeft, ButtonRect& outRight,
                                  float* outValueLeft, float* outValueRight) const
{
    const float sx = screenW / Layout::RefW;
    const float pad = Layout::PanelPadX * sx;
    const float gap = Layout::CycleArrowGap * sx;
    const float areaR = row.right - pad;

    float valueW = 40.0f * sx;
    float arrowW = Layout::CycleArrowW * sx;
    if (m_pDraw)
    {
        if (valueText && valueText[0])
            valueW = m_pDraw->GetTextWidth(valueText, 1.0f);
        const float aw = m_pDraw->GetTextWidth("<", 1.0f);
        if (aw > 1.0f)
            arrowW = aw + 4.0f * sx; // tight hit — no steal from neighbor/value
    }

    const float clusterW = arrowW + gap + valueW + gap + arrowW;
    const float areaL = areaR - clusterW;
    // Vertical hit band inset so row edges don't fight
    const float insetY = 4.0f * (screenH / Layout::RefH);
    const float top = row.top + insetY;
    const float bottom = row.bottom - insetY;

    outLeft = { areaL, top, areaL + arrowW, bottom };
    outRight = { areaR - arrowW, top, areaR, bottom };
    if (outValueLeft)
        *outValueLeft = outLeft.right + gap;
    if (outValueRight)
        *outValueRight = outRight.left - gap;
}

bool GameSettings::GetResolutionDepthHit(const ButtonRect& row, float screenW, float screenH,
                                         ButtonRect& outDepth) const
{
    outDepth = {};
    if (!m_pDraw)
        return false;
    const std::string value = FormatSettingValue(SettingId::Resolution);
    std::string prefix, depth;
    if (!SplitResDepth(value, prefix, depth))
        return false;

    ButtonRect leftArr{}, rightArr{};
    float valueL = 0.0f, valueR = 0.0f;
    GetCycleArrowRects(row, screenW, screenH, value.c_str(), leftArr, rightArr, &valueL, &valueR);

    const float fullW = m_pDraw->GetTextWidth(value.c_str(), 1.0f);
    const float prefixW = m_pDraw->GetTextWidth(prefix.c_str(), 1.0f);
    if (fullW < 1.0f || prefixW < 1.0f || prefixW >= fullW)
        return false;

    const float startX = valueL + (valueR - valueL - fullW) * 0.5f;
    const float padX = 10.0f * (screenW / Layout::RefW);
    const float hitR = startX + fullW + padX;
    outDepth = {
        startX + prefixW - padX,
        row.top,
        hitR > valueR ? hitR : valueR,
        row.bottom
    };
    return outDepth.right > outDepth.left;
}

void GameSettings::ToggleResolutionBitDepth()
{
    int mode = FrontEndMenuManager.m_nDisplayVideoMode;
    if (mode < 0)
        mode = FrontEndMenuManager.m_nPrefsVideoMode;

    int w = 0, h = 0, d = 0;
    char** list = GameGetVideoModeList();
    if (list && mode >= 0 && list[mode] && ParseModeString(list[mode], w, h, d))
    {
        if (RadarConfig::HasColorDepth())
            d = RadarConfig::GetColorDepth();
    }
    else
    {
        RwVideoMode info{};
        if (!RwEngineGetVideoModeInfo(&info, mode))
            return;
        w = info.width;
        h = info.height;
        d = RadarConfig::HasColorDepth()
            ? RadarConfig::GetColorDepth()
            : static_cast<int>(info.depth);
    }
    if (d != 16 && d != 32)
        d = 32;

    const int want = (d >= 32) ? 16 : 32;
    const int twin = FindSameSizeDepth(mode, want);
    if (twin >= 0)
        mode = twin;

    FrontEndMenuManager.m_nDisplayVideoMode = mode;
    FrontEndMenuManager.m_nPrefsVideoMode = mode;
    RadarConfig::SetColorDepth(want);
    if (w >= 640 && h >= 480)
        RadarConfig::SetWindowResolution(w, h, want);
    QueueVideoModeApply();
    PersistSettings();
}

void GameSettings::OnSettingActivated(SettingId id, float cursorX, float cursorY, float screenW, float screenH)
{
    int count = 0;
    const SettingDef* rows = GetSettingsForTab(m_settingsTab, count);
    const SettingDef* def = nullptr;
    int rowIndex = -1;
    for (int i = 0; i < count; ++i)
    {
        if (rows[i].id == id)
        {
            def = &rows[i];
            rowIndex = i;
            break;
        }
    }
    if (!def)
        return;

    switch (def->kind)
    {
    case SettingKind::Toggle:
        ToggleSetting(id);
        PlayFe(AE_FRONTEND_SELECT);
        break;
    case SettingKind::Cycle:
    {
        ButtonRect leftArr{}, rightArr{};
        const ButtonRect row = GetSlotRect(rowIndex, screenW, screenH);
        const float sy = screenH / Layout::RefH;
        const int baseFont = static_cast<int>(Layout::PanelFont * sy + 0.5f);
        if (m_pDraw)
            m_pDraw->EnsureFontHeight(baseFont > 0 ? baseFont : 1);
        const std::string value = FormatSettingValue(id);
        GetCycleArrowRects(row, screenW, screenH, value.c_str(), leftArr, rightArr);
        if (id == SettingId::Resolution)
        {
            ButtonRect depthHit{};
            if (GetResolutionDepthHit(row, screenW, screenH, depthHit)
                && depthHit.Contains(cursorX, cursorY))
            {
                const int beforeDepth = RadarConfig::HasColorDepth()
                    ? RadarConfig::GetColorDepth() : 0;
                ToggleResolutionBitDepth();
                const int afterDepth = RadarConfig::HasColorDepth()
                    ? RadarConfig::GetColorDepth() : 0;
                if (afterDepth != beforeDepth)
                    PlayFe(AE_FRONTEND_SELECT);
                break;
            }
        }
        const float midY = (row.top + row.bottom) * 0.5f;
        // < = назад, > = вперёд; клик по подписи/значению — вперёд
        const int dir = leftArr.Contains(cursorX, midY) ? -1 : 1;
        CycleSetting(id, dir);
        PlayFe(AE_FRONTEND_SELECT);
        break;
    }
    case SettingKind::Slider:
    {
        const ButtonRect row = GetSlotRect(rowIndex, screenW, screenH);
        const ButtonRect track = GetSliderTrackRect(row, screenW, screenH);
        const float t = (track.right > track.left)
            ? Clampf((cursorX - track.left) / (track.right - track.left), 0.0f, 1.0f)
            : 0.0f;
        ApplySlider(id, t, false);
        m_dragSetting = id;
        PlayFe(id == SettingId::SfxVolume ? AE_FRONTEND_NOISE_TEST : AE_FRONTEND_SELECT);
        break;
    }
    case SettingKind::Action:
        if (id == SettingId::RedefineControls)
        {
            m_bControlsRemap = true;
            m_remapScroll[0] = m_remapScroll[1] = 0.0f;
            m_remapScrollDisplay[0] = m_remapScrollDisplay[1] = 0.0f;
            m_remapVel[0] = m_remapVel[1] = 0.0f;
            m_remapDragCol = -1;
            m_remapPressCol = -1;
            m_remapPressRow = -1;
            m_remapPressMoved = false;
            m_remapLmbWas = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
            m_remapSwallowLmb = m_remapLmbWas; // swallow the open click
            CancelRebind();
            PlayFe(AE_FRONTEND_SELECT);
            break;
        }
        if (id == SettingId::RestoreControls)
            RestoreTabDefaults(SettingsTab::Controls);
        else if (id == SettingId::RestoreGame)
            RestoreTabDefaults(SettingsTab::Game);
        else if (id == SettingId::RestoreGraphics)
            RestoreTabDefaults(SettingsTab::Graphics);
        else if (id == SettingId::RestoreSound)
            RestoreTabDefaults(SettingsTab::Sound);
        else if (id == SettingId::RestoreOptions)
            RestoreTabDefaults(SettingsTab::Options);
        PlayFe(AE_FRONTEND_SELECT);
        break;
    }
}

void GameSettings::HandleSliderDrag(float screenW, float screenH)
{
    const bool lmb = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
    if (m_dragSetting == SettingId::Count)
        return;

    if (!lmb)
    {
        PersistSettings();
        m_dragSetting = SettingId::Count;
        return;
    }

    float cx = 0.0f, cy = 0.0f;
    if (!GetCursorPosClient(screenW, screenH, cx, cy))
        return;

    int count = 0;
    const SettingDef* rows = GetSettingsForTab(m_settingsTab, count);
    for (int i = 0; i < count; ++i)
    {
        if (rows[i].id != m_dragSetting)
            continue;
        const ButtonRect row = GetSlotRect(i, screenW, screenH);
        const ButtonRect track = GetSliderTrackRect(row, screenW, screenH);
        const float t = (track.right > track.left)
            ? Clampf((cx - track.left) / (track.right - track.left), 0.0f, 1.0f)
            : 0.0f;
        ApplySlider(m_dragSetting, t, false);
        return;
    }
}

void GameSettings::DrawSettingRow(int row, const SettingDef& def, float screenW, float screenH,
                              float cursorX, float cursorY)
{
    const ButtonRect box = GetSlotRect(row, screenW, screenH);
    const bool hot = box.Contains(cursorX, cursorY);
    DWORD rowBg = (row & 1) ? Layout::SlotZebraB : Layout::SlotZebraA;
    if (hot)
        rowBg = Layout::SlotHoverBg;

    const float sx = screenW / Layout::RefW;
    const float sy = screenH / Layout::RefH;
    const float pad = Layout::PanelPadX * sx;
    const float valueW = Layout::ValueAreaW * sx;
    const float h = box.bottom - box.top;
    const float labelR = box.right - pad - valueW;
    const float rowW = box.right - box.left;
    const DWORD fmtRow = DT_LEFT | DT_VCENTER | DT_NOCLIP | DT_SINGLELINE;

    // Action / restore: full-row cutout
    if (def.kind == SettingKind::Action)
    {
        m_pDraw->DrawRectCutoutText(box.left, box.top, rowW, h, rowBg,
                                    box.left + pad, box.top, box.right - pad, box.bottom,
                                    LanguageManager::Get(def.label), fmtRow);
        return;
    }

    if (def.kind == SettingKind::Slider)
    {
        // Full-row cutout (no mid seam) + slider on top of right side
        m_pDraw->DrawRectCutoutText(box.left, box.top, rowW, h, rowBg,
                                    box.left + pad, box.top, labelR, box.bottom,
                                    LanguageManager::Get(def.label), fmtRow);

        const ButtonRect track = GetSliderTrackRect(box, screenW, screenH);
        const float tw = track.right - track.left;
        const float th = track.bottom - track.top;
        const float t = GetSlider01(def.id);

        m_pDraw->DrawRect(track.left, track.top, tw, th, Layout::SliderTrack);
        m_pDraw->DrawRect(track.left, track.top, tw * t, th, Layout::SliderFill);

        const float knobR = Layout::SliderKnobR * sy;
        const float kx = track.left + tw * t;
        const float ky = (track.top + track.bottom) * 0.5f;
        m_pDraw->DrawCircleAA(kx, ky, knobR, Layout::SliderKnob);
        m_pDraw->DrawCircleAA(kx, ky, knobR * 0.28f, 0xFF000000);
    }
    else if (def.kind == SettingKind::Toggle || def.kind == SettingKind::Cycle)
    {
        const int baseFont = static_cast<int>(Layout::PanelFont * sy + 0.5f);
        const int hoverFont = static_cast<int>(baseFont * Layout::CycleArrowScale + 0.5f);
        m_pDraw->EnsureFontHeight(baseFont > 0 ? baseFont : 1);
        m_pDraw->EnsureHoverFontHeight(hoverFont > 0 ? hoverFont : 1);

        const std::string value = FormatSettingValue(def.id);
        ButtonRect leftArr{}, rightArr{};
        float valueL = 0.0f, valueR = 0.0f;
        GetCycleArrowRects(box, screenW, screenH, value.c_str(), leftArr, rightArr, &valueL, &valueR);
        const bool hotL = leftArr.Contains(cursorX, cursorY);
        const bool hotR = rightArr.Contains(cursorX, cursorY);

        ID3DXFont* baseF = m_pDraw->GetFont();
        ID3DXFont* hoverF = m_pDraw->GetHoverFont();
        ID3DXFont* leftF = (hotL && hoverF) ? hoverF : baseF;
        ID3DXFont* rightF = (hotR && hoverF) ? hoverF : baseF;

        ButtonRect depthHit{};
        const bool hotDepth = (def.id == SettingId::Resolution)
            && GetResolutionDepthHit(box, screenW, screenH, depthHit)
            && depthHit.Contains(cursorX, cursorY);

        if (hotDepth)
        {
            std::string prefix, depth;
            SplitResDepth(value, prefix, depth);
            const float fullW = m_pDraw->GetTextWidth(value.c_str(), 1.0f);
            const float prefixW = m_pDraw->GetTextWidth(prefix.c_str(), 1.0f);
            const float startX = valueL + (valueR - valueL - fullW) * 0.5f;
            const DWORD fmtLeft = DT_LEFT | DT_VCENTER | DT_NOCLIP | DT_SINGLELINE;
            m_pDraw->DrawRectCutoutCycleValue(box.left, box.top, rowW, h, rowBg,
                                              box.left + pad, labelR, LanguageManager::Get(def.label),
                                              leftArr.left, leftArr.right, "<", leftF,
                                              startX, startX + prefixW, prefix.c_str(),
                                              rightArr.left, rightArr.right, ">", rightF,
                                              box.top, box.bottom, fmtLeft);
            m_pDraw->DrawString(startX + prefixW, box.top, startX + fullW + 8.0f, box.bottom,
                                0xFFFFFFFF, depth.c_str(), 1.0f, 1.0f, fmtLeft, false);
        }
        else
        {
            // One full-row cutout — no center zebra seam
            m_pDraw->DrawRectCutoutCycleValue(box.left, box.top, rowW, h, rowBg,
                                              box.left + pad, labelR, LanguageManager::Get(def.label),
                                              leftArr.left, leftArr.right, "<", leftF,
                                              valueL, valueR, value.c_str(),
                                              rightArr.left, rightArr.right, ">", rightF,
                                              box.top, box.bottom);
        }
    }
}

void GameSettings::Render(float screenW, float screenH)
{
    if (!m_pDraw)
        return;

    const float sx = screenW / Layout::RefW;
    const float sy = screenH / Layout::RefH;
    const float tabPadX = Layout::TabPadX * sx;

    float cursorX = 0.0f, cursorY = 0.0f;
    GetCursorPosClient(screenW, screenH, cursorX, cursorY);

    const int fontH = static_cast<int>(Layout::PanelFont * sy + 0.5f);
    m_pDraw->EnsureFontHeight(fontH > 0 ? fontH : 1);

    for (int t = 0; t < Layout::SettingsTabCount; ++t)
    {
        const ButtonRect tab = GetTabRect(t, Layout::SettingsTabW, screenW, screenH);
        const bool active = (static_cast<int>(m_settingsTab) == t);
        const bool hot = tab.Contains(cursorX, cursorY);
        const float tw = tab.right - tab.left;
        const float th = tab.bottom - tab.top;

        if (active)
        {
            m_pDraw->DrawRect(tab.left, tab.top, tw, th, Layout::TabActiveBg);
            DrawUiText(tab.left + tabPadX, tab.top, tab.right - tabPadX, tab.bottom,
                       LanguageManager::Get(SettingsTabKeys[t]), DT_CENTER | DT_VCENTER | DT_NOCLIP | DT_SINGLELINE,
                       false, screenW, screenH, true);
        }
        else
        {
            // Hover: only rect alpha; cutout text unchanged
            const DWORD bg = hot ? Layout::TabHoverBg : Layout::TabIdleBg;
            m_pDraw->DrawRectCutoutText(tab.left, tab.top, tw, th, bg,
                                        tab.left + tabPadX, tab.top, tab.right - tabPadX, tab.bottom,
                                        LanguageManager::Get(SettingsTabKeys[t]), DT_CENTER | DT_VCENTER | DT_NOCLIP | DT_SINGLELINE);
        }
    }

    int count = 0;
    const SettingDef* rows = GetSettingsForTab(m_settingsTab, count);

    if (m_bControlsRemap && m_settingsTab == SettingsTab::Controls)
    {
        DrawControlsRemap(screenW, screenH, cursorX, cursorY);
        return;
    }

    for (int s = 0; s < Layout::SlotCount; ++s)
    {
        if (s < count)
        {
            DrawSettingRow(s, rows[s], screenW, screenH, cursorX, cursorY);
        }
        else
        {
            const ButtonRect box = GetSlotRect(s, screenW, screenH);
            const DWORD rowBg = (s & 1) ? Layout::SlotZebraB : Layout::SlotZebraA;
            m_pDraw->DrawRect(box.left, box.top, box.right - box.left, box.bottom - box.top, rowBg);
        }
    }
}

void GameSettings::DrawControlsRemap(float screenW, float screenH, float cursorX, float cursorY)
{
    if (!m_pDraw || !m_pDevice)
        return;

    UpdateRebindCapture();
    if (!IsRebindWaiting())
        UpdateRemapScroll(screenW, screenH);

    const float sx = screenW / Layout::RefW;
    const float sy = screenH / Layout::RefH;
    const float pad = Layout::PanelPadX * sx;

    const int fontH = static_cast<int>(Layout::RemapFont * sy + 0.5f);
    m_pDraw->EnsureFontHeight(fontH > 0 ? fontH : 1);

    m_nRemapHoverCol = -1;
    m_nRemapHoverRow = -1;

    static const char* headers[2] = { "FET_CFT", "FET_CCR" };

    for (int col = 0; col < 2; ++col)
    {
        const ButtonRect hdr = GetRemapHeaderRect(col, screenW, screenH);
        m_pDraw->DrawRectCutoutText(hdr.left, hdr.top, hdr.right - hdr.left, hdr.bottom - hdr.top,
                                    Layout::SlotZebraB, hdr.left, hdr.top, hdr.right, hdr.bottom,
                                    LanguageManager::Get(headers[col]), DT_CENTER | DT_VCENTER | DT_NOCLIP | DT_SINGLELINE);

        const ButtonRect clip = GetRemapListClipRect(col, screenW, screenH);
        const float rowH = Layout::RemapRowH * sy;
        const float viewH = clip.bottom - clip.top;
        const float clipW = clip.right - clip.left;
        const int fillerRows = static_cast<int>(ceilf((viewH + fabsf(m_remapScrollDisplay[col])) / rowH)) + 2;

        int bindCount = 0;
        const RemapBind* binds = GetRemapBinds(col, bindCount);
        const int drawRows = (bindCount > fillerRows) ? bindCount : fillerRows;

        const bool clipped = m_pDraw->BeginClipRT(clip.left, clip.top, clipW, viewH);

        for (int row = 0; row < drawRows; ++row)
        {
            const ButtonRect box = GetRemapRowRect(col, row, screenW, screenH);
            if (box.bottom < clip.top || box.top > clip.bottom)
                continue;

            // Local coords inside clip RT (viewport clips partial rows → float scroll)
            const float lx = clipped ? (box.left - clip.left) : box.left;
            const float ly = clipped ? (box.top - clip.top) : box.top;
            const float rx = lx + (box.right - box.left);
            const float by = ly + (box.bottom - box.top);

            const bool hasBind = (row < bindCount);
            const bool hot = hasBind && !IsRebindWaiting()
                             && clip.Contains(cursorX, cursorY) && box.Contains(cursorX, cursorY)
                             && m_remapDragCol < 0;
            if (hot)
            {
                m_nRemapHoverCol = col;
                m_nRemapHoverRow = row;
            }

            // α200 / α255 zebra — solid white text in cells (not cutout)
            DWORD bg = (row & 1) ? Layout::SlotZebraB : Layout::SlotZebraA;
            if (hot)
                bg = Layout::SlotHoverBg;

            if (!hasBind)
            {
                m_pDraw->DrawRect(lx, ly, rx - lx, by - ly, bg);
                continue;
            }

            const float mid = (lx + rx) * 0.5f;
            const float localPad = pad;
            const std::string key = FormatActionKey(binds[row].action);
            m_pDraw->DrawRect(lx, ly, rx - lx, by - ly, bg);
            m_pDraw->DrawString(lx + localPad, ly, mid, by, 0xFFFFFFFF,
                                LanguageManager::Get(binds[row].label), 1.0f, 1.0f,
                                DT_LEFT | DT_VCENTER | DT_NOCLIP | DT_SINGLELINE, false);
            m_pDraw->DrawString(mid, ly, rx - localPad, by, 0xFFFFFFFF, key.c_str(), 1.0f, 1.0f,
                                DT_RIGHT | DT_VCENTER | DT_NOCLIP | DT_SINGLELINE, false);
        }

        if (clipped)
            m_pDraw->EndClipRT();
    }

    {
        const ButtonRect back = GetRemapFooterRect(0, screenW, screenH);
        const bool hotBack = !IsRebindWaiting() && back.Contains(cursorX, cursorY);
        if (hotBack)
        {
            m_pDraw->DrawRect(back.left, back.top, back.right - back.left, back.bottom - back.top, Layout::TabActiveBg);
            DrawUiText(back.left, back.top, back.right, back.bottom, LanguageManager::Get("UI_BACK"),
                       DT_CENTER | DT_VCENTER | DT_NOCLIP | DT_SINGLELINE, false, screenW, screenH, true);
        }
        else
        {
            m_pDraw->DrawRectCutoutText(back.left, back.top, back.right - back.left, back.bottom - back.top,
                                        Layout::TabIdleBg, back.left, back.top, back.right, back.bottom,
                                        LanguageManager::Get("UI_BACK"), DT_CENTER | DT_VCENTER | DT_NOCLIP | DT_SINGLELINE);
        }

        const ButtonRect reset = GetRemapFooterRect(1, screenW, screenH);
        const bool hotReset = !IsRebindWaiting() && reset.Contains(cursorX, cursorY);
        const DWORD rbg = hotReset ? Layout::TabHoverBg : Layout::TabIdleBg;
        m_pDraw->DrawRectCutoutText(reset.left, reset.top, reset.right - reset.left, reset.bottom - reset.top,
                                    rbg, reset.left, reset.top, reset.right, reset.bottom,
                                    LanguageManager::Get("UI_RESET"), DT_CENTER | DT_VCENTER | DT_NOCLIP | DT_SINGLELINE);
    }

    if (IsRebindWaiting())
    {
        const float px = Layout::PanelX * sx;
        const float pw = Layout::PanelW * sx;
        const float listY = Layout::ListTopY * sy;
        const float ph = Layout::PanelH * sy;
        const float bandH = Layout::RemapWaitBandH * sy;
        const float bandY = listY + (ph - bandH) * 0.5f;

        m_pDraw->DrawRect(px, bandY, pw, bandH, Layout::RemapWaitBandBg);

        const float midY = bandY + bandH * 0.5f;
        const float lineH = 36.0f * sy;
        DrawUiText(px, midY - lineH, px + pw, midY, LanguageManager::Get("UI_REBIND_WAIT"),
                   DT_CENTER | DT_VCENTER | DT_NOCLIP | DT_SINGLELINE, false, screenW, screenH);
        DrawUiText(px, midY, px + pw, midY + lineH, LanguageManager::Get("UI_REBIND_ESC"),
                   DT_CENTER | DT_VCENTER | DT_NOCLIP | DT_SINGLELINE, false, screenW, screenH);
    }
}
