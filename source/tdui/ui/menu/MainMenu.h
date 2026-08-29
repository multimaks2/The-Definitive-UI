/*****************************************************************************
 *
 *  PROJECT:     The-Definitive-UI
 *  FILE:        source/MainMenu/MainMenu.h
 *  PURPOSE:     Custom main menu
 *
 *****************************************************************************/

#pragma once

#include <d3d9.h>

#include "Utils.h"
#include "SaveSlots.h"

class Draw;
class TxdManager;
class HookManager;
class GameSettings;

class MainMenu
{
public:
    struct Path
    {
        static constexpr const char* MainMenuTxd = "The-Definitive-UI.SA\\MainMenu.txd";
    };

    struct Tex
    {
        static constexpr const char* Background = "MainMenu";
        static constexpr const char* Start      = "Start";
        static constexpr const char* Game       = "Game";
        static constexpr const char* Settings   = "Settings";
        static constexpr const char* Exit       = "Exit";
        static constexpr const char* GameNoHoverCancel  = "GameNoHoverCancel";
        static constexpr const char* GameHoverCancel    = "GameHoverCancel";
        static constexpr const char* GameNoHoverAccept  = "GameNoHoverAccept";
        static constexpr const char* GameHoverAccept    = "GameHoverAccept";
        static constexpr const char* Logo               = "Logo";
        static constexpr const char* Rochelle           = "ROCHELLE";
    };

    // 2K reference (2560x1440). Menu button Y is from the BOTTOM of the screen.
    struct Layout
    {
        static constexpr float RefW = 2560.0f;
        static constexpr float RefH = 1440.0f;

        static constexpr float CenterX      = 330.0f;
        static constexpr float FirstCenterY = 400.0f;
        static constexpr float RowStepY     = 85.0f;

        static constexpr float FontSize     = 50.0f;
        static constexpr float TextHeight   = 50.0f;
        static constexpr float HoverWidth   = 388.0f;
        static constexpr float HoverHeight  = 78.0f;

        static constexpr float LogoPadX     = 60.0f;
        static constexpr float LogoPadY     = 80.0f;
        static constexpr float LogoW        = 575.0f;
        static constexpr float LogoH        = 510.0f;

        static constexpr float RochelleW        = 1265.0f;
        static constexpr float RochelleH        = 1440.0f;
        static constexpr float RochellePadRight = 115.0f;

        static constexpr DWORD OutlineColor   = 0xFF588942;
        static constexpr float OutlineOffsetX = 3.0f;
        static constexpr float OutlineOffsetY = 3.0f;

        // Shared right panel @2K (Game + Settings)
        // Right margin 83 → panel left = 2560 - 83 - 1803 = 674
        static constexpr float PanelRightPad = 83.0f;
        static constexpr float PanelW        = 1803.0f;
        static constexpr float PanelX        = RefW - PanelRightPad - PanelW; // 674
        static constexpr float TabTopY       = 55.0f;
        static constexpr float TabH          = 80.0f;
        static constexpr float TabGapX       = 6.0f;
        static constexpr float TabToListGapY = 13.0f;
        static constexpr float ListTopY      = TabTopY + TabH + TabToListGapY; // 148
        static constexpr float PanelH        = 1000.0f;
        static constexpr float PanelPadX     = 36.0f;
        static constexpr float TabPadX       = 16.0f;
        static constexpr float PanelFont     = 28.0f;
        static constexpr int   SlotCount     = 8; // vanilla MAX_SAVEGAME_SLOTS; PC has no autosave

        // Game tabs: 3 (title/pause) or 4 when save pickup is active
        static constexpr int   GameTabCountMax = 4;
        static constexpr int   GameTabCount    = 3; // Load / New / Delete (no Save)

        // Settings: 5×355.8 + 4×6 = 1803
        static constexpr float SettingsTabW     = 355.8f;
        static constexpr int   SettingsTabCount = 5;
        static constexpr float SliderTrackW     = 420.0f;
        static constexpr float SliderTrackH     = 10.0f;
        static constexpr float SliderKnobR      = 14.0f;
        static constexpr float ValueAreaW       = 520.0f;
        static constexpr float CycleArrowW      = 28.0f;  // hit + glyph box (tight)
        static constexpr float CycleArrowGap    = 2.0f;   // space between < value >
        static constexpr float CycleArrowScale  = 1.28f;  // hover via larger font face

