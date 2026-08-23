/*****************************************************************************
 *
 *  PROJECT:     The-Definitive-UI
 *  FILE:        source/Game/WindowMode.cpp
 *  PURPOSE:     Windowed / exclusive / borderless — MTA CVideoModeManager flow
 *
 *  Port of mtasa-blue Client/core/Graphics/CVideoModeManager.cpp:
 *    PreReset  — force Windowed + backbuffer, FLIP→DISCARD
 *    PostReset — frame AFTER Reset, client == config WxH, recenter on work area
 *    Title: "GTA: San Andreas  —  1920x1080  —  Windowed"
 *    PreCreate — WS_POPUP + MoveWindow to config WxH
 *
 *  Config is the only size source. Never re-apply measured client/BB.
 *
 *****************************************************************************/

#include "WindowMode.h"

#include "plugin.h"
#include "Patch.h"
#include "RenderWare.h"
#include "CGame.h"
#include "CMenuManager.h"
#include "CPostEffects.h"
#include "CScene.h"
#include "Config.h"
#include "Utils.h"
#include "LanguageManager.h"

#include "safetyhook.hpp"

#include <windows.h>
#include <cstdio>
#include <cstring>

namespace
{
    constexpr uintptr_t kWindowHandleAddr = 0xC97C1C;
    constexpr uintptr_t kPresentAddr      = 0xC9C040;
    constexpr uintptr_t kAdapterInfoAddr  = 0xC9BCE0;
    constexpr uintptr_t kReleaseVram      = 0x7F7F70;
    constexpr uintptr_t kRasterRestore    = 0x4CC970;
    constexpr uintptr_t kDynVbRestore     = 0x7F58D0;
    constexpr uintptr_t kRenderStateReset = 0x7FD100;
    constexpr uintptr_t kIm2DOpen         = 0x7FB480;
    constexpr uintptr_t kIm3DOpen         = 0x80E020;
    constexpr uintptr_t kRestoreCallback  = 0xC980B0;
    constexpr uintptr_t kRtSurface        = 0xC97C30;
    constexpr uintptr_t kDsSurface        = 0xC97C2C;
    constexpr uintptr_t kWindowedFlagAddr = 0xC920CC;
    constexpr uintptr_t kInsideSceneAddr  = 0xC97C54;
    constexpr uintptr_t kSetVideoModeAddr = 0x745C70;
    constexpr uintptr_t kIsExclusiveAddr  = 0x745CA0; // IsVideoModeExclusive
    constexpr uintptr_t kDIReleaseMouse   = 0x746F70;
    constexpr uintptr_t kDiMouseInit      = 0x7469A0;
    constexpr uintptr_t kCurSelVideoMode  = 0x8D6220;
    constexpr uintptr_t kDisplayModesPtr  = 0xC97C48;
    constexpr uintptr_t kNumDisplayModes  = 0xC97C40;
    constexpr LONG      kWindowedStyle    = WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX
                                          | WS_VISIBLE | WS_CLIPSIBLINGS;
    constexpr uintptr_t kD3dObjectAddr    = 0xC97C20;
    constexpr uintptr_t kAdapterIndexAddr = 0xC97C24;
    constexpr size_t    kAdapterModeOff   = 516;
    constexpr int       kMinW             = 640;
    constexpr int       kMinH             = 480;

    D3DPRESENT_PARAMETERS& PresentParams()
    {
        return *reinterpret_cast<D3DPRESENT_PARAMETERS*>(kPresentAddr);
    }

    HWND DeviceWindow()
    {
        HWND hwnd = *reinterpret_cast<HWND*>(kWindowHandleAddr);
        if (hwnd)
            return hwnd;
        return Ui::GameWindow();
    }

    int VideoModeIndex()
    {
        int mode = FrontEndMenuManager.m_nPrefsVideoMode;
        if (mode < 0)
            mode = FrontEndMenuManager.m_nDisplayVideoMode;
        if (mode < 0)
            mode = RwEngineGetCurrentVideoMode();
        return mode;
    }

    bool ModeSize(int modeIndex, int& outW, int& outH)
    {
        RwVideoMode info{};
        if (!RwEngineGetVideoModeInfo(&info, modeIndex))
            return false;
        if (info.width < kMinW || info.height < kMinH)
            return false;
        outW = info.width;
        outH = info.height;
        return true;
    }

    int ModeDepth(int modeIndex)
    {
        char** list = plugin::CallAndReturn<char**, 0x745AF0>();
        if (list && modeIndex >= 0 && list[modeIndex] && list[modeIndex][0])
        {
            int w = 0, h = 0, d = 0;
            if (sscanf_s(list[modeIndex], "%d X %d X %d", &w, &h, &d) == 3
                && (d == 16 || d == 32))
                return d;
        }
        RwVideoMode info{};
        if (!RwEngineGetVideoModeInfo(&info, modeIndex))
            return 0;
        return static_cast<int>(info.depth);
    }

