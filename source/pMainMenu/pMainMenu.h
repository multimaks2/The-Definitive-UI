/*****************************************************************************
 *
 *  PROJECT:     The-Definitive-UI
 *  FILE:        source/pMainMenu/pMainMenu.h
 *  PURPOSE:     Process (in-game) pause menu — hosts MainMenu panels
 *
 *****************************************************************************/

#pragma once

#include <d3d9.h>
#include <cstddef>

#include "TxdManager.h"
#include "Utils.h"

class Draw;
class HookManager;
class MainMenu;
class Shader;
class GameSettings;
class MapChunkManager;

// p = process: menu while the game is loaded (not frontend MainMenu)
class pMainMenu
{
public:
    struct Path
    {
        static constexpr const char* MenuTxd = "The-Definitive-UI.SA\\MainMenu.txd";
        static constexpr const char* MapTxd  = "The-Definitive-UI.SA\\map.txd";
    };

    struct Tex
    {
        static constexpr const char* Background = "MainMenu";
        static constexpr const char* Start      = "Start";
        static constexpr const char* Game       = "Game";
        static constexpr const char* Settings   = "Settings";
        static constexpr const char* Exit       = "Exit";
        static constexpr const char* ExitNoHoverCancel = "ExitNoHoverCancel";
        static constexpr const char* ExitHoverCancel   = "ExitHoverCancel";
        static constexpr const char* ExitNoHoverAccept = "ExitNoHoverAccept";
        static constexpr const char* ExitHoverAccept   = "ExitHoverAccept";
        static constexpr const char* Logo              = "Logo";
    };

    // 2K reference (2560x1440). Button Y is from the BOTTOM of the screen.
    struct Layout
    {
        static constexpr float RefW = 2560.0f;
        static constexpr float RefH = 1440.0f;

        static constexpr float CenterX      = 330.0f;
        static constexpr float FirstCenterY = 620.0f; // 7 rows
        static constexpr float RowStepY     = 85.0f;

        static constexpr float FontSize     = 50.0f;
        static constexpr float TextHeight   = 50.0f;
        static constexpr float HoverWidth   = 388.0f;
        static constexpr float HoverHeight  = 78.0f;

        static constexpr float LogoPadX     = 60.0f;
        static constexpr float LogoPadY     = 80.0f;
        static constexpr float LogoW        = 575.0f;
        static constexpr float LogoH        = 510.0f;

        static constexpr DWORD OutlineColor   = 0xFF588942;
        static constexpr float OutlineOffsetX = 3.0f;
        static constexpr float OutlineOffsetY = 3.0f;

        // Map mode: left strip keeps menu art; right = radar tile plane
        static constexpr float MapLeftW      = 662.0f;
        static constexpr float MapLeftFrac   = MapLeftW / RefW; // ~0.2586
        static constexpr DWORD MapUnderlay   = 0xFF7BC4F9; // stock sea / outside map
        static constexpr DWORD MapFogColor   = 0xC8000000; // black, alpha 200
        static constexpr int   MapTilesX     = 12;
        static constexpr int   MapTilesY     = 12;
        static constexpr int   MapTileCount  = MapTilesX * MapTilesY; // 144
        static constexpr float MapDragThreshold = 18.0f; // px — click vs pan
        static constexpr float MapZoomStep   = 1.25f;  // discrete notch
        static constexpr float MapZoomMin    = MapZoomStep * MapZoomStep; // 2 notches above 1.0
        static constexpr float MapZoomMax    = 5.0f;
        static constexpr int   MapZoomDefaultSteps = 4;
        static constexpr float MapZoomDefault =
            MapZoomStep * MapZoomStep * MapZoomStep * MapZoomStep; // 4 notches from 1.0
        static constexpr float MapZoomLerp   = 0.22f;  // inertia toward target
        static constexpr float MapPanWorldX  = 3500.0f; // overscroll past tile edge (±3000) on X
        static constexpr float MapPanWorldY  = 3300.0f; // overscroll past tile edge (±3000) on Y

        // Fullscreen map hint bar (2K ref) — right edge is the static anchor
        static constexpr float MapHintInsetX = 76.0f;  // right margin (fixed)
        static constexpr float MapHintInsetY = 71.0f;  // bottom margin (fixed)
        static constexpr float MapHintH      = 72.0f;  // min height; grows with font
        static constexpr DWORD MapHintColor  = 0xC8000000; // black, alpha 200
        static constexpr float MapHintPadX   = 28.0f;
        static constexpr float MapHintPadY   = 10.0f;
        static constexpr float MapHintFont   = 28.0f;  // controls row inside bar
        static constexpr float MapZoneFont   = 36.0f;  // place name above bar
        static constexpr float MapZoneGap    = 12.0f;  // gap between name and bar top
        static constexpr float MapHintGap    = 36.0f;  // space between hint groups

