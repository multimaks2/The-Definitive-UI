/*****************************************************************************
 *
 *  PROJECT:     The-Definitive-UI
 *  FILE:        source/Utils/Utils.cpp
 *  PURPOSE:     Shared UI helpers
 *
 *****************************************************************************/

#include "Utils.h"

#include "Draw/Draw.h"
#include "plugin.h"
#include "RenderWare.h"

void Ui::PoisonRwShaderCache()
{
    // 0xFFFFFFFF never equals a real shader; nullptr matches FFP and skips SetPixelShader.
    *reinterpret_cast<unsigned*>(0x8E2440) = 0xFFFFFFFFu;
    *reinterpret_cast<void**>(0x8E2444) = reinterpret_cast<void*>(static_cast<uintptr_t>(-1));
    *reinterpret_cast<void**>(0x8E2448) = reinterpret_cast<void*>(static_cast<uintptr_t>(-1));
    *reinterpret_cast<void**>(0x8E244C) = reinterpret_cast<void*>(static_cast<uintptr_t>(-1));
    *reinterpret_cast<void**>(0x8E2450) = reinterpret_cast<void*>(static_cast<uintptr_t>(-1));
}

HWND Ui::GameWindow()
{
    if (RsGlobal.ps && RsGlobal.ps->window)
        return RsGlobal.ps->window;
    return nullptr;
}

bool Ui::IsGameWindowFocused()
{
    HWND hwnd = GameWindow();
    if (!hwnd)
        return true;
    return GetForegroundWindow() == hwnd;
}

bool Ui::GetBackbufferSize(float& outW, float& outH)
{
    auto* dev = static_cast<IDirect3DDevice9*>(RwD3D9GetCurrentD3DDevice());
    if (!dev)
        return false;

    IDirect3DSurface9* bb = nullptr;
    if (FAILED(dev->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &bb)) || !bb)
        return false;

    D3DSURFACE_DESC desc{};
    bb->GetDesc(&desc);
    bb->Release();
    if (desc.Width < 64 || desc.Height < 64)
        return false;

    outW = static_cast<float>(desc.Width);
    outH = static_cast<float>(desc.Height);
    return true;
}

void Ui::GetScreenSizeGame(float& outW, float& outH)
{
    outW = SCREEN_WIDTH;
    outH = SCREEN_HEIGHT;
}

void Ui::GetScreenSizeViewport(LPDIRECT3DDEVICE9 device, float& outW, float& outH)
{
    if (GetBackbufferSize(outW, outH))
        return;

    outW = SCREEN_WIDTH;
    outH = SCREEN_HEIGHT;

    D3DVIEWPORT9 vp{};
    if (!device || FAILED(device->GetViewport(&vp)) || vp.Width < 64 || vp.Height < 64)
        return;

    outW = static_cast<float>(vp.Width);
    outH = static_cast<float>(vp.Height);
}

bool Ui::GetCursorPosClient(float screenW, float screenH, float& outX, float& outY)
{
    POINT pt{};
    if (!GetCursorPos(&pt))
        return false;

    HWND hwnd = GameWindow();
    if (hwnd)
    {
        if (!ScreenToClient(hwnd, &pt))
            return false;

        RECT rc{};
        if (GetClientRect(hwnd, &rc) && rc.right > 0 && rc.bottom > 0)
        {
            outX = static_cast<float>(pt.x) * (screenW / static_cast<float>(rc.right));
            outY = static_cast<float>(pt.y) * (screenH / static_cast<float>(rc.bottom));
            return true;
        }
    }

    outX = static_cast<float>(pt.x);
    outY = static_cast<float>(pt.y);
    return true;
}

void Ui::ShowOsCursor()
{
    CURSORINFO ci{ sizeof(ci) };
    if (GetCursorInfo(&ci) && (ci.flags & CURSOR_SHOWING))
    {
        ClipCursor(nullptr);
        return;
    }

    SetCursor(LoadCursor(nullptr, IDC_ARROW));
    ClipCursor(nullptr);
    while (ShowCursor(TRUE) < 0)
    {
    }
}

void Ui::HideOsCursor()
{
    SetCursor(nullptr);
    while (ShowCursor(FALSE) >= 0)
    {
    }
}

void Ui::DrawMenuText(Draw* draw,
                      float left, float top, float right, float bottom,
                      const char* text, DWORD format, bool hovered,
                      float screenW, float screenH,
                      bool onActivePlate, bool solidIdle)
{
    if (!draw || !text)
        return;

    const float ox = kOutlineOffsetX * Sx(screenW);
    const float oy = kOutlineOffsetY * Sy(screenH);

    if (onActivePlate)
    {
        draw->DrawString(left, top, right, bottom, 0xFF000000, text, 1.0f, 1.0f,
                         format, false);
        return;
    }

    if (hovered)
    {
        draw->DrawString(left, top, right, bottom, 0xFFFFFFFF, text, 1.0f, 1.0f,
                         format, false);
        return;
    }

    if (solidIdle)
    {
        draw->DrawString(left, top, right, bottom, 0xFFFFFFFF, text, 1.0f, 1.0f,
                         format, true, kOutlineColor, ox, oy);
        return;
    }

    draw->DrawString(left, top, right, bottom, 0x00FFFFFF, text, 1.0f, 1.0f,
                     format, true, kOutlineColor, ox, oy);
}

void Ui::DrawTexturedConfirmButton(Draw* draw,
                                   float left, float top, float right, float bottom,
                                   LPDIRECT3DTEXTURE9 idleTex, LPDIRECT3DTEXTURE9 hoverTex,
                                   bool hovered, const char* label,
                                   float screenW, float screenH)
{
    if (!draw)
        return;

    const float w = right - left;
    const float h = bottom - top;
    const DWORD fmt = DT_CENTER | DT_VCENTER | DT_NOCLIP | DT_SINGLELINE;

    if (hovered)
    {
        if (hoverTex)
            draw->DrawTexture(hoverTex, left, top, w, h);
        else if (idleTex)
            draw->DrawTexture(idleTex, left, top, w, h);
        DrawMenuText(draw, left, top, right, bottom, label, fmt, false, screenW, screenH, true, false);
        return;
    }

    if (idleTex)
    {
        draw->DrawTextureCutoutText(idleTex, left, top, w, h, left, top, right, bottom, label, fmt);
        return;
    }

    draw->DrawRectCutoutText(left, top, w, h, 0xC8000000, left, top, right, bottom, label, fmt);
}