    int FindListedVideoMode(int w, int h, int preferDepth = 0)
    {
        const int n = RwEngineGetNumVideoModes();
        if (n <= 0 || w < kMinW || h < kMinH)
            return -1;

        char** list = plugin::CallAndReturn<char**, 0x745AF0>();
        int any = -1;
        int listed = -1;
        int matchDepth = -1;
        for (int i = 0; i < n; ++i)
        {
            int mw = 0, mh = 0;
            if (!ModeSize(i, mw, mh) || mw != w || mh != h)
                continue;
            if (any < 0)
                any = i;
            if (!list || !list[i] || !list[i][0])
                continue;
            if (listed < 0)
                listed = i;
            if (preferDepth >= 16 && ModeDepth(i) == preferDepth)
            {
                matchDepth = i;
                break;
            }
        }
        if (matchDepth >= 0)
            return matchDepth;
        return listed >= 0 ? listed : any;
    }

    bool GetMonitorRects(HWND hwnd, RECT& monitor, RECT& work)
    {
        HMONITOR mon = MonitorFromWindow(hwnd ? hwnd : GetDesktopWindow(), MONITOR_DEFAULTTONEAREST);
        MONITORINFO mi{};
        mi.cbSize = sizeof(mi);
        if (!GetMonitorInfo(mon, &mi))
            return false;
        monitor = mi.rcMonitor;
        work = mi.rcWork;
        return true;
    }

    bool GetAdapterRect(RECT& out)
    {
        RECT work{};
        HWND hwnd = DeviceWindow();
        if (!GetMonitorRects(hwnd, out, work))
            return false;
        return true;
    }

    const char* ModeLabel(int mode)
    {
        if (mode == 0)
            return LanguageManager::Get("WIN_WINDOWED");
        if (mode == 2)
            return LanguageManager::Get("WIN_BORDERLESS");
        return LanguageManager::Get("WIN_FULLSCREEN");
    }

    void UpdateWindowTitle(int mode, int w, int h)
    {
        HWND hwnd = DeviceWindow();
        if (!hwnd)
            return;
        const char* app = RsGlobal.appName ? RsGlobal.appName : "GTA: San Andreas";
        char utf8[192];
        sprintf_s(utf8, "%s - %dx%d - %s", app, w, h, ModeLabel(mode));
        wchar_t wide[192]{};
        if (MultiByteToWideChar(CP_UTF8, 0, utf8, -1, wide, 192) <= 0)
            return;
        SetWindowTextW(hwnd, wide);
    }

    int DetectMode(HWND hwnd)
    {
        if (PresentParams().Windowed == FALSE)
            return 1;
        if (!hwnd)
            return 0;
        const LONG style = GetWindowLong(hwnd, GWL_STYLE);
        if ((style & WS_CAPTION) != 0)
            return 0;
        return 2;
    }

    struct Snap
    {
        bool                  valid = false;
        int                   windowMode = 1;
        int                   videoMode = -1;
        int                   w = 0;
        int                   h = 0;
        int                   depth = 32;
        D3DPRESENT_PARAMETERS pp{};
        LONG                  style = 0;
        LONG                  exStyle = 0;
    };

    Snap s_good{};
    bool s_busy = false;
    bool s_pending = false;
    bool s_ownReset = false;
    int  s_reqMode = 1;
    int  s_reqVid = -1;
    int  s_mode = 1; // applied: 0 windowed, 1 exclusive, 2 borderless
    ULONG s_forceW = 0;
    ULONG s_forceH = 0;
    ULONG s_forceDepth = 32;
    void (*s_graphicsFlush)() = nullptr;
    void (*s_deviceLost)() = nullptr;
    SafetyHookInline s_setVideoMode{};

    bool TargetSize(int& w, int& h)
    {
        if (s_forceW >= static_cast<ULONG>(kMinW) && s_forceH >= static_cast<ULONG>(kMinH))
        {
            w = static_cast<int>(s_forceW);
            h = static_cast<int>(s_forceH);
            return true;
        }
        if (s_good.valid)
        {
            w = s_good.w;
            h = s_good.h;
            return true;
        }
        if (RadarConfig::HasWindowResolution())
        {
            w = RadarConfig::GetWindowWidth();
            h = RadarConfig::GetWindowHeight();
            return w >= kMinW && h >= kMinH;
        }
        return false;
    }

    void SyncAdapterMode(int w, int h)
    {
        auto* info = reinterpret_cast<uint8_t*>(kAdapterInfoAddr);
        *reinterpret_cast<UINT*>(info + kAdapterModeOff) = static_cast<UINT>(w);
        *reinterpret_cast<UINT*>(info + kAdapterModeOff + sizeof(UINT)) = static_cast<UINT>(h);
    }

