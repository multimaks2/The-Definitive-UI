/*****************************************************************************
 *
 *  PROJECT:     The-Definitive-UI
 *  FILE:        source/GameSettings/GameSettings.h
 *  PURPOSE:     Settings + control remap (shared by MainMenu and pMainMenu)
 *
 *****************************************************************************/

#pragma once

#include <d3d9.h>
#include <functional>
#include <string>

#include "Utils.h"
#include "Config.h"

class Draw;
class Radar;

class GameSettings
{
public:
    // Shared right panel @2K — same geometry as MainMenu Game panel
    struct Layout
    {
        static constexpr float RefW = 2560.0f;
        static constexpr float RefH = 1440.0f;

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
        static constexpr int   SlotCount     = 11;

        static constexpr float SettingsTabW     = 355.8f;
        static constexpr int   SettingsTabCount = 5;
        static constexpr float SliderTrackW     = 420.0f;
        static constexpr float SliderTrackH     = 10.0f;
        static constexpr float SliderKnobR      = 14.0f;
        static constexpr float ValueAreaW       = 520.0f;
        static constexpr float CycleArrowW      = 28.0f;
        static constexpr float CycleArrowGap    = 2.0f;
        static constexpr float CycleArrowScale  = 1.28f;

        static constexpr float RemapColGap       = 40.0f;
        static constexpr float RemapHeaderH      = 48.0f;
        static constexpr float RemapFooterH      = 72.0f;
        static constexpr float RemapFooterGap    = 48.0f;
        static constexpr float RemapRowH         = 56.0f;
        static constexpr float RemapFont         = 26.0f;
        static constexpr float RemapWheelImpulse = 220.0f;
        static constexpr float RemapWheelMult    = 3.5f;
        static constexpr float RemapVelDamp      = 7.0f;
        static constexpr float RemapDisplaySmooth = 14.0f;
        static constexpr float RemapWaitBandH    = 160.0f;
        static constexpr DWORD RemapWaitBandBg   = 0xE0000000;

        // Options → Radar appearance (list left, live preview right)
        static constexpr float RadarPreviewW    = 520.0f;
        static constexpr float RadarPreviewGap  = 28.0f;
        static constexpr float RadarPreviewPad  = 36.0f;
        static constexpr float ColorSwatch      = 36.0f;
        static constexpr float ColorSwatchGap   = 8.0f;
        static constexpr int   ColorPresetCount = 8;

        static constexpr float RowH() { return PanelH / static_cast<float>(SlotCount); }

        static constexpr DWORD TabIdleBg   = 0xC8000000;
        static constexpr DWORD TabHoverBg  = 0xE1000000;
        static constexpr DWORD TabActiveBg = 0xFFFFFFFF;
        static constexpr DWORD TabActiveFg = 0xFF000000;
        static constexpr DWORD SlotZebraA  = 0xC8000000;
        static constexpr DWORD SlotZebraB  = 0xFF000000;
        static constexpr DWORD SlotHoverBg = 0xC84A4A4A;
        static constexpr DWORD SliderTrack = 0xFF2A2A2A;
        static constexpr DWORD SliderFill  = 0xFFB0B0B0;
        static constexpr DWORD SliderKnob  = 0xFFFFFFFF;
        static constexpr DWORD LockedFg    = 0xFF707070;
    };

    enum class SettingsTab : int
    {
        Controls = 0,
        Game,
        Graphics,
        Sound,
        Options
    };

    enum class SettingKind : int
    {
        Action = 0,
        Toggle,
        Cycle,
        Slider,
        ColorPresets
    };

    enum class SettingId : int
    {
        RedefineControls = 0,
        InvertLookY,
        InvertLookX,
        MouseSensX,
        MouseSensY,
        MouseSteer,
        MouseFly,
        RestoreControls,

        ShowHud,
        RadarMode,
        Widescreen,
        RestoreGame,

