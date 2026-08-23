/*****************************************************************************
 *
 *  PROJECT:     The-Definitive-UI
 *  FILE:        source/Main.cpp
 *  PURPOSE:     Plugin entry point - owns subsystems and installs game hooks
 *
 *****************************************************************************/

#include "plugin.h"
#include "RenderWare.h"

#include "CrashDump.h"
#include "Config.h"
#include "Shader.h"
#include "HookManager.h"
#include "LanguageManager.h"
#include "Draw.h"
#include "InputManager.h"
#include "TxdManager.h"
#include "Radar.h"
#include "GpsRender.h"
#include "MainMenu.h"
#include "pMainMenu.h"
#include "GameSettings.h"
#include "WindowMode.h"
#include "Help.h"

class TheDefinitiveUI
{
public:
    TheDefinitiveUI()
    {
        s_self = this;
        CrashDump::Install();
        WindowMode::Install();
        WindowMode::SetGraphicsFlush([]() {
            if (s_self)
                s_self->m_mainMenu.FlushPendingVideoMode();
        });
        WindowMode::SetDeviceLost([]() {
            if (s_self)
                s_self->OnDeviceLost();
        });

        plugin::Events::initRwEvent += [this]() {
            RadarConfig::Load();
            GpsRenderer::SetPathfindingPatchesEnabled(RadarConfig::GetGps());
            LanguageManager::ApplySavedLanguage();

            if (RadarConfig::GetMenuRender())
                m_hookManager.Install(&DrawMainThunk, &DrawPauseThunk);
            if (RadarConfig::GetRadarRender())
            {
                m_hookManager.InstallRadar(&DrawRadarThunk);
                m_hookManager.InstallRadio(&DrawRadioThunk);
            }
            m_hookManager.InstallHelp(&DrawHelpThunk);

            InitShared();
            if (RadarConfig::GetRadarRender())
                EnsureRadarGpu();
            EnsureHelpGpu();
            WindowMode::Init();
        };
        plugin::Events::shutdownRwEvent += [this]() { Shutdown(); };
        plugin::Events::d3dLostEvent += [this]() { OnDeviceLost(); };
        plugin::Events::d3dResetEvent += [this]() {
            InitShared();
            if (RadarConfig::GetMenuRender())
                EnsureMenuGpu();
            if (RadarConfig::GetRadarRender())
                EnsureRadarGpu();
            EnsureHelpGpu();
            WindowMode::OnDeviceReset();
        };
        plugin::Events::gameProcessEvent += []() {
            WindowMode::Pump();
        };
    }

    ~TheDefinitiveUI()
    {
        Shutdown();
        if (s_self == this)
            s_self = nullptr;
    }

private:
    static TheDefinitiveUI* s_self;

    static void DrawMainImpl()
    {
        if (s_self)
            s_self->OnMainMenuDraw();
    }

    static void DrawPauseImpl()
    {
        if (s_self)
            s_self->OnPauseMenuDraw();
    }

    static void DrawRadarImpl()
    {
        if (!s_self)
            return;
        if (s_self->m_hookManager.IsCustomMainMenuSession()
            || s_self->m_hookManager.IsCustomPauseSession())
            return;
        if (!s_self->EnsureRadarGpu())
            return;
        s_self->m_radar.Render();
    }

    static void DrawRadioImpl()
    {
        if (!s_self)
            return;
        if (s_self->m_hookManager.IsCustomMainMenuSession()
            || s_self->m_hookManager.IsCustomPauseSession())
            return;
        s_self->m_radar.MarkRadioNameVisible();
    }

    static void DrawMainThunk()
    {
        __try { DrawMainImpl(); }
        __except (CrashDump::Filter(GetExceptionInformation())) {}
    }

    static void DrawPauseThunk()
    {
        __try { DrawPauseImpl(); }
        __except (CrashDump::Filter(GetExceptionInformation())) {}
    }

    static void DrawRadarThunk()
    {
        __try { DrawRadarImpl(); }
        __except (CrashDump::Filter(GetExceptionInformation())) {}
    }

    static void DrawRadioThunk()
    {
        __try { DrawRadioImpl(); }
        __except (CrashDump::Filter(GetExceptionInformation())) {}
    }

    static void DrawHelpImpl()
    {
        if (!s_self)
            return;
        if (s_self->m_hookManager.IsCustomMainMenuSession()
            || s_self->m_hookManager.IsCustomPauseSession())
            return;
        if (!s_self->EnsureHelpGpu())
            return;
        s_self->m_help.Render();
    }