    void SyncRsGlobal(int w, int h)
    {
        RsGlobal.maximumWidth = w;
        RsGlobal.maximumHeight = h;
        SyncAdapterMode(w, h);
        CPostEffects::SetupBackBufferVertex();
    }

    RwCamera* GameCamera()
    {
        return Scene.m_pCamera;
    }

    void PokeCameraSize(int w, int h)
    {
        RwCamera* cam = GameCamera();
        if (!cam)
            return;
        if (RwRaster* r = RwCameraGetRaster(cam))
        {
            r->width = w;
            r->height = h;
        }
        if (RwRaster* z = RwCameraGetZRaster(cam))
        {
            z->width = w;
            z->height = h;
        }
    }

    void SyncCameraRasters(int w, int h)
    {
        PokeCameraSize(w, h);
        RwRect rect{};
        rect.x = 0;
        rect.y = 0;
        rect.w = w;
        rect.h = h;
        RsEventHandler(rsCAMERASIZE, &rect);
        // Exclusive CameraSize stomps rect to RwEngine VM (not our Reset BB).
        SyncRsGlobal(w, h);
        PokeCameraSize(w, h);
    }

    void EndActiveScene()
    {
        RwBool& inside = *reinterpret_cast<RwBool*>(kInsideSceneAddr);
        if (!inside)
            return;
        if (auto* dev = WindowMode::Device())
            dev->EndScene();
        inside = FALSE;
    }

    bool IsWindowedStyle()
    {
        return s_mode != 1;
    }

    // MTA CVideoModeManager::PreCreateDevice — popup sized to BB, then Windowed=TRUE
    void PrepareWindowForReset(HWND hwnd, int mode, int w, int h)
    {
        if (!hwnd)
            return;

        RECT rc{};
        if (!GetAdapterRect(rc))
        {
            rc.left = 0;
            rc.top = 0;
            rc.right = GetSystemMetrics(SM_CXSCREEN);
            rc.bottom = GetSystemMetrics(SM_CYSCREEN);
        }

        SetWindowLong(hwnd, GWL_STYLE, WS_POPUP);
        SetWindowLong(hwnd, GWL_EXSTYLE, 0);

        if (mode == 0)
        {
            const int x = (rc.left + rc.right) / 2 - w / 2;
            const int y = (rc.top + rc.bottom) / 2 - h / 2;
            SetWindowPos(hwnd, HWND_TOP, x, y, w, h, SWP_FRAMECHANGED | SWP_SHOWWINDOW);
        }
        else if (mode == 2)
        {
            SetWindowPos(hwnd, HWND_TOP, rc.left, rc.top, w, h, SWP_FRAMECHANGED | SWP_SHOWWINDOW);
        }
    }

    // MTA CVideoModeManager::PreReset
    void PreReset(D3DPRESENT_PARAMETERS* pp)
    {
        if (!pp)
            return;
        if (IsWindowedStyle())
        {
            pp->Windowed = TRUE;
            if (pp->SwapEffect == D3DSWAPEFFECT_FLIP || pp->SwapEffect == D3DSWAPEFFECT_COPY)
                pp->SwapEffect = D3DSWAPEFFECT_DISCARD;
        }
        if (s_forceW >= static_cast<ULONG>(kMinW) && s_forceH >= static_cast<ULONG>(kMinH))
        {
            pp->BackBufferWidth = s_forceW;
            pp->BackBufferHeight = s_forceH;
        }
        pp->FullScreen_RefreshRateInHz = IsWindowedStyle() ? 0 : D3DPRESENT_RATE_DEFAULT;
        if (pp->SwapEffect == D3DSWAPEFFECT_FLIP && IsWindowedStyle())
            pp->SwapEffect = D3DSWAPEFFECT_DISCARD;
    }