        Brightness,
        DrawDistance,
        FrameLimiter,
        FxQuality,
        AntiAlias,
        MipMapping,
        WindowMode,
        Resolution,
        HeatHaze,
        SpeedBlur,
        RestoreGraphics,

        SfxVolume,
        MusicVolume,
        RadioAuto,
        RadioEq,
        RestoreSound,

        Language,
        Subtitles,
        UpdatedHelp,
        RadioText,
        Gps,
        SavePhotos,
        DeIcons,
        CustomRadarTxd,
        ShowGangZones,
        BlipEdgeFade,
        BlipIconScale,
        RadarAppearance,
        RestoreOptions,

        RadarShape,
        RadarViewMode,
        RadarSize,
        RadarSizeW,
        RadarSizeH,
        RadarOffsetX,
        RadarOffsetY,
        RadarBorderThick,
        RadarBgColor,
        RadarBgR,
        RadarBgG,
        RadarBgB,
        RadarBgA,
        RadarBorderColor,
        RadarBorderR,
        RadarBorderG,
        RadarBorderB,
        RadarBorderA,
        RestoreRadar,
        BackRadar,

        RadarCameraSettings,
        RadarCamContext,
        RadarCamOffsetX,
        RadarCamOffsetY,
        RadarCamOffsetZ,
        RadarCamPitch,
        RadarCamFov,
        RadarCullRadiusMul,
        RadarCullRadiusAdd,
        RestoreRadarCamera,
        BackRadarCamera,

        Count
    };

    struct SettingDef
    {
        SettingId   id;
        SettingKind kind;
        const char* label;
    };

    struct RemapBind
    {
        int         action; // e_ControllerAction
        const char* label;  // LanguageManager / GXT key (FEC_*)
    };

    void Bind(Draw* draw, LPDIRECT3DDEVICE9 device, bool* pendingVideo);
    void SetRadarPreviewSource(Radar* radar, std::function<bool()> ensureRadarGpu);
    void OnDeviceLost();

    void Open();
    void Close();
    void CancelDrag();

    bool IsRebindWaiting() const { return m_rebindAction >= 0; }
    bool IsDragging() const { return m_dragSetting != SettingId::Count; }

    void Render(float screenW, float screenH);
    void HandleSliderDrag(float screenW, float screenH);
    bool HandleClick(float cx, float cy, float screenW, float screenH, bool& outClosePanel);
    int  HitTestHoverSoundId(float cursorX, float cursorY, float screenW, float screenH) const;

private:
    using ButtonRect = UiRect;

    void DrawUiText(float left, float top, float right, float bottom, const char* text,
                    DWORD format, bool hovered, float screenW, float screenH,
                    bool onActivePlate = false, bool solidIdle = false);
    bool GetCursorPosClient(float screenW, float screenH, float& outX, float& outY) const;
    void QueueVideoModeApply();

    ButtonRect GetTabRect(int tab, float tabW, float screenW, float screenH) const;
    ButtonRect GetSlotRect(int slot, float screenW, float screenH) const;
    ButtonRect GetPanelRect(float screenW, float screenH) const;
    ButtonRect GetSettingsListRect(float screenW, float screenH) const;
    ButtonRect GetRadarPreviewRect(float screenW, float screenH) const;
    ButtonRect GetSliderTrackRect(const ButtonRect& row, float screenW, float screenH) const;
    void GetCycleArrowRects(const ButtonRect& row, float screenW, float screenH,
                            const char* valueText,
                            ButtonRect& outLeft, ButtonRect& outRight,
                            float* outValueLeft = nullptr, float* outValueRight = nullptr) const;
    void GetColorSwatchRects(const ButtonRect& row, float screenW, float screenH,
                             ButtonRect outSwatches[Layout::ColorPresetCount],
                             ButtonRect& outCustom) const;