        // Fullscreen map legend (2K) — TAB toggle
        static constexpr float LegendX       = 200.0f;
        static constexpr float LegendY       = 70.0f;
        static constexpr float LegendW       = 515.0f;
        static constexpr float LegendH       = 1200.0f;
        static constexpr float LegendPadX    = 20.0f;
        static constexpr float LegendIcon    = 48.0f;
        static constexpr float LegendIconGap = 16.0f;
        static constexpr float LegendFont    = 32.0f;
        static constexpr DWORD LegendZebraA  = 0xE1000000; // α225
        static constexpr DWORD LegendZebraB  = 0xC8000000; // α200

        // Pause Exit confirm — origin = screen center (2K)
        static constexpr float ExitConfirmBtnW       = 480.0f;
        static constexpr float ExitConfirmBtnH       = 90.0f;
        static constexpr float ExitConfirmBtnGap     = 40.0f;
        static constexpr float ExitConfirmBtnDown    = 0.85f; // × pair body H, down from center
        static constexpr float ExitConfirmTextUp     = 4.0f;  // × font body, up from center
        static constexpr float ExitConfirmFont       = 50.0f;

        // ZonesVisited 10×10 — contour + shader fog (per-pixel arcs)
        static constexpr int   MapFogCells       = 10;
        static constexpr int   MapFogRevealAll   = 80;
        static constexpr int   MapFogMaskPx      = 48; // fallback CPU mask only
        static constexpr DWORD MapExploredLine   = 0xFFFF7A00;
        static constexpr float MapExploredLineW  = 2.0f;
        static constexpr float MapExploredCorner = 0.175f;
        static constexpr float MapFogSoft        = 0.14f; // soft along contour into fog

        static constexpr int Count = 7;

        static float CenterYFromTop(int index)
        {
            return RefH - (FirstCenterY - RowStepY * static_cast<float>(index));
        }
    };

    enum class Button : int
    {
        Continue = 0,
        Map,
        Game,
        Messages,
        Stats,
        Settings,
        Exit
    };

    pMainMenu();
    ~pMainMenu();

    bool Initialize(LPDIRECT3DDEVICE9 pDevice, Draw* pDraw, TxdManager* pTxd,
                    HookManager* pHooks, MainMenu* pSharedMainMenu, Shader* pShader,
                    GameSettings* pSettings);
    void Shutdown();
    void OnDeviceLost();

    void Render();

    // Reuse HUD radar tiles (already decoded) — kills first-open hitch.
    void SetMapChunkManager(MapChunkManager* chunks) { m_pMapChunks = chunks; }

    bool IsInitialized() const { return m_bInitialized; }

private:
    using ButtonRect = UiRect;

    struct MapLayout
    {
        float areaL, areaT, areaR, areaB; // underlay / hit zone
        float mapL, mapT, mapSize;        // fitted square + pan
    };

    bool LoadBackground();
    bool LoadHoverTextures();
    bool LoadMapTextures();
    bool EnsureStockRadarTextures();
    bool MapTilesReady() const;
    void WarmMapResources();
    bool WarmMapTexturesStep(int budget);
    void ReleaseMapTextures();
    void GetScreenSize(float& outW, float& outH) const;
    bool GetCursorPosClient(float screenW, float screenH, float& outX, float& outY) const;
    void ShowOsCursor();
    void HideOsCursor();

    ButtonRect GetButtonRect(int index, float screenW, float screenH) const;
    ButtonRect GetExitConfirmBtnRect(int which, float screenW, float screenH) const; // 0=Cancel 1=Confirm
    int  HitTestButton(float cursorX, float cursorY, float screenW, float screenH) const;
    void SyncActiveFromPanel();
    void UpdateHoverSound(float screenW, float screenH);
    void HandleClicks(float screenW, float screenH);
    void HandleMapInput(float screenW, float screenH);
    void UpdateMapZoom(float screenW, float screenH);
    void HandleEscape();
    void LeavePauseSession();
    void ResumeGame();
    void OnButtonActivated(int index);
    void EnterMapFullscreen();
    void ExitMapFullscreen();
    void ResetMapView();
    void CenterMapOnPlayer(float screenW = 0.0f, float screenH = 0.0f);
    MapLayout ComputeMapLayout(float screenW, float screenH) const;
    void ClampMapPan(float areaW, float areaH, float mapSize);
    void ApplyMapZoomFocus(float screenW, float screenH);
    bool MapScreenToWorld(float screenX, float screenY, const MapLayout& lay, float& outX, float& outY) const;
    void ClearMapWaypoint();
    void PlaceMapWaypoint(float worldX, float worldY);