    void ApplyWindowStyle(HWND hwnd, LONG style)
    {
        SetWindowLong(hwnd, GWL_STYLE, style);
        SetWindowLong(hwnd, GWL_EXSTYLE, 0);
        SetWindowPos(hwnd, nullptr, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
    }

    // Client == config WxH. Outer is OS caption/border (not a new resolution).
    void SizeWindowToClient(HWND hwnd, int clientW, int clientH)
    {
        const LONG style = GetWindowLong(hwnd, GWL_STYLE);
        const LONG ex = GetWindowLong(hwnd, GWL_EXSTYLE);
        RECT wr{ 0, 0, clientW, clientH };
        AdjustWindowRectEx(&wr, style, FALSE, ex);
        SetWindowPos(hwnd, nullptr, 0, 0, wr.right - wr.left, wr.bottom - wr.top,
                     SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
    }

    void CenterOnWorkArea(HWND hwnd, const RECT& work)
    {
        RECT wr{};
        if (!GetWindowRect(hwnd, &wr))
            return;
        const int ww = wr.right - wr.left;
        const int wh = wr.bottom - wr.top;
        const int workW = work.right - work.left;
        const int workH = work.bottom - work.top;
        int x = work.left + (workW - ww) / 2;
        int y = work.top + (workH - wh) / 2;
        if (x + ww > work.right)
            x = work.right - ww;
        if (y + wh > work.bottom)
            y = work.bottom - wh;
        if (x < work.left)
            x = work.left;
        if (y < work.top)
            y = work.top;
        SetWindowPos(hwnd, HWND_NOTOPMOST, x, y, 0, 0,
                     SWP_NOSIZE | SWP_SHOWWINDOW);
    }

    // After Reset: HWND == BB/ini, caption overlay, recenter on work area.
    void FitWindowFrame(int mode, int clientW, int clientH)
    {
        HWND hwnd = DeviceWindow();
        if (!hwnd || clientW < kMinW || clientH < kMinH)
            return;

        if (mode == 1)
        {
            ApplyWindowStyle(hwnd, WS_POPUP | WS_VISIBLE | WS_CLIPSIBLINGS);
            UpdateWindowTitle(mode, clientW, clientH);
            return;
        }

        RECT monitor{}, work{};
        if (!GetMonitorRects(hwnd, monitor, work))
        {
            monitor = { 0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN) };
            work = monitor;
        }

        if (mode == 2)
        {
            ApplyWindowStyle(hwnd, WS_POPUP | WS_VISIBLE | WS_CLIPSIBLINGS);
            SetWindowPos(hwnd, HWND_TOP, monitor.left, monitor.top, clientW, clientH,
                         SWP_SHOWWINDOW);
            UpdateWindowTitle(mode, clientW, clientH);
            return;
        }

        ApplyWindowStyle(hwnd, kWindowedStyle);
        SizeWindowToClient(hwnd, clientW, clientH);
        CenterOnWorkArea(hwnd, work);
        UpdateWindowTitle(mode, clientW, clientH);
    }

    bool FillWindowedPresent(D3DPRESENT_PARAMETERS& pp, HWND hwnd, int w, int h)
    {
        if (w < kMinW || h < kMinH)
            return false;
        pp.Windowed = TRUE;
        pp.BackBufferWidth = static_cast<UINT>(w);
        pp.BackBufferHeight = static_cast<UINT>(h);
        pp.BackBufferCount = 1;
        pp.FullScreen_RefreshRateInHz = 0;
        pp.SwapEffect = D3DSWAPEFFECT_DISCARD;
        pp.hDeviceWindow = hwnd;
        pp.Flags = D3DPRESENTFLAG_DISCARD_DEPTHSTENCIL;
        pp.EnableAutoDepthStencil = TRUE;
        pp.PresentationInterval = FrontEndMenuManager.m_bPrefsVsync
            ? D3DPRESENT_INTERVAL_ONE
            : D3DPRESENT_INTERVAL_IMMEDIATE;

        auto* d3d = *reinterpret_cast<IDirect3D9**>(kD3dObjectAddr);
        const UINT adapter = *reinterpret_cast<UINT*>(kAdapterIndexAddr);
        D3DDISPLAYMODE dm{};
        if (d3d && SUCCEEDED(d3d->GetAdapterDisplayMode(adapter, &dm)))
        {
            // X8 — DWM does not pay for alpha compose. 16-bit list entry → R5G6B5.
            pp.BackBufferFormat = s_forceDepth >= 32 ? D3DFMT_X8R8G8B8 : D3DFMT_R5G6B5;
            if (FAILED(d3d->CheckDeviceType(adapter, D3DDEVTYPE_HAL, dm.Format, pp.BackBufferFormat, TRUE)))
                pp.BackBufferFormat = dm.Format;
            D3DFORMAT ds = pp.AutoDepthStencilFormat;
            if (ds == D3DFMT_UNKNOWN)
                ds = D3DFMT_D24S8;
            if (FAILED(d3d->CheckDepthStencilMatch(adapter, D3DDEVTYPE_HAL, dm.Format, pp.BackBufferFormat, ds)))
                ds = D3DFMT_D24S8;
            pp.AutoDepthStencilFormat = ds;
        }
        return true;
    }

    bool FillExclusivePresent(D3DPRESENT_PARAMETERS& pp, HWND hwnd, int w, int h)
    {
        if (w < kMinW || h < kMinH)
            return false;
        pp.Windowed = FALSE;
        pp.BackBufferWidth = static_cast<UINT>(w);
        pp.BackBufferHeight = static_cast<UINT>(h);
        pp.BackBufferCount = 1;
        pp.FullScreen_RefreshRateInHz = D3DPRESENT_RATE_DEFAULT;
        pp.SwapEffect = D3DSWAPEFFECT_FLIP;
        pp.hDeviceWindow = hwnd;
        pp.EnableAutoDepthStencil = TRUE;
        pp.BackBufferFormat = s_forceDepth >= 32 ? D3DFMT_X8R8G8B8 : D3DFMT_R5G6B5;
        pp.AutoDepthStencilFormat = s_forceDepth >= 32 ? D3DFMT_D24S8 : D3DFMT_D16;
        pp.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;
        return true;
    }

    void RestoreRwAfterReset()
    {
        auto* dev = WindowMode::Device();
        if (!dev)
            return;

        IDirect3DSurface9* rt = nullptr;
        IDirect3DSurface9* ds = nullptr;
        if (SUCCEEDED(dev->GetRenderTarget(0, &rt)) && rt)
        {
            *reinterpret_cast<IDirect3DSurface9**>(kRtSurface) = rt;
            rt->Release();
        }
        if (SUCCEEDED(dev->GetDepthStencilSurface(&ds)) && ds)
        {
            *reinterpret_cast<IDirect3DSurface9**>(kDsSurface) = ds;
            ds->Release();
        }

        plugin::CallAndReturn<RwBool, kRasterRestore>();
        plugin::CallAndReturn<RwBool, kDynVbRestore>();
        plugin::Call<kRenderStateReset>();
        plugin::CallAndReturn<RwBool, kIm2DOpen>();
        plugin::CallAndReturn<RwBool, kIm3DOpen>();

        auto cb = *reinterpret_cast<void (**)()>(kRestoreCallback);
        if (cb)
            cb();
    }

    HRESULT Coop()
    {
        auto* dev = WindowMode::Device();
        if (!dev)
            return E_FAIL;
        return dev->TestCooperativeLevel();
    }

    bool BackBufferSize(int& outW, int& outH)
    {
        auto* dev = WindowMode::Device();
        if (!dev)
            return false;
        IDirect3DSurface9* bb = nullptr;
        if (FAILED(dev->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &bb)) || !bb)
            return false;
        D3DSURFACE_DESC desc{};
        bb->GetDesc(&desc);
        bb->Release();
        if (desc.Width < static_cast<UINT>(kMinW) || desc.Height < static_cast<UINT>(kMinH))
            return false;
        outW = static_cast<int>(desc.Width);
        outH = static_cast<int>(desc.Height);
        return true;
    }

