/*****************************************************************************
 *
 *  PROJECT:     The-Definitive-UI
 *  FILE:        source/Utils/Utils.h
 *  PURPOSE:     Shared UI helpers (hit-rects, 2K scale, cursor, screen size)
 *
 *****************************************************************************/

#pragma once

#include <d3d9.h>

class Draw;

struct UiRect
{
    float left = 0.0f;
    float top = 0.0f;
    float right = 0.0f;
    float bottom = 0.0f;

    bool Contains(float x, float y) const
    {
        return x >= left && x < right && y >= top && y < bottom;
    }
};

namespace Ui
{
    constexpr float kRefW = 2560.0f;
    constexpr float kRefH = 1440.0f;
    constexpr DWORD kOutlineColor = 0xFF588942;
    constexpr float kOutlineOffsetX = 3.0f;
    constexpr float kOutlineOffsetY = 3.0f;

    inline float Sx(float screenW) { return screenW / kRefW; }
    inline float Sy(float screenH) { return screenH / kRefH; }

    HWND GameWindow();
    bool IsGameWindowFocused();

    bool GetBackbufferSize(float& outW, float& outH);
    // Pause/map: must match CRadar::Limit (SCREEN_WIDTH), not the D3D viewport
    void GetScreenSizeGame(float& outW, float& outH);
    // Frontend menu: live D3D backbuffer (RsGlobal lags / CameraSize exclusive VM)
    void GetScreenSizeViewport(LPDIRECT3DDEVICE9 device, float& outW, float& outH);

    bool GetCursorPosClient(float screenW, float screenH, float& outX, float& outY);

    void ShowOsCursor();
    void HideOsCursor();

    // Force RW to rebind VS/PS next draw (D3DX leftover vs FFP skip).
    void PoisonRwShaderCache();

    void DrawMenuText(Draw* draw,
                      float left, float top, float right, float bottom,
                      const char* text, DWORD format, bool hovered,
                      float screenW, float screenH,
                      bool onActivePlate, bool solidIdle);

    // Idle: texture + cutout label. Hover: hover texture + black label.
    void DrawTexturedConfirmButton(Draw* draw,
                                   float left, float top, float right, float bottom,
                                   LPDIRECT3DTEXTURE9 idleTex, LPDIRECT3DTEXTURE9 hoverTex,
                                   bool hovered, const char* label,
                                   float screenW, float screenH);
}