    void GetRemapColumnRect(int col, float screenW, float screenH, float& outLeft, float& outRight) const;
    ButtonRect GetRemapHeaderRect(int col, float screenW, float screenH) const;
    ButtonRect GetRemapListClipRect(int col, float screenW, float screenH) const;
    ButtonRect GetRemapRowRect(int col, int row, float screenW, float screenH) const;
    ButtonRect GetRemapFooterRect(int which, float screenW, float screenH) const;
    float GetRemapMaxScroll(int col, float screenW, float screenH) const;
    void ClampRemapScroll(int col, float screenW, float screenH);
    void UpdateRemapScroll(float screenW, float screenH);
    void UpdateRebindCapture();
    void BeginRebind(int action);
    void CancelRebind();
    void ApplyRebindKey(unsigned rsKey, bool isMouse);
    int  PollMouseWheelDelta() const;

    void DrawSettingRow(int row, const SettingDef& def, float screenW, float screenH,
                        float cursorX, float cursorY);
    void DrawControlsRemap(float screenW, float screenH, float cursorX, float cursorY);
    void DrawRadarPreview(float screenW, float screenH);
    void OnSettingActivated(SettingId id, float cursorX, float cursorY, float screenW, float screenH);
    bool GetResolutionDepthHit(const ButtonRect& row, float screenW, float screenH,
                               ButtonRect& outDepth) const;
    void ToggleResolutionBitDepth();
    void ApplySlider(SettingId id, float t01, bool persist = true);
    float GetSlider01(SettingId id) const;
    void CycleSetting(SettingId id, int dir);
    void ToggleSetting(SettingId id);
    void RestoreTabDefaults(SettingsTab tab);
    void PersistSettings();
    void PersistRadarGeometry();
    void ResetControlBindings();
    std::string FormatSettingValue(SettingId id) const;
    std::string FormatActionKey(int action) const;
    static std::string KeyCodeToName(unsigned key);
    const SettingDef* GetSettingsForTab(SettingsTab tab, int& outCount) const;
    void BuildRadarAppearanceRows(SettingDef* out, int& outCount) const;
    void BuildRadarCameraRows(SettingDef* out, int& outCount) const;
    RadarConfig::RadarCamContext GetRadarCamEditContext() const;
    static const RemapBind* GetRemapBinds(int col, int& outCount);
    static bool ColorMatches(int r, int g, int b, int pr, int pg, int pb);
    bool IsSettingLocked(SettingId id) const;
    int FindBgPresetIndex() const;
    int FindBorderPresetIndex() const;
    void ApplyBgPreset(int index);
    void ApplyBorderPreset(int index);
    void SyncColorCustomFlags();

    LPDIRECT3DDEVICE9 m_pDevice = nullptr;
    Draw*             m_pDraw = nullptr;
    bool*             m_pendingVideo = nullptr;
    Radar*            m_pRadar = nullptr;
    std::function<bool()> m_ensureRadarGpu;

    SettingsTab m_settingsTab = SettingsTab::Controls;
    SettingId   m_dragSetting = SettingId::Count;
    bool        m_bControlsRemap = false;
    bool        m_bRadarOptions = false;
    bool        m_bRadarCameraOptions = false;
    int         m_nRadarCamEditCtx = 0;
    bool        m_bgColorCustom = false;
    bool        m_borderColorCustom = false;
    int         m_nRemapHoverCol = -1;
    int         m_nRemapHoverRow = -1;
    float       m_remapScroll[2] = {};
    float       m_remapScrollDisplay[2] = {};
    float       m_remapVel[2] = {};
    int         m_remapDragCol = -1;
    float       m_remapDragLastY = 0.0f;
    DWORD       m_remapLastTick = 0;
    int         m_rebindAction = -1;
    bool        m_rebindArmed = false;
    int         m_remapPressCol = -1;
    int         m_remapPressRow = -1;
    float       m_remapPressY = 0.0f;
    bool        m_remapPressMoved = false;
    bool        m_remapLmbWas = false;
    bool        m_remapSwallowLmb = false;
};