    bool ResetDevice(const D3DPRESENT_PARAMETERS& pp, bool releaseVram)
    {
        if (pp.BackBufferWidth < static_cast<UINT>(kMinW)
            || pp.BackBufferHeight < static_cast<UINT>(kMinH))
            return false;

        EndActiveScene();
        const HRESULT coop = Coop();
        if (coop == D3DERR_DEVICELOST)
            return false;

        if (s_deviceLost)
            s_deviceLost();

        const bool lost = (coop == D3DERR_DEVICENOTRESET);
        if (releaseVram && !lost)
            plugin::CallAndReturn<RwBool, kReleaseVram>();

        PresentParams() = pp;
        PreReset(&PresentParams());
        auto* dev = WindowMode::Device();
        if (!dev)
            return false;

        s_ownReset = true;
        HRESULT hr = dev->Reset(&PresentParams());
        if (FAILED(hr) && PresentParams().Windowed)
        {
            auto* d3d = *reinterpret_cast<IDirect3D9**>(kD3dObjectAddr);
            const UINT adapter = *reinterpret_cast<UINT*>(kAdapterIndexAddr);
            D3DDISPLAYMODE dm{};
            if (d3d && SUCCEEDED(d3d->GetAdapterDisplayMode(adapter, &dm)))
            {
                PresentParams().BackBufferFormat = dm.Format;
                hr = dev->Reset(&PresentParams());
            }
        }

        if (FAILED(hr))
        {
            s_ownReset = false;
            if (s_good.valid)
                PresentParams() = s_good.pp;
            return false;
        }

        const int nw = (s_forceW >= static_cast<ULONG>(kMinW))
            ? static_cast<int>(s_forceW)
            : static_cast<int>(PresentParams().BackBufferWidth);
        const int nh = (s_forceH >= static_cast<ULONG>(kMinH))
            ? static_cast<int>(s_forceH)
            : static_cast<int>(PresentParams().BackBufferHeight);
        SyncRsGlobal(nw, nh);
        PokeCameraSize(nw, nh);
        FitWindowFrame(s_mode, nw, nh);
        RestoreRwAfterReset();
        s_ownReset = false;
        SyncCameraRasters(nw, nh);
        return Coop() == D3D_OK;
    }

    void CaptureInto(Snap& snap, int mode, int vid, int w, int h)
    {
        HWND hwnd = DeviceWindow();
        snap.valid = true;
        snap.windowMode = mode;
        snap.videoMode = vid;
        snap.w = w;
        snap.h = h;
        snap.depth = ModeDepth(vid);
        if (RadarConfig::HasColorDepth())
            snap.depth = RadarConfig::GetColorDepth();
        if (snap.depth < 16)
            snap.depth = 32;
        snap.pp = PresentParams();
        snap.style = hwnd ? GetWindowLong(hwnd, GWL_STYLE) : 0;
        snap.exStyle = hwnd ? GetWindowLong(hwnd, GWL_EXSTYLE) : 0;
    }