    static void DrawHelpThunk()
    {
        __try { DrawHelpImpl(); }
        __except (CrashDump::Filter(GetExceptionInformation())) {}
    }

    static LPDIRECT3DDEVICE9 Device()
    {
        return reinterpret_cast<LPDIRECT3DDEVICE9>(RwD3D9GetCurrentD3DDevice());
    }

    void InitShared()
    {
        auto* device = Device();
        if (!device)
            return;

        m_inputManager.Initialize(device);
    }

    bool EnsureMenuGpu()
    {
        auto* device = Device();
        if (!device)
            return false;

        if (!m_draw.IsInitialized() && !m_draw.Initialize(device))
            return false;

        if (!m_shader.IsReady() && !m_shader.Initialize(device))
            return false;

        if (!m_txdManager.IsInitialized())
            m_txdManager.Initialize(device);

        if (!m_mainMenu.IsInitialized())
            m_mainMenu.Initialize(device, &m_draw, &m_txdManager, &m_hookManager, &m_gameSettings);

        if (!m_pMainMenu.IsInitialized())
            m_pMainMenu.Initialize(device, &m_draw, &m_txdManager, &m_hookManager, &m_mainMenu, &m_shader, &m_gameSettings);

        return m_mainMenu.IsInitialized() && m_pMainMenu.IsInitialized();
    }

    bool EnsureRadarGpu()
    {
        auto* device = Device();
        if (!device)
            return false;
        if (!m_radar.IsInitialized() && !m_radar.Initialize(device))
            return false;
        m_pMainMenu.SetMapChunkManager(m_radar.GetMapChunkManager());
        return true;
    }

    bool EnsureHelpGpu()
    {
        auto* device = Device();
        if (!device)
            return false;

        if (!m_draw.IsInitialized() && !m_draw.Initialize(device))
            return false;

        if (!m_help.IsInitialized() && !m_help.Initialize(device, &m_draw))
            return false;

        return m_help.IsInitialized();
    }

    void OnDeviceLost()
    {
        m_pMainMenu.OnDeviceLost();
        m_mainMenu.OnDeviceLost();
        m_shader.OnDeviceLost();
        m_help.Shutdown();
        m_radar.Shutdown();
        m_txdManager.ReleaseCachedTextures();
        m_draw.Shutdown();
    }

    void ReleaseMenuGpu()
    {
        if (!m_mainMenu.IsInitialized() && !m_pMainMenu.IsInitialized()
            && !m_draw.IsInitialized() && !m_txdManager.IsTxdLoaded())
            return;

        m_pMainMenu.Shutdown();
        m_mainMenu.Shutdown();
        m_help.Shutdown();
        m_radar.Shutdown();
        m_txdManager.UnloadTxd();
        m_shader.Shutdown();
        m_draw.Shutdown();
    }

    void Shutdown()
    {
        m_pMainMenu.Shutdown();
        m_mainMenu.Shutdown();
        m_help.Shutdown();
        m_radar.Shutdown();
        m_inputManager.Shutdown();
        m_txdManager.Shutdown();
        m_shader.Shutdown();
        m_draw.Shutdown();
    }

    void OnMainMenuDraw()
    {
        if (!m_hookManager.IsCustomMainMenuSession())
            return;

        if (!EnsureMenuGpu())
            return;

        m_mainMenu.Render();
    }

    void OnPauseMenuDraw()
    {
        if (m_hookManager.IsCustomMainMenuSession())
            return;

        // ESC closes MenuActive in Process before Draw — still need Render to drain UI
        if (m_hookManager.IsCustomPauseSession())
        {
            if (!EnsureMenuGpu())
                return;
            // Warm pause map from HUD tiles (no second TXD decode).
            EnsureRadarGpu();
        }

        m_pMainMenu.Render();
    }

    Config          m_config;
    Shader          m_shader;
    HookManager     m_hookManager;
    LanguageManager m_languageManager;
    Draw            m_draw;
    InputManager    m_inputManager;
    TxdManager      m_txdManager;
    Radar           m_radar;
    Help            m_help;
    GameSettings    m_gameSettings;
    MainMenu        m_mainMenu;
    pMainMenu       m_pMainMenu;
} gTheDefinitiveUI;

TheDefinitiveUI* TheDefinitiveUI::s_self = nullptr;
