/*****************************************************************************
 *
 *  PROJECT:     The-Definitive-UI
 *  FILE:        source/MainMenu/MainMenu.cpp
 *  PURPOSE:     Custom main menu (background, hover, Game/Settings panels)
 *
 *****************************************************************************/

#include "MainMenu.h"
#include "GameSettings.h"
#include "WindowMode.h"
#include "Config.h"

#include "plugin.h"
#include "RenderWare.h"
#include "Draw/Draw.h"
#include "TxdManager.h"
#include "HookManager.h"
#include "InputManager.h"
#include "C_PcSave.h"
#include "CGenericGameStorage.h"
#include "CText.h"
#include "CMenuManager.h"
#include "LanguageManager.h"
#include "CCamera.h"
#include "CVehicle.h"
#include "CAudioEngine.h"
#include "eAudioEvents.h"
#include "CControllerConfigManager.h"
#include "CRenderer.h"
#include "CPostEffects.h"
#include "Fx_c.h"

#include <windows.h>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>

#ifndef GET_WHEEL_DELTA_WPARAM
#define GET_WHEEL_DELTA_WPARAM(wParam) ((short)HIWORD(wParam))
#endif

namespace
{
    const char* ButtonKeys[MainMenu::Layout::Count] = {
        "UI_START", "UI_GAME", "UI_SETTINGS", "UI_EXIT"
    };

    const char* GameTabKeys[MainMenu::Layout::GameTabCountMax] = {
        "UI_SAVE", "UI_LOAD", "UI_NEW_GAME", "UI_DELETE"
    };

    const char* SettingsTabKeys[MainMenu::Layout::SettingsTabCount] = {
        "UI_TAB_CONTROLS", "UI_TAB_GAME", "UI_TAB_GRAPHICS", "UI_TAB_SOUND", "UI_TAB_OPTIONS"
    };

    void FormatPrompt(char* out, size_t cap, const char* key, const char* arg)
    {
        if (!out || cap == 0)
            return;
        out[0] = 0;
        const char* fmt = LanguageManager::Get(key);
        if (!fmt)
            return;
        _snprintf_s(out, cap, _TRUNCATE, fmt, arg ? arg : "");
    }

    const char* RadarModeKeys[] = {
        "RADAR_MAP_BLIPS", "RADAR_BLIPS", "RADAR_OFF"
    };

    const char* FxQualityKeys[] = {
        "FX_LOW", "FX_MED", "FX_HIGH", "FX_VERY"
    };