    bool CaptureFromDevice()
    {
        HWND hwnd = DeviceWindow();
        if (!hwnd || !WindowMode::Device())
            return false;
        if (Coop() != D3D_OK)
            return false;

        int bbW = 0, bbH = 0;
        if (!BackBufferSize(bbW, bbH))
            return false;

        int vid = VideoModeIndex();
        int mw = 0, mh = 0;
        if (!ModeSize(vid, mw, mh))
        {
            vid = RwEngineGetCurrentVideoMode();
            if (!ModeSize(vid, mw, mh))
            {
                mw = bbW;
                mh = bbH;
            }
        }

        CaptureInto(s_good, DetectMode(hwnd), vid, bbW, bbH);
        s_mode = s_good.windowMode;
        s_forceW = static_cast<ULONG>(bbW);
        s_forceH = static_cast<ULONG>(bbH);
        return s_good.valid;
    }

    struct BoundSlot
    {
        int  idx = -1;
        UINT origW = 0;
        UINT origH = 0;
        bool valid = false;
    };

    BoundSlot s_boundSel{};
    BoundSlot s_boundCur{};

    struct ModeSlot
    {
        UINT width;
        UINT height;
        UINT refresh;
        UINT format;
        int  flags;
    };

    ModeSlot* DisplayModes(int& n)
    {
        n = *reinterpret_cast<int*>(kNumDisplayModes);
        return *reinterpret_cast<ModeSlot**>(kDisplayModesPtr);
    }

    void RestoreBoundSlot(BoundSlot& slot)
    {
        int n = 0;
        ModeSlot* modes = DisplayModes(n);
        if (!slot.valid || !modes || slot.idx < 0 || (n > 0 && slot.idx >= n))
        {
            slot.valid = false;
            return;
        }
        modes[slot.idx].width = slot.origW;
        modes[slot.idx].height = slot.origH;
        slot.valid = false;
        slot.idx = -1;
    }

    void RestoreBoundModes()
    {
        RestoreBoundSlot(s_boundSel);
        RestoreBoundSlot(s_boundCur);
    }

    void BindSlot(BoundSlot& slot, int idx, int w, int h)
    {
        int n = 0;
        ModeSlot* modes = DisplayModes(n);
        if (!modes || idx < 0 || (n > 0 && idx >= n))
            return;

        if (slot.valid && slot.idx != idx)
            RestoreBoundSlot(slot);
        if (!slot.valid)
        {
            slot.origW = modes[idx].width;
            slot.origH = modes[idx].height;
            slot.idx = idx;
            slot.valid = true;
        }
        modes[idx].width = static_cast<UINT>(w);
        modes[idx].height = static_cast<UINT>(h);
    }

    // CameraSize (0x72FC70) uses DisplayModes[RwEngineGetCurrentVideoMode()],
    // not GcurSelVM. Patch both; restore before the next apply so 2K stays in the list.
    void BindEngineVideoMode(int vid, int w, int h)
    {
        *reinterpret_cast<int*>(kCurSelVideoMode) = vid;
        BindSlot(s_boundSel, vid, w, h);
        const int cur = RwEngineGetCurrentVideoMode();
        if (cur != vid)
            BindSlot(s_boundCur, cur, w, h);
    }

    void SyncGameFlags(int mode, int vid, int w, int h)
    {
        FrontEndMenuManager.m_nPrefsVideoMode = vid;
        FrontEndMenuManager.m_nDisplayVideoMode = vid;
        FrontEndMenuManager.m_bChangeVideoMode = false;
        if (RsGlobal.ps)
            RsGlobal.ps->fullScreen = (mode == 1) ? TRUE : FALSE;
        *reinterpret_cast<bool*>(kWindowedFlagAddr) = (mode != 1);
        BindEngineVideoMode(vid, w, h);
        SyncRsGlobal(w, h);
        {
            int depth = ModeDepth(vid);
            if (RadarConfig::HasColorDepth())
                depth = RadarConfig::GetColorDepth();
            if (depth != 16 && depth != 32)
                depth = 32;
            RadarConfig::SetWindowResolution(w, h, depth);
        }
    }

    bool Rollback()
    {
        if (!s_good.valid)
            return false;
        s_mode = s_good.windowMode;
        s_forceW = static_cast<ULONG>(s_good.w);
        s_forceH = static_cast<ULONG>(s_good.h);
        s_forceDepth = static_cast<ULONG>(s_good.depth >= 16 ? s_good.depth : 32);
        if (s_good.depth == 16 || s_good.depth == 32)
            RadarConfig::SetColorDepth(s_good.depth);
        if (!ResetDevice(s_good.pp, true))
            return false;
        SyncGameFlags(s_good.windowMode, s_good.videoMode, s_good.w, s_good.h);
        int bbW = 0, bbH = 0;
        return Coop() == D3D_OK && BackBufferSize(bbW, bbH);
    }