    void DrawUiText(float left, float top, float right, float bottom, const char* text,
                    DWORD format, bool hovered, float screenW, float screenH,
                    bool onActivePlate = false, bool solidIdle = false);
    void DrawButtons(float screenW, float screenH);
    void DrawLogo(float screenW, float screenH);
    void DrawExitConfirm(float screenW, float screenH);
    void OpenExitConfirm();
    void CloseExitConfirm();
    void DrawMapPlane(float mapL, float mapT, float mapSize);
    void DrawMapFogShader(float mapL, float mapT, float mapSize,
                         float coverL, float coverT, float coverW, float coverH);
    bool EnsureFogMaskRT();
    void ReleaseFogMaskRT();
    bool BuildFogContourMask();
    bool EnsureZonesTex();
    void ReleaseZonesTex();
    bool UploadZonesTex();
    void DrawMapExploredOutlines(float mapL, float mapT, float mapSize);
    void DrawMapTiles(float screenW, float screenH);
    void DrawMapGps(float screenW, float screenH);
    void DrawMapLeftArt(float screenW, float screenH);
    void DrawMapBlips(float screenW, float screenH);
    void DrawMapHintBar(float screenW, float screenH);
    void DrawMapLegend(float screenW, float screenH);
    void HandleMapLegendToggle();
    ButtonRect GetMapLegendRect(float screenW, float screenH) const;
    bool IsCursorOnMapLegend(float cursorX, float cursorY, float screenW, float screenH) const;
    // Zone under cursor for fullscreen map; dictionary via LanguageManager::GetZone
    void GetMapHoverPlaceNameUtf8(float screenW, float screenH, char* out, size_t outChars) const;
    bool IsMapOpen() const { return m_nActive == static_cast<int>(Button::Map); }
    static void ConsumeEscJustPressed();

    LPDIRECT3DDEVICE9  m_pDevice = nullptr;
    Draw*              m_pDraw = nullptr;
    TxdManager*        m_pTxd = nullptr;   // shared MainMenu.txd
    TxdManager         m_mapTxd;           // The-Definitive-UI.SA\map.txd (radar00..143)
    HookManager*       m_pHooks = nullptr;
    MainMenu*          m_pMainMenu = nullptr;
    GameSettings*      m_pSettings = nullptr;
    Shader*            m_pShader = nullptr;
    MapChunkManager*   m_pMapChunks = nullptr; // RadarRenderer owns; borrow for pause map
    LPDIRECT3DTEXTURE9 m_pBackground = nullptr;
    LPDIRECT3DTEXTURE9 m_pLogo = nullptr;
    LPDIRECT3DTEXTURE9 m_pHover[Layout::Count] = {};
    LPDIRECT3DTEXTURE9 m_pExitBtnIdle[2] = {};
    LPDIRECT3DTEXTURE9 m_pExitBtnHover[2] = {};
    LPDIRECT3DTEXTURE9 m_pRadar[Layout::MapTileCount] = {};
    LPDIRECT3DTEXTURE9 m_pRadarStock[Layout::MapTileCount] = {};
    LPDIRECT3DTEXTURE9 m_pFogMaskTex = nullptr;
    LPDIRECT3DSURFACE9 m_pFogMaskSurf = nullptr;
    int                m_nFogMaskSize = 0;
    LPDIRECT3DTEXTURE9 m_pZonesTex = nullptr;
    bool               m_bMapTxdReady = false;
    bool               m_bStockRadarReady = false;
    int                m_nMapWarmIndex = 0; // progressive fallback load
    bool               m_bMapFullscreen = false;
    bool               m_bMapLegend = false;
    bool               m_bTabWasDown = false;
    float              m_fMapPanX = 0.0f;
    float              m_fMapPanY = 0.0f;
    float              m_fMapZoom = Layout::MapZoomDefault;
    float              m_fMapZoomTarget = Layout::MapZoomDefault;
    bool               m_bMapZoomFocus = false; // keep wheel focus until lerp settles
    float              m_fMapZoomFocusSX = 0.0f;
    float              m_fMapZoomFocusSY = 0.0f;
    float              m_fMapZoomFocusRelX = 0.5f;
    float              m_fMapZoomFocusRelY = 0.5f;
    bool               m_bMapPress = false;      // LMB down on map area
    bool               m_bMapDragging = false;   // passed drag threshold
    bool               m_bMapPressOpenedFs = false; // this press entered fullscreen — don't place
    float              m_fMapDragLastX = 0.0f;
    float              m_fMapDragLastY = 0.0f;
    float              m_fMapDragStartX = 0.0f;
    float              m_fMapDragStartY = 0.0f;
    float              m_fMapClickWorldX = 0.0f;
    float              m_fMapClickWorldY = 0.0f;
    bool               m_bMapClickWorldValid = false;
    int                m_nHovered = -1;
    int                m_nHoverSoundId = -1;
    int                m_nActive = -1;
    bool               m_bLmbWasDown = false;
    bool               m_bRmbWasDown = false;
    bool               m_bSwallowClick = false;
    bool               m_bWasFocused = true;
    bool               m_bPauseWasOpen = false;
    bool               m_bExitConfirm = false;
    bool               m_bOsCursorHeld = false;
    bool               m_bInitialized = false;
};