    const char* AaNames[] = {
        "Off", "1x", "2x", "3x"
    };

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

MainMenu::MainMenu() = default;

MainMenu::~MainMenu()
{
    Shutdown();
}

bool MainMenu::Initialize(LPDIRECT3DDEVICE9 pDevice, Draw* pDraw, TxdManager* pTxd, HookManager* pHooks,
                           GameSettings* pSettings)
{
    if (m_bInitialized)
        return true;

    if (!pDevice || !pDraw || !pTxd || !pHooks || !pSettings)
        return false;

    m_pDevice = pDevice;
    m_pDraw = pDraw;
    m_pTxd = pTxd;
    m_pHooks = pHooks;
    m_pSettings = pSettings;
    m_pBackground = nullptr;

    m_pSettings->Bind(pDraw, pDevice, &m_bPendingVideoModeApply);
    LoadBackground();
    m_bInitialized = true;
    return true;
}

bool MainMenu::IsRebindWaiting() const
{
    return m_pSettings && m_pSettings->IsRebindWaiting();
}

void MainMenu::Shutdown()
{
    if (m_pSettings)
        m_pSettings->Close();
    CancelLoadConfirm();

    m_pBackground = nullptr;
    m_pLogo = nullptr;
    m_pRochelle = nullptr;
    for (int i = 0; i < Layout::Count; ++i)
        m_pHover[i] = nullptr;
    m_pGameBtnIdle[0] = m_pGameBtnIdle[1] = nullptr;
    m_pGameBtnHover[0] = m_pGameBtnHover[1] = nullptr;

    m_nHovered = -1;
    m_nHoverSoundId = -1;
    m_nHoverSlot = -1;
    m_pendingLoadSlot = -1;
    m_panel = Panel::None;
    m_gameTab = GameTab::Load;
    m_bEmbeddedPanels = false;
    m_bAltF4WasDown = false;
    m_bLmbWasDown = false;
    m_bWasFocused = true;
    m_bSwallowClick = false;

    if (m_pTxd && m_pTxd->IsTxdLoaded())
        m_pTxd->UnloadTxd();

    m_pDevice = nullptr;
    m_pDraw = nullptr;
    m_pTxd = nullptr;
    m_pHooks = nullptr;
    m_bInitialized = false;
}

void MainMenu::OnDeviceLost()
{
    // Keep m_panel / tabs / remap / rebind — only GPU handles go stale
    m_pBackground = nullptr;
    m_pLogo = nullptr;
    m_pRochelle = nullptr;
    for (int i = 0; i < Layout::Count; ++i)
        m_pHover[i] = nullptr;
    m_pGameBtnIdle[0] = m_pGameBtnIdle[1] = nullptr;
    m_pGameBtnHover[0] = m_pGameBtnHover[1] = nullptr;
    if (m_pSettings)
        m_pSettings->OnDeviceLost();
    m_bSwallowClick = false;
    m_bWasFocused = false;

    m_pDevice = nullptr;
    m_pDraw = nullptr;
    m_pTxd = nullptr;
    // hooks survive device reset; pending video mode still flushed after EndUi
    m_bInitialized = false;
}

bool MainMenu::LoadBackground()
{
    if (m_pBackground)
    {
        if (m_pTxd && m_pTxd->IsTxdLoaded())
        {
            if (!m_pLogo)
                m_pLogo = m_pTxd->GetTexture(Tex::Logo);
            if (!m_pRochelle)
                m_pRochelle = m_pTxd->GetTexture(Tex::Rochelle);
        }
        return true;
    }

    if (!m_pTxd || !m_pTxd->IsInitialized())
        return false;

    if (!m_pTxd->IsTxdLoaded() && !m_pTxd->LoadTxd(PLUGIN_PATH(Path::MainMenuTxd)))
        return false;

    m_pBackground = m_pTxd->GetTexture(Tex::Background);
    if (!m_pLogo)
        m_pLogo = m_pTxd->GetTexture(Tex::Logo);
    if (!m_pRochelle)
        m_pRochelle = m_pTxd->GetTexture(Tex::Rochelle);
    LoadHoverTextures();
    return m_pBackground != nullptr;
}

bool MainMenu::LoadHoverTextures()
{
    if (!m_pTxd || !m_pTxd->IsTxdLoaded())
        return false;

    static const char* names[Layout::Count] = {
        Tex::Start, Tex::Game, Tex::Settings, Tex::Exit
    };

    bool ok = true;
    for (int i = 0; i < Layout::Count; ++i)
    {
        if (!m_pHover[i])
            m_pHover[i] = m_pTxd->GetTexture(names[i]);
        if (!m_pHover[i])
            ok = false;
    }
    LoadGameConfirmTextures();
    return ok;
}

bool MainMenu::LoadGameConfirmTextures()
{
    if (!m_pTxd || !m_pTxd->IsTxdLoaded())
        return false;

    if (!m_pGameBtnIdle[0])
        m_pGameBtnIdle[0] = m_pTxd->GetTexture(Tex::GameNoHoverCancel);
    if (!m_pGameBtnHover[0])
        m_pGameBtnHover[0] = m_pTxd->GetTexture(Tex::GameHoverCancel);
    if (!m_pGameBtnIdle[1])
        m_pGameBtnIdle[1] = m_pTxd->GetTexture(Tex::GameNoHoverAccept);
    if (!m_pGameBtnHover[1])
        m_pGameBtnHover[1] = m_pTxd->GetTexture(Tex::GameHoverAccept);
    return m_pGameBtnIdle[0] && m_pGameBtnHover[0] && m_pGameBtnIdle[1] && m_pGameBtnHover[1];
}



const char* MainMenu::GetStartButtonLabel() const
{
    return m_saves.HasAny() ? LanguageManager::Get("UI_CONTINUE") : LanguageManager::Get("UI_START");
}

bool MainMenu::IsSaveTabAvailable() const
{
    // Save pickup / script-only save menu — never on title screen
    return m_bEmbeddedPanels
        && (FrontEndMenuManager.m_bSaveMenuActive || FrontEndMenuManager.m_bOnlySaveMenu);
}

int MainMenu::GetGameTabCount() const
{
    return IsSaveTabAvailable() ? Layout::GameTabCountMax : Layout::GameTabCount;
}

float MainMenu::GetGameTabWidth() const
{
    const int n = GetGameTabCount();
    if (n <= 0)
        return Layout::PanelW;
    return (Layout::PanelW - Layout::TabGapX * static_cast<float>(n - 1)) / static_cast<float>(n);
}

MainMenu::GameTab MainMenu::GameTabFromVisual(int visual) const
{
    if (IsSaveTabAvailable())
        return static_cast<GameTab>(visual);
    // visual 0..2 → Load / NewGame / Delete
    return static_cast<GameTab>(visual + 1);
}

int MainMenu::GameTabToVisual(GameTab tab) const
{
    if (IsSaveTabAvailable())
        return static_cast<int>(tab);
    return static_cast<int>(tab) - 1;
}

bool MainMenu::TrySaveSlot(int slot)
{
    if (slot < 0 || slot >= Layout::SlotCount)
        return false;

    FrontEndMenuManager.m_nSelectedSaveGame = static_cast<char>(slot);
    // gta-reversed: SaveSlot returns non-zero on failure
    if (PcSaveHelper.SaveSlot(slot))
    {
        PlayFe(AE_FRONTEND_ERROR);
        m_saves.Refresh();
        return false;
    }

    PlayFe(AE_FRONTEND_SELECT);
    m_saves.Refresh();
    CancelLoadConfirm();
    m_bSaveSuccessOpen = true;
    return true;
}

void MainMenu::DismissSaveSuccess()
{
    m_bSaveSuccessOpen = false;
    FrontEndMenuManager.m_bSaveMenuActive = false;
    FrontEndMenuManager.m_bOnlySaveMenu = false;
    ClosePanel();
    if (m_pHooks)
        m_pHooks->RequestResumeGame();
}

void MainMenu::OpenLoadConfirm(int slot)
{
    if (slot < 0 || slot >= Layout::SlotCount)
        return;
    if (m_saves.Get(slot).empty || m_saves.Get(slot).corrupted)
        return;
    m_pendingLoadSlot = slot;
    m_pendingIsDelete = false;
    m_pendingIsSave = false;
}

void MainMenu::OpenDeleteConfirm(int slot)
{
    if (slot < 0 || slot >= Layout::SlotCount)
        return;
    if (m_saves.Get(slot).empty)
        return;
    m_pendingLoadSlot = slot;
    m_pendingIsDelete = true;
    m_pendingIsSave = false;
}

void MainMenu::OpenSaveOverwriteConfirm(int slot)
{
    if (slot < 0 || slot >= Layout::SlotCount)
        return;
    if (m_saves.Get(slot).empty)
        return;
    m_pendingLoadSlot = slot;
    m_pendingIsDelete = false;
    m_pendingIsSave = true;
}

void MainMenu::CancelLoadConfirm()
{
    m_pendingLoadSlot = -1;
    m_pendingIsDelete = false;
    m_pendingIsSave = false;
}

void MainMenu::ConfirmLoadPending()
{
    const int slot = m_pendingLoadSlot;
    const bool isDelete = m_pendingIsDelete;
    const bool isSave = m_pendingIsSave;
    m_pendingLoadSlot = -1;
    m_pendingIsDelete = false;
    m_pendingIsSave = false;
    if (slot < 0)
        return;

    if (isSave)
    {
        TrySaveSlot(slot);
        return;
    }

    if (isDelete)
    {
        PcSaveHelper.DeleteSlot(slot);
        m_saves.Refresh();
        return;
    }

    if (!m_pHooks)
        return;

    if (!CGenericGameStorage::CheckSlotDataValid(slot, false))
    {
        PlayFe(AE_FRONTEND_ERROR);
        m_saves.Refresh();
        return;
    }

    m_pHooks->RequestLoadGame(slot);
    ClosePanel();
}

void MainMenu::OnStartOrContinue()
{
    m_saves.Refresh();
    if (!m_saves.HasAny())
    {
        m_panel = Panel::Game;
        m_gameTab = GameTab::NewGame;
        CancelLoadConfirm();
        if (m_pSettings)
            m_pSettings->CancelDrag();
        return;
    }

    const int slot = m_saves.FindMostRecent();
    if (slot < 0 || !m_pHooks)
        return;
    CancelLoadConfirm();
    m_pHooks->RequestLoadGame(slot);
    ClosePanel();
}

void MainMenu::ShowOsCursor()
{
    Ui::ShowOsCursor();
}

void MainMenu::HideOsCursor()
{
    Ui::HideOsCursor();
}

void MainMenu::HandleAltF4()
{
    const bool down = (GetAsyncKeyState(VK_MENU) & 0x8000) && (GetAsyncKeyState(VK_F4) & 0x8000);

    if (down && !m_bAltF4WasDown)
        RsGlobal.quit = TRUE;

    m_bAltF4WasDown = down;
}

void MainMenu::GetScreenSize(float& outW, float& outH) const
{
    Ui::GetScreenSizeViewport(m_pDevice, outW, outH);
}

bool MainMenu::GetCursorPosClient(float screenW, float screenH, float& outX, float& outY) const
{
    return Ui::GetCursorPosClient(screenW, screenH, outX, outY);
}

bool MainMenu::IsGameWindowFocused() const
{
    return Ui::IsGameWindowFocused();
}

int MainMenu::HitTestButton(float cursorX, float cursorY, float screenW, float screenH) const
{
    // Prefer closest center — avoids edge “stealing” when plates almost touch
    int best = -1;
    float bestDist2 = 1.0e30f;
    for (int i = 0; i < Layout::Count; ++i)
    {
        const ButtonRect box = GetButtonRect(i, screenW, screenH);
        if (!box.Contains(cursorX, cursorY))
            continue;
        const float cx = 0.5f * (box.left + box.right);
        const float cy = 0.5f * (box.top + box.bottom);
        const float dx = cursorX - cx;
        const float dy = cursorY - cy;
        const float d2 = dx * dx + dy * dy;
        if (d2 < bestDist2)
        {
            bestDist2 = d2;
            best = i;
        }
    }
    return best;
}

int MainMenu::HitTestHoverSoundId(float cursorX, float cursorY, float screenW, float screenH) const
{
    if (IsRebindWaiting())
        return -1;

    if (IsGameModal())
    {
        if (IsSaveSuccessOpen())
        {
            if (GetOkBtnRect(screenW, screenH).Contains(cursorX, cursorY))
                return 602;
            return -1;
        }
        if (GetNewGameBtnRect(0, screenW, screenH).Contains(cursorX, cursorY))
            return 600;
        if (GetNewGameBtnRect(1, screenW, screenH).Contains(cursorX, cursorY))
            return 601;
        return -1;
    }

    const int btn = HitTestButton(cursorX, cursorY, screenW, screenH);
    if (btn >= 0)
        return 100 + btn;

    if (m_panel == Panel::Game)
    {
        const int tabCount = GetGameTabCount();
        const float tabW = GetGameTabWidth();
        for (int t = 0; t < tabCount; ++t)
        {
            if (GetTabRect(t, tabW, screenW, screenH).Contains(cursorX, cursorY))
                return 200 + t;
        }
        if (m_gameTab == GameTab::Save || m_gameTab == GameTab::Load || m_gameTab == GameTab::Delete)
        {
            for (int s = 0; s < Layout::SlotCount; ++s)
            {
                if (GetSlotRect(s, screenW, screenH).Contains(cursorX, cursorY))
                    return 300 + s;
            }
        }
    }
    else if (m_panel == Panel::Settings && m_pSettings)
        return m_pSettings->HitTestHoverSoundId(cursorX, cursorY, screenW, screenH);

    return -1;
}

void MainMenu::UpdateHoverSound(float screenW, float screenH)
{
    if (m_pSettings && m_pSettings->IsDragging())
        return;

    float cx = 0.0f, cy = 0.0f;
    int id = -1;
    if (GetCursorPosClient(screenW, screenH, cx, cy))
        id = HitTestHoverSoundId(cx, cy, screenW, screenH);

    if (id >= 0 && id != m_nHoverSoundId)
        PlayFe(AE_FRONTEND_HIGHLIGHT);

    m_nHoverSoundId = id;
}

MainMenu::ButtonRect MainMenu::GetButtonRect(int index, float screenW, float screenH) const
{
    const float sx = screenW / Layout::RefW;
    const float sy = screenH / Layout::RefH;

    const float cx = Layout::CenterX * sx;
    const float cy = Layout::CenterYFromTop(index) * sy;
    const float halfW = Layout::HoverWidth * sx * 0.5f;
    // Hit/visual height leave a gap vs RowStepY so neighbors never fight on the edge
    const float hitH = (Layout::HoverHeight > Layout::RowStepY - 8.0f)
        ? (Layout::RowStepY - 8.0f)
        : Layout::HoverHeight;
    const float halfH = hitH * sy * 0.5f;

    return { cx - halfW, cy - halfH, cx + halfW, cy + halfH };
}

MainMenu::ButtonRect MainMenu::GetTabRect(int tab, float tabW, float screenW, float screenH) const
{
    const float sx = screenW / Layout::RefW;
    const float sy = screenH / Layout::RefH;
    const float x = (Layout::PanelX + (tabW + Layout::TabGapX) * static_cast<float>(tab)) * sx;
    const float y = Layout::TabTopY * sy;
    return { x, y, x + tabW * sx, y + Layout::TabH * sy };
}

MainMenu::ButtonRect MainMenu::GetSlotRect(int slot, float screenW, float screenH) const
{
    const float sx = screenW / Layout::RefW;
    const float sy = screenH / Layout::RefH;
    const float x = Layout::PanelX * sx;
    const float y = (Layout::ListTopY + Layout::RowH() * static_cast<float>(slot)) * sy;
    const float w = Layout::PanelW * sx;
    const float h = Layout::RowH() * sy;
    return { x, y, x + w, y + h };
}

MainMenu::ButtonRect MainMenu::GetPanelRect(float screenW, float screenH) const
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

MainMenu::ButtonRect MainMenu::GetNewGameBtnRect(int which, float screenW, float screenH) const
{
    const float sx = screenW / Layout::RefW;
    const float sy = screenH / Layout::RefH;
    const float btnW = Layout::NewGameBtnW * sx;
    const float btnH = Layout::NewGameBtnH * sy;
    const float gap = Layout::NewGameBtnGap * sx;
    const float pairW = btnW * 2.0f + gap;
    const float listCx = (Layout::PanelX + Layout::PanelW * 0.5f) * sx;
    const float listCy = (Layout::ListTopY + Layout::PanelH * 0.5f) * sy;
    const float top = listCy + btnH - btnH * 0.5f;
    const float left0 = listCx - pairW * 0.5f;
    const float left = left0 + static_cast<float>(which) * (btnW + gap);
    return { left, top, left + btnW, top + btnH };
}

MainMenu::ButtonRect MainMenu::GetOkBtnRect(float screenW, float screenH) const
{
    const float sx = screenW / Layout::RefW;
    const float sy = screenH / Layout::RefH;
    const float btnW = Layout::NewGameBtnW * sx;
    const float btnH = Layout::NewGameBtnH * sy;
    const float listCx = (Layout::PanelX + Layout::PanelW * 0.5f) * sx;
    const float listCy = (Layout::ListTopY + Layout::PanelH * 0.5f) * sy;
    const float left = listCx - btnW * 0.5f;
    const float top = listCy + btnH - btnH * 0.5f;
    return { left, top, left + btnW, top + btnH };
}


void MainMenu::OpenGamePanel()
{
    m_panel = Panel::Game;
    // Title-screen New Game confirm must not survive into pause
    if (m_gameTab == GameTab::NewGame || m_pendingLoadSlot >= 0 || m_bSaveSuccessOpen)
        m_gameTab = GameTab::Load;
    if (!IsSaveTabAvailable() && m_gameTab == GameTab::Save)
        m_gameTab = GameTab::Load;
    if (m_pSettings)
        m_pSettings->CancelDrag();
    CancelLoadConfirm();
    m_saves.Refresh();
}

void MainMenu::OpenGamePanelForSave()
{
    m_panel = Panel::Game;
    m_gameTab = GameTab::Save;
    if (m_pSettings)
        m_pSettings->CancelDrag();
    CancelLoadConfirm();
    m_saves.Refresh();
}

void MainMenu::OpenSettingsPanel()
{
    if (m_panel != Panel::Settings)
    {
        m_panel = Panel::Settings;
        if (m_pSettings)
            m_pSettings->Open();
    }
}

void MainMenu::ClosePanel()
{
    m_panel = Panel::None;
    m_gameTab = GameTab::Load;
    if (m_pSettings)
        m_pSettings->Close();
    CancelLoadConfirm();
    m_bSaveSuccessOpen = false;
}

void MainMenu::RequestExitGame()
{
    RsGlobal.quit = TRUE;
}

void MainMenu::SwallowNextClick()
{
    m_bSwallowClick = true;
}

void MainMenu::QueueVideoModeApply()
{
    m_bPendingVideoModeApply = true;
}

void MainMenu::FlushPendingVideoMode()
{
    if (!m_bPendingVideoModeApply)
        return;
    m_bPendingVideoModeApply = false;

    const int aa = FrontEndMenuManager.m_nPrefsAntiAliasing;
    const auto currentAa = *reinterpret_cast<RwUInt32*>(0x8E2430);
    if (aa >= 1 && static_cast<RwUInt32>(aa) != currentAa)
        RwD3D9ChangeMultiSamplingLevels(static_cast<RwUInt32>(aa));

    FrontEndMenuManager.m_nDisplayVideoMode = FrontEndMenuManager.m_nPrefsVideoMode;
    WindowMode::Flush();
    if (m_pDraw && m_pDraw->IsInitialized())
        m_pDraw->NotifyBackbufferChanged();
    if (WindowMode::Query() == 1)
        FrontEndMenuManager.CentreMousePointer();
    CPostEffects::DoScreenModeDependentInitializations();
}

void MainMenu::RenderEmbeddedPanels()
{
    if (!m_bInitialized || !m_pDraw)
        return;

    m_bEmbeddedPanels = true;

    // Ensure atlas for panel chrome (shared with pMainMenu TXD)
    LoadBackground();

    if (m_pSettings)
        m_pSettings->Bind(m_pDraw, m_pDevice, &m_bPendingVideoModeApply);

    float w = 0.0f;
    float h = 0.0f;
    GetScreenSize(w, h);

    UpdateHoverSound(w, h);
    HandleClicks(w, h);
    // Device may have been lost mid-click if something else reset D3D
    if (!m_bInitialized || !m_pDraw)
    {
        m_bEmbeddedPanels = false;
        return;
    }
    DrawGamePanel(w, h);
    DrawSettingsPanel(w, h);

    m_bEmbeddedPanels = false;
}

void MainMenu::OnButtonActivated(int index)
{
    PlayFe(AE_FRONTEND_SELECT);

    switch (static_cast<Button>(index))
    {
    case Button::Start:
        OnStartOrContinue();
        break;

    case Button::Game:
        OpenGamePanel();
        break;

    case Button::Settings:
        OpenSettingsPanel();
        break;

    case Button::Exit:
        RequestExitGame();
        break;
    }
}

void MainMenu::HandleSliderDrag(float screenW, float screenH)
{
    if (m_pSettings)
        m_pSettings->HandleSliderDrag(screenW, screenH);
}


void MainMenu::HandleClicks(float screenW, float screenH)
{
    HandleSliderDrag(screenW, screenH);

    const bool focused = IsGameWindowFocused();
    if (!focused)
    {
        m_bWasFocused = false;
        m_bLmbWasDown = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
        return;
    }
    if (!m_bWasFocused)
    {
        m_bWasFocused = true;
        // Swallow only the click that restored focus (button still held), not the next click
        const bool lmbHeld = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
        m_bSwallowClick = lmbHeld;
        m_bLmbWasDown = lmbHeld;
        if (lmbHeld)
            return;
        // Focus returned without a held click — process clicks normally this frame
    }

    const bool lmb = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
    const bool clicked = lmb && !m_bLmbWasDown;
    m_bLmbWasDown = lmb;

    if (!clicked)
        return;

    if (m_bSwallowClick)
    {
        m_bSwallowClick = false;
        return;
    }

    float cx = 0.0f, cy = 0.0f;
    if (!GetCursorPosClient(screenW, screenH, cx, cy))
        return;

    // Modal: rebind wait — block menu clicks
    if (IsRebindWaiting())
    {
        m_bLmbWasDown = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
        return;
    }

    // Modal: New Game / Load-Delete-Save confirm / Save success
    if (IsSaveSuccessOpen())
    {
        if (GetOkBtnRect(screenW, screenH).Contains(cx, cy))
        {
            PlayFe(AE_FRONTEND_SELECT);
            DismissSaveSuccess();
        }
        return;
    }
    if (IsNewGameConfirmOpen())
    {
        if (GetNewGameBtnRect(0, screenW, screenH).Contains(cx, cy))
        {
            PlayFe(AE_FRONTEND_BACK);
            m_gameTab = GameTab::Load;
            m_saves.Refresh();
        }
        else if (GetNewGameBtnRect(1, screenW, screenH).Contains(cx, cy))
        {
            PlayFe(AE_FRONTEND_SELECT);
            if (m_pHooks)
                m_pHooks->RequestNewGame();
            ClosePanel();
        }
        return;
    }
    if (IsLoadConfirmOpen())
    {
        if (GetNewGameBtnRect(0, screenW, screenH).Contains(cx, cy))
        {
            PlayFe(AE_FRONTEND_BACK);
            CancelLoadConfirm();
        }
        else if (GetNewGameBtnRect(1, screenW, screenH).Contains(cx, cy))
        {
            PlayFe(AE_FRONTEND_SELECT);
            ConfirmLoadPending();
        }
        return;
    }

    if (!m_bEmbeddedPanels)
    {
        const int btn = HitTestButton(cx, cy, screenW, screenH);
        if (btn >= 0)
        {
            OnButtonActivated(btn);
            return;
        }
    }

    if (m_panel == Panel::Game)
    {
        const int tabCount = GetGameTabCount();
        const float tabW = GetGameTabWidth();
        for (int t = 0; t < tabCount; ++t)
        {
            if (GetTabRect(t, tabW, screenW, screenH).Contains(cx, cy))
            {
                PlayFe(AE_FRONTEND_SELECT);
                m_gameTab = GameTabFromVisual(t);
                CancelLoadConfirm();
                if (m_gameTab == GameTab::Save || m_gameTab == GameTab::Load || m_gameTab == GameTab::Delete)
                    m_saves.Refresh();
                return;
            }
        }

        if (m_gameTab == GameTab::Save || m_gameTab == GameTab::Load || m_gameTab == GameTab::Delete)
        {
            for (int s = 0; s < Layout::SlotCount; ++s)
            {
                if (!GetSlotRect(s, screenW, screenH).Contains(cx, cy))
                    continue;

                m_nHoverSlot = s;
                if (m_gameTab == GameTab::Save)
                {
                    PlayFe(AE_FRONTEND_SELECT);
                    if (m_saves.Get(s).empty)
                        TrySaveSlot(s);
                    else
                        OpenSaveOverwriteConfirm(s);
                }
                else if (m_gameTab == GameTab::Load && !m_saves.Get(s).empty && !m_saves.Get(s).corrupted)
                {
                    PlayFe(AE_FRONTEND_SELECT);
                    OpenLoadConfirm(s);
                }
                else if (m_gameTab == GameTab::Delete && !m_saves.Get(s).empty)
                {
                    PlayFe(AE_FRONTEND_SELECT);
                    OpenDeleteConfirm(s);
                }
                else
                    PlayFe(AE_FRONTEND_ERROR);
                return;
            }
        }

        if (!GetPanelRect(screenW, screenH).Contains(cx, cy))
        {
            PlayFe(AE_FRONTEND_BACK);
            CancelLoadConfirm();
            m_panel = Panel::None;
        }
    }
    else if (m_panel == Panel::Settings && m_pSettings)
    {
        bool closePanel = false;
        m_pSettings->HandleClick(cx, cy, screenW, screenH, closePanel);
        if (closePanel)
            m_panel = Panel::None;
    }
}

void MainMenu::DrawUiText(float left, float top, float right, float bottom, const char* text,
                          DWORD format, bool hovered, float screenW, float screenH,
                          bool onActivePlate, bool solidIdle)
{
    Ui::DrawMenuText(m_pDraw, left, top, right, bottom, text, format, hovered,
                     screenW, screenH, onActivePlate, solidIdle);
}

void MainMenu::DrawLogo(float screenW, float screenH)
{
    if (!m_pDraw || !m_pLogo)
        return;

    const float sx = screenW / Layout::RefW;
    const float sy = screenH / Layout::RefH;
    m_pDraw->DrawTexture(m_pLogo, Layout::LogoPadX * sx, Layout::LogoPadY * sy,
                         Layout::LogoW * sx, Layout::LogoH * sy);
}

void MainMenu::DrawRochelle(float screenW, float screenH)
{
    if (!m_pDraw || !m_pRochelle || m_panel != Panel::None)
        return;

    const float sx = screenW / Layout::RefW;
    const float sy = screenH / Layout::RefH;
    const float w = Layout::RochelleW * sx;
    const float h = Layout::RochelleH * sy;
    const float x = screenW - (Layout::RochellePadRight * sx) - w;
    m_pDraw->DrawTexture(m_pRochelle, x, 0.0f, w, h);
}

void MainMenu::DrawButtons(float screenW, float screenH)
{
    if (!m_pDraw)
        return;

    float cursorX = 0.0f;
    float cursorY = 0.0f;
    m_nHovered = -1;
    m_saves.Ensure();
    const bool modal = IsGameModal();
    if (!modal && GetCursorPosClient(screenW, screenH, cursorX, cursorY))
        m_nHovered = HitTestButton(cursorX, cursorY, screenW, screenH);

    int active = -1;
    if (m_panel == Panel::Game)
        active = static_cast<int>(Button::Game);
    else if (m_panel == Panel::Settings)
        active = static_cast<int>(Button::Settings);

    const float sx = screenW / Layout::RefW;
    const float sy = screenH / Layout::RefH;
    const int fontH = static_cast<int>(Layout::FontSize * sy + 0.5f);
    m_pDraw->EnsureFontHeight(fontH > 0 ? fontH : 1);

    const float halfW = Layout::HoverWidth * sx * 0.5f;
    const float halfH = Layout::TextHeight * sy * 0.5f;

    for (int i = 0; i < Layout::Count; ++i)
    {
        const ButtonRect box = GetButtonRect(i, screenW, screenH);
        // Sticky active when cursor not on another left-menu button;
        // while hovering another button — only that one (not both)
        int showIdx = active;
        if (modal)
            showIdx = active; // confirm modal: keep «Игра»
        else if (m_nHovered >= 0)
            showIdx = m_nHovered;
        const bool showHover = (showIdx >= 0 && i == showIdx);

        if (showHover && m_pHover[i])
            m_pDraw->DrawTexture(m_pHover[i], box.left, box.top,
                                 box.right - box.left, box.bottom - box.top);

        const float cx = Layout::CenterX * sx;
        const float cy = Layout::CenterYFromTop(i) * sy;
        // Sticky/hover plate → чёрный; idle left buttons → white+metal (solidIdle)
        const char* label = (i == static_cast<int>(Button::Start)) ? GetStartButtonLabel() : LanguageManager::Get(ButtonKeys[i]);
        DrawUiText(cx - halfW, cy - halfH, cx + halfW, cy + halfH, label,
                   DT_CENTER | DT_VCENTER | DT_NOCLIP, false, screenW, screenH, showHover, true);
    }
}

void MainMenu::DrawGamePanel(float screenW, float screenH)
{
    if (!m_pDraw || m_panel != Panel::Game)
        return;

    m_saves.Ensure();

    const float sx = screenW / Layout::RefW;
    const float sy = screenH / Layout::RefH;

    const float px = Layout::PanelX * sx;
    const float listY = Layout::ListTopY * sy;
    const float pw = Layout::PanelW * sx;
    const float ph = Layout::PanelH * sy;
    const float tabPadX = Layout::TabPadX * sx;
    const float slotPadX = Layout::PanelPadX * sx;

    float cursorX = 0.0f, cursorY = 0.0f;
    GetCursorPosClient(screenW, screenH, cursorX, cursorY);
    const bool modal = IsGameModal();

    const int fontH = static_cast<int>(Layout::PanelFont * sy + 0.5f);
    m_pDraw->EnsureFontHeight(fontH > 0 ? fontH : 1);

    const int tabCount = GetGameTabCount();
    const float tabW = GetGameTabWidth();
    for (int t = 0; t < tabCount; ++t)
    {
        const ButtonRect tab = GetTabRect(t, tabW, screenW, screenH);
        const GameTab tabId = GameTabFromVisual(t);
        const bool active = (m_gameTab == tabId);
        const bool hot = !modal && tab.Contains(cursorX, cursorY);
        const float tw = tab.right - tab.left;
        const float th = tab.bottom - tab.top;
        const char* label = LanguageManager::Get(GameTabKeys[static_cast<int>(tabId)]);

        if (active)
        {
            m_pDraw->DrawRect(tab.left, tab.top, tw, th, Layout::TabActiveBg);
            DrawUiText(tab.left + tabPadX, tab.top, tab.right - tabPadX, tab.bottom,
                       label, DT_CENTER | DT_VCENTER | DT_NOCLIP | DT_SINGLELINE,
                       false, screenW, screenH, true);
        }
        else
        {
            // Hover: only rect alpha; cutout text stays transparent (unchanged)
            const DWORD bg = hot ? Layout::TabHoverBg : Layout::TabIdleBg;
            m_pDraw->DrawRectCutoutText(tab.left, tab.top, tw, th, bg,
                                        tab.left + tabPadX, tab.top, tab.right - tabPadX, tab.bottom,
                                        label, DT_CENTER | DT_VCENTER | DT_NOCLIP | DT_SINGLELINE);
        }
    }

    if (m_gameTab == GameTab::NewGame)
    {
        DrawNewGameConfirm(screenW, screenH, cursorX, cursorY);
        return;
    }

    if (IsSaveSuccessOpen())
    {
        DrawSaveSuccess(screenW, screenH, cursorX, cursorY);
        return;
    }

    if (IsLoadConfirmOpen())
    {
        DrawLoadConfirm(screenW, screenH, cursorX, cursorY);
        return;
    }

    m_nHoverSlot = -1;
    for (int s = 0; s < Layout::SlotCount; ++s)
    {
        const ButtonRect row = GetSlotRect(s, screenW, screenH);
        const bool hot = row.Contains(cursorX, cursorY);
        if (hot)
            m_nHoverSlot = s;

        DWORD rowBg = (s & 1) ? Layout::SlotZebraB : Layout::SlotZebraA;
        if (hot)
            rowBg = Layout::SlotHoverBg;

        std::string label = m_saves.Get(s).name;
        if (!m_saves.Get(s).empty && !m_saves.Get(s).date.empty())
            label += "  —  " + m_saves.Get(s).date;

        m_pDraw->DrawRectCutoutText(row.left, row.top, row.right - row.left, row.bottom - row.top, rowBg,
                                    row.left + slotPadX, row.top, row.right - slotPadX, row.bottom,
                                    label.c_str(), DT_LEFT | DT_VCENTER | DT_NOCLIP | DT_SINGLELINE);
    }
}

void MainMenu::DrawConfirmButtons(float screenW, float screenH, float cursorX, float cursorY)
{
    const char* labels[2] = { LanguageManager::Get("UI_CANCEL"), LanguageManager::Get("UI_CONFIRM") };
    for (int i = 0; i < 2; ++i)
    {
        const ButtonRect btn = GetNewGameBtnRect(i, screenW, screenH);
        const bool hot = btn.Contains(cursorX, cursorY);
        Ui::DrawTexturedConfirmButton(m_pDraw, btn.left, btn.top, btn.right, btn.bottom,
                                      m_pGameBtnIdle[i], m_pGameBtnHover[i], hot, labels[i],
                                      screenW, screenH);
    }
}

void MainMenu::DrawOkButton(float screenW, float screenH, float cursorX, float cursorY)
{
    const ButtonRect btn = GetOkBtnRect(screenW, screenH);
    const bool hot = btn.Contains(cursorX, cursorY);
    const DWORD bg = hot ? Layout::NewGameBtnHover : Layout::NewGameBtnIdle;
    m_pDraw->DrawRectCutoutText(btn.left, btn.top, btn.right - btn.left, btn.bottom - btn.top, bg,
                                btn.left, btn.top, btn.right, btn.bottom,
                                LanguageManager::Get("UI_OK"), DT_CENTER | DT_VCENTER | DT_NOCLIP | DT_SINGLELINE);
}

void MainMenu::DrawConfirmSheet(float screenW, float screenH, const char* prompt, bool okOnly)
{
    const float sx = screenW / Layout::RefW;
    const float sy = screenH / Layout::RefH;
    const float px = Layout::PanelX * sx;
    const float listY = Layout::ListTopY * sy;
    const float pw = Layout::PanelW * sx;
    const float ph = Layout::PanelH * sy;
    const float padX = Layout::PanelPadX * sx;

    const float listCy = listY + ph * 0.5f;
    const float textBody = m_pDraw->GetFontHeight();
    const float textCy = listCy - textBody;
    const float wrapH = textBody * 8.0f;
    const float promptL = px + padX;
    const float promptR = px + pw - padX;
    const float promptT = textCy - wrapH * 0.5f;
    const float promptB = textCy + wrapH * 0.5f;

    const DWORD promptFmt = DT_CENTER | DT_VCENTER | DT_NOCLIP | DT_WORDBREAK;
    const DWORD btnFmt = DT_CENTER | DT_VCENTER | DT_NOCLIP | DT_SINGLELINE;

    Draw::CutoutSpan spans[3];
    int n = 0;
    spans[n++] = { promptL, promptT, promptR, promptB, prompt, promptFmt };

    if (okOnly)
    {
        const ButtonRect ok = GetOkBtnRect(screenW, screenH);
        spans[n++] = { ok.left, ok.top, ok.right, ok.bottom, LanguageManager::Get("UI_OK"), btnFmt };
    }
    else
    {
        const char* labels[2] = { LanguageManager::Get("UI_CANCEL"), LanguageManager::Get("UI_CONFIRM") };
        for (int i = 0; i < 2; ++i)
        {
            const ButtonRect btn = GetNewGameBtnRect(i, screenW, screenH);
            spans[n++] = { btn.left, btn.top, btn.right, btn.bottom, labels[i], btnFmt };
        }
    }

    m_pDraw->DrawRectCutoutTexts(px, listY, pw, ph, Layout::SlotZebraA, spans, n);
}

void MainMenu::DrawNewGameConfirm(float screenW, float screenH, float cursorX, float cursorY)
{
    if (!m_pDraw)
        return;

    DrawConfirmSheet(screenW, screenH, LanguageManager::Get("UI_NEW_GAME_PROMPT"), false);
    DrawConfirmButtons(screenW, screenH, cursorX, cursorY);
}

void MainMenu::DrawLoadConfirm(float screenW, float screenH, float cursorX, float cursorY)
{
    if (!m_pDraw || m_pendingLoadSlot < 0 || m_pendingLoadSlot >= Layout::SlotCount)
        return;

    std::string title = m_saves.Get(m_pendingLoadSlot).name;
    char msg[512];
    if (m_pendingIsSave)
        FormatPrompt(msg, sizeof(msg), "UI_OVERWRITE_PROMPT", title.c_str());
    else if (m_pendingIsDelete)
        FormatPrompt(msg, sizeof(msg), "UI_DELETE_PROMPT", title.c_str());
    else
        FormatPrompt(msg, sizeof(msg), "UI_LOAD_PROMPT", title.c_str());

    DrawConfirmSheet(screenW, screenH, msg, false);
    DrawConfirmButtons(screenW, screenH, cursorX, cursorY);
}

void MainMenu::DrawSaveSuccess(float screenW, float screenH, float cursorX, float cursorY)
{
    if (!m_pDraw)
        return;

    DrawConfirmSheet(screenW, screenH, LanguageManager::Get("UI_SAVE_SUCCESS"), true);
    DrawOkButton(screenW, screenH, cursorX, cursorY);
}

void MainMenu::DrawSettingsPanel(float screenW, float screenH)
{
    if (m_panel != Panel::Settings || !m_pSettings)
        return;
    m_pSettings->Render(screenW, screenH);
}


void MainMenu::Render()
{
    if (!m_bInitialized || !m_pDraw || !m_pHooks)
        return;

    if (!m_pHooks->IsCustomMainMenuSession() || m_pHooks->GetGameState() < 7)
        return;

    m_pHooks->TickSession();
    HandleAltF4();
    ShowOsCursor();

    if (!LoadBackground())
        return;

    if (m_pSettings)
        m_pSettings->Bind(m_pDraw, m_pDevice, &m_bPendingVideoModeApply);

    float w = 0.0f;
    float h = 0.0f;
    GetScreenSize(w, h);

    UpdateHoverSound(w, h);
    HandleClicks(w, h);

    // New game / load dismissed the custom session this frame — stop drawing
    if (!m_pHooks->IsCustomMainMenuSession())
    {
        HideOsCursor();
        return;
    }

    if (!m_bInitialized || !m_pDraw)
    {
        return;
    }

    m_pDraw->BeginUi();
    m_pDraw->DrawTexture(m_pBackground, 0.0f, 0.0f, w, h);
    DrawRochelle(w, h);
    DrawLogo(w, h);
    DrawButtons(w, h);
    DrawGamePanel(w, h);
    DrawSettingsPanel(w, h);
    m_pDraw->EndUi();
}