    bool ApplyNow(int mode, int vid)
    {
        if (mode < 0) mode = 0;
        if (mode > 2) mode = 2;

        HWND hwnd = DeviceWindow();
        auto* dev = WindowMode::Device();
        if (!hwnd || !dev)
            return false;

        int w = 0, h = 0;
        RestoreBoundModes();
        if (!ModeSize(vid, w, h))
            return false;

        if (!s_good.valid)
            CaptureFromDevice();

        const HRESULT coop = Coop();
        if (coop == D3DERR_DEVICELOST)
            return false;
        if (coop == D3DERR_DEVICENOTRESET)
        {
            if (!s_good.valid)
                return false;
            s_mode = s_good.windowMode;
            s_forceW = static_cast<ULONG>(s_good.w);
            s_forceH = static_cast<ULONG>(s_good.h);
            if (!ResetDevice(s_good.pp, false))
                return false;
        }

        const int listedDepth = ModeDepth(vid);
        const int depth = (RadarConfig::HasColorDepth()
            ? RadarConfig::GetColorDepth()
            : (listedDepth >= 16 ? listedDepth : 32));
        if (s_good.valid && s_good.windowMode == mode && s_good.videoMode == vid
            && s_good.w == w && s_good.h == h
            && s_good.depth == depth
            && coop == D3D_OK
            && PresentParams().Windowed == (mode != 1 ? TRUE : FALSE))
        {
            s_mode = mode;
            SyncGameFlags(mode, vid, w, h);
            SyncCameraRasters(w, h);
            FitWindowFrame(mode, w, h);
            return true;
        }

        s_mode = mode;
        s_forceW = static_cast<ULONG>(w);
        s_forceH = static_cast<ULONG>(h);
        s_forceDepth = static_cast<ULONG>(depth >= 16 ? depth : 32);
        BindEngineVideoMode(vid, w, h);

        D3DPRESENT_PARAMETERS pp = s_good.valid ? s_good.pp : PresentParams();
        if (mode == 1)
        {
            if (!FillExclusivePresent(pp, hwnd, w, h))
                return false;
        }
        else
        {
            PrepareWindowForReset(hwnd, mode, w, h);
            if (!FillWindowedPresent(pp, hwnd, w, h))
                return false;
        }

        if (!ResetDevice(pp, true))
        {
            Rollback();
            return false;
        }

        int bbW = 0, bbH = 0;
        if (!BackBufferSize(bbW, bbH))
        {
            Rollback();
            return false;
        }
        (void)bbW;
        (void)bbH;

        SyncAdapterMode(w, h);
        SyncCameraRasters(w, h);
        SyncGameFlags(mode, vid, w, h);
        FitWindowFrame(mode, w, h);
        CaptureInto(s_good, mode, vid, w, h);
        return true;
    }

    void EnsureWindowChrome()
    {
        if (s_mode != 0 || PresentParams().Windowed == FALSE)
            return;
        HWND hwnd = DeviceWindow();
        if (!hwnd)
            return;
        if ((GetWindowLong(hwnd, GWL_STYLE) & WS_CAPTION) != 0)
            return;

        int w = 0, h = 0;
        if (!TargetSize(w, h))
            return;
        FitWindowFrame(0, w, h);
    }

    void __cdecl SetVideoMode_Detour(int mode)
    {
        if (s_busy)
            return;
        int wm = s_mode;
        if (RadarConfig::HasWindowModeOverride())
            wm = RadarConfig::GetWindowMode();
        WindowMode::Request(wm, mode);
        if (!*reinterpret_cast<RwBool*>(kInsideSceneAddr))
            WindowMode::Flush();
    }

    bool GameHasFocus()
    {
        HWND hwnd = DeviceWindow();
        return hwnd && GetForegroundWindow() == hwnd;
    }

    // Stock reads rwVIDEOMODEEXCLUSIVE from the mode list — still true while we
    // force D3D windowed. CPad then diMouseInit(exclusive) and grabs the cursor.
    // While unfocused: lie (windowed) so Pad won't re-acquire exclusive and hide the OS cursor.
    bool __cdecl IsVideoModeExclusive_Detour()
    {
        if (!GameHasFocus())
            return false;
        return WindowMode::Query() == 1;
    }

    void ReleaseOsMouse()
    {
        ClipCursor(nullptr);
        SetCursor(LoadCursor(nullptr, IDC_ARROW));
        while (ShowCursor(TRUE) < 0)
        {
        }
        plugin::Call<kDIReleaseMouse>();
    }