        // Game confirm @2K — origin = list center; text up 1× font body; pair down 1× pair H
        static constexpr float NewGameBtnW         = 400.0f;
        static constexpr float NewGameBtnH         = 72.0f;
        static constexpr float NewGameBtnGap       = 24.0f;
        static constexpr DWORD NewGameBtnIdle      = 0xC8000000;
        static constexpr DWORD NewGameBtnHover     = 0xE1000000;

        // Controls remap (Изменить раскладку)
        static constexpr float RemapColGap     = 40.0f;
        static constexpr float RemapHeaderH    = 48.0f;
        static constexpr float RemapFooterH    = 72.0f;
        static constexpr float RemapFooterGap  = 48.0f; // gap between grid list and footer buttons
        static constexpr float RemapRowH       = 56.0f;  // fixed row; list scrolls
        static constexpr float RemapFont       = 26.0f;
        static constexpr float RemapWheelImpulse = 220.0f; // per notch (AZ2 UiSmoothScroll)
        static constexpr float RemapWheelMult    = 3.5f;
        static constexpr float RemapVelDamp       = 7.0f;   // velocity decay
        static constexpr float RemapDisplaySmooth = 14.0f;  // display catch-up
        static constexpr float RemapWaitBandH  = 160.0f;
        static constexpr DWORD RemapWaitBandBg = 0xE0000000;

        static constexpr float RowH() { return PanelH / static_cast<float>(SlotCount); }

        static constexpr DWORD TabIdleBg   = 0xC8000000; // α200
        static constexpr DWORD TabHoverBg  = 0xE1000000; // α225
        static constexpr DWORD TabActiveBg = 0xFFFFFFFF;
        static constexpr DWORD TabActiveFg = 0xFF000000;
        static constexpr DWORD SlotZebraA  = 0xC8000000;
        static constexpr DWORD SlotZebraB  = 0xFF000000;
        static constexpr DWORD SlotHoverBg = 0xC84A4A4A;
        static constexpr DWORD SliderTrack = 0xFF2A2A2A;
        static constexpr DWORD SliderFill  = 0xFFB0B0B0;
        static constexpr DWORD SliderKnob  = 0xFFFFFFFF;

        static constexpr int Count = 4;

        static float CenterYFromTop(int index)
        {
            return RefH - (FirstCenterY - RowStepY * static_cast<float>(index));
        }
    };

    enum class Button : int
    {
        Start = 0,
        Game,
        Settings,
        Exit
    };

    enum class Panel : int
    {
        None = 0,
        Game,
        Settings
    };

    enum class GameTab : int
    {
        Save = 0, // only while save pickup / OnlySaveMenu
        Load,
        NewGame,
        Delete
    };

    MainMenu();
    ~MainMenu();

    bool Initialize(LPDIRECT3DDEVICE9 pDevice, Draw* pDraw, TxdManager* pTxd, HookManager* pHooks,
                    GameSettings* pSettings);
    void Shutdown();
    // Drop D3D refs only — keep open panel / tab / remap state across alt-tab
    void OnDeviceLost();
    void Render();

    // Used by pMainMenu (pause) — Game / Settings panels + Exit, no left-column chrome
    void OpenGamePanel();
    void OpenGamePanelForSave(); // pickup → Игра / Сохранить
    void OpenSettingsPanel();
    void ClosePanel();
    void RequestExitGame();
    void SwallowNextClick();
    Panel GetOpenPanel() const { return m_panel; }
    bool IsRebindWaiting() const;
    void RenderEmbeddedPanels();
    // Apply resolution/AA after EndUi — GameSetVideoMode mid-draw kills D3D
    void FlushPendingVideoMode();

    bool IsInitialized() const { return m_bInitialized; }

private:
    using ButtonRect = UiRect;

    bool IsNewGameConfirmOpen() const
    {
        return m_panel == Panel::Game && m_gameTab == GameTab::NewGame;
    }

    bool IsLoadConfirmOpen() const
    {
        return m_panel == Panel::Game && m_pendingLoadSlot >= 0;
    }

    bool IsSaveSuccessOpen() const
    {
        return m_panel == Panel::Game && m_bSaveSuccessOpen;
    }

    bool IsGameModal() const
    {
        return IsNewGameConfirmOpen() || IsLoadConfirmOpen() || IsSaveSuccessOpen();
    }