    void AcquireGameMouse()
    {
        plugin::Call<kDIReleaseMouse>();
        plugin::Call<kDiMouseInit, bool>(WindowMode::Query() == 1);
    }
}

IDirect3DDevice9* WindowMode::Device()
{
    return static_cast<IDirect3DDevice9*>(RwD3D9GetCurrentD3DDevice());
}

int WindowMode::Query()
{
    return DetectMode(DeviceWindow());
}

void WindowMode::Install()
{
    if (!s_setVideoMode)
        s_setVideoMode = safetyhook::create_inline(reinterpret_cast<void*>(kSetVideoModeAddr), SetVideoMode_Detour);
    plugin::patch::ReplaceFunction(kIsExclusiveAddr, IsVideoModeExclusive_Detour);
}

void WindowMode::OnLostFocus()
{
    ReleaseOsMouse();
}

void WindowMode::OnGotFocus()
{
    if (FrontEndMenuManager.m_bGameNotLoaded || FrontEndMenuManager.m_bMenuActive)
        return;
    AcquireGameMouse();
}

int WindowMode::FindVideoMode(int width, int height)
{
    const int depth = RadarConfig::HasColorDepth() ? RadarConfig::GetColorDepth() : 0;
    return FindListedVideoMode(width, height, depth);
}

void WindowMode::SyncMenuFromConfig()
{
    if (!RadarConfig::HasWindowResolution())
        return;
    const int depth = RadarConfig::HasColorDepth() ? RadarConfig::GetColorDepth() : 0;
    const int found = FindListedVideoMode(
        RadarConfig::GetWindowWidth(), RadarConfig::GetWindowHeight(), depth);
    if (found < 0)
        return;
    FrontEndMenuManager.m_nPrefsVideoMode = found;
    FrontEndMenuManager.m_nDisplayVideoMode = found;
}

void WindowMode::Init()
{
    CaptureFromDevice();
    SyncMenuFromConfig();

    const bool hasMode = RadarConfig::HasWindowModeOverride();
    const bool hasRes = RadarConfig::HasWindowResolution();
    int vid = VideoModeIndex();
    if (hasRes)
    {
        const int found = FindListedVideoMode(
            RadarConfig::GetWindowWidth(), RadarConfig::GetWindowHeight(),
            RadarConfig::HasColorDepth() ? RadarConfig::GetColorDepth() : 0);
        if (found >= 0)
            vid = found;
    }
    const int wm = hasMode ? RadarConfig::GetWindowMode() : Query();

    if (hasMode || hasRes)
    {
        Request(wm, vid);
        Flush();
        FrontEndMenuManager.m_bChangeVideoMode = false;
    }

    if (s_good.valid)
        UpdateWindowTitle(s_good.windowMode, s_good.w, s_good.h);
}

void WindowMode::SetGraphicsFlush(void (*fn)())
{
    s_graphicsFlush = fn;
}

void WindowMode::SetDeviceLost(void (*fn)())
{
    s_deviceLost = fn;
}

void WindowMode::Request(int windowMode, int videoModeIndex)
{
    if (windowMode < 0) windowMode = 0;
    if (windowMode > 2) windowMode = 2;
    s_reqMode = windowMode;
    s_reqVid = videoModeIndex;
    s_pending = true;
}

void WindowMode::Flush()
{
    if (s_busy || !s_pending)
        return;

    const int mode = s_reqMode;
    const int vid = s_reqVid;
    s_pending = false;

    s_busy = true;
    const bool ok = ApplyNow(mode, vid);
    s_busy = false;

    if (!ok && Coop() == D3DERR_DEVICELOST)
    {
        s_reqMode = mode;
        s_reqVid = vid;
        s_pending = true;
    }
}

void WindowMode::Pump()
{
    if (s_graphicsFlush)
        s_graphicsFlush();
    Flush();
}

void WindowMode::OnDeviceReset()
{
    if (s_busy || s_ownReset)
        return;

    int w = 0, h = 0;
    if (!TargetSize(w, h))
        return;

    int mode = s_mode;
    if (RadarConfig::HasWindowModeOverride())
        mode = RadarConfig::GetWindowMode();
    s_mode = mode;
    FitWindowFrame(mode, w, h);
}

void WindowMode::TickChrome()
{
    EnsureWindowChrome();
}

void WindowMode::SyncRsFromBackbuffer()
{
    int w = 0, h = 0;
    if (TargetSize(w, h))
    {
        if (RsGlobal.maximumWidth != w || RsGlobal.maximumHeight != h)
            SyncRsGlobal(w, h);
        PokeCameraSize(w, h);
        return;
    }
    if (!BackBufferSize(w, h))
        return;
    if (RsGlobal.maximumWidth != w || RsGlobal.maximumHeight != h)
        SyncRsGlobal(w, h);
    PokeCameraSize(w, h);
}