    bool LoadBackground();
    bool LoadHoverTextures();
    bool LoadGameConfirmTextures();
    void ShowOsCursor();
    void HideOsCursor();
    void HandleClicks(float screenW, float screenH);
    void HandleSliderDrag(float screenW, float screenH);
    int  HitTestHoverSoundId(float cursorX, float cursorY, float screenW, float screenH) const;
    void UpdateHoverSound(float screenW, float screenH);
    void GetScreenSize(float& outW, float& outH) const;
    bool GetCursorPosClient(float screenW, float screenH, float& outX, float& outY) const;
    int  HitTestButton(float cursorX, float cursorY, float screenW, float screenH) const;
    ButtonRect GetButtonRect(int index, float screenW, float screenH) const;
    ButtonRect GetTabRect(int tab, float tabW, float screenW, float screenH) const;
    ButtonRect GetSlotRect(int slot, float screenW, float screenH) const;
    ButtonRect GetPanelRect(float screenW, float screenH) const;
    ButtonRect GetNewGameBtnRect(int which, float screenW, float screenH) const; // 0=Cancel 1=Confirm
    ButtonRect GetOkBtnRect(float screenW, float screenH) const; // centered single OK

    bool IsSaveTabAvailable() const;
    int  GetGameTabCount() const;
    float GetGameTabWidth() const;
    GameTab GameTabFromVisual(int visual) const;
    int  GameTabToVisual(GameTab tab) const;
    const char* GetStartButtonLabel() const;
    void OpenLoadConfirm(int slot);
    void OpenDeleteConfirm(int slot);
    void OpenSaveOverwriteConfirm(int slot);
    void CancelLoadConfirm();
    void ConfirmLoadPending();
    bool TrySaveSlot(int slot); // true on success → success modal
    void DismissSaveSuccess();
    void OnStartOrContinue();
    void DrawButtons(float screenW, float screenH);
    void DrawLogo(float screenW, float screenH);
    void DrawRochelle(float screenW, float screenH);
    void DrawGamePanel(float screenW, float screenH);
    void DrawNewGameConfirm(float screenW, float screenH, float cursorX, float cursorY);
    void DrawLoadConfirm(float screenW, float screenH, float cursorX, float cursorY);
    void DrawSaveSuccess(float screenW, float screenH, float cursorX, float cursorY);
    void DrawConfirmButtons(float screenW, float screenH, float cursorX, float cursorY);
    void DrawOkButton(float screenW, float screenH, float cursorX, float cursorY);
    void DrawConfirmSheet(float screenW, float screenH, const char* prompt, bool okOnly);
    void DrawSettingsPanel(float screenW, float screenH);
    // onActivePlate = black on white plate; hovered = solid white;
    // solidIdle = white+outline (left 4 buttons on green bg);
    // else idle = outline-only / transparent fill (panels)
    void DrawUiText(float left, float top, float right, float bottom, const char* text,
                    DWORD format, bool hovered, float screenW, float screenH,
                    bool onActivePlate = false, bool solidIdle = false);
    void OnButtonActivated(int index);
    bool IsGameWindowFocused() const;

    LPDIRECT3DDEVICE9  m_pDevice = nullptr;
    Draw*              m_pDraw = nullptr;
    TxdManager*        m_pTxd = nullptr;
    HookManager*       m_pHooks = nullptr;
    GameSettings*      m_pSettings = nullptr;
    SaveSlots          m_saves;
    LPDIRECT3DTEXTURE9 m_pBackground = nullptr;
    LPDIRECT3DTEXTURE9 m_pLogo = nullptr;
    LPDIRECT3DTEXTURE9 m_pRochelle = nullptr;
    LPDIRECT3DTEXTURE9 m_pHover[Layout::Count] = {};
    LPDIRECT3DTEXTURE9 m_pGameBtnIdle[2] = {};  // Cancel, Accept
    LPDIRECT3DTEXTURE9 m_pGameBtnHover[2] = {};
    int                m_nHovered = -1;
    int                m_nHoverSoundId = -1;
    int                m_nHoverSlot = -1;
    int                m_pendingLoadSlot = -1; // >=0 → load/delete/save-overwrite confirm
    bool               m_pendingIsDelete = false;
    bool               m_pendingIsSave = false;
    bool               m_bSaveSuccessOpen = false;
    Panel              m_panel = Panel::None;
    GameTab            m_gameTab = GameTab::Load;
    bool               m_bInitialized = false;
    bool               m_bEmbeddedPanels = false; // true while hosted by pMainMenu
    bool               m_bLmbWasDown = false;
    bool               m_bWasFocused = true;
    bool               m_bSwallowClick = false;
    bool               m_bPendingVideoModeApply = false;

    void QueueVideoModeApply();
};
