/*****************************************************************************
 *
 *  PROJECT:     The-Definitive-UI
 *  FILE:        source/pMainMenu/pMainMenu.cpp
 *  PURPOSE:     Process pause menu — Continue + shared MainMenu panels
 *
 *****************************************************************************/

#include "pMainMenu.h"
#include "Config.h"
#include "GpsRender.h"
#include "MapChunkManager.h"
#include "MainMenu.h"

#include "plugin.h"
#include "RenderWare.h"
#include "Draw/Draw.h"
#include "TxdManager.h"
#include "HookManager.h"
#include "GameSettings.h"
#include "InputManager.h"
#include "Shader.h"
#include "CMenuManager.h"
#include "CRadar.h"
#include "CSprite2d.h"
#include "BlipManager.h"
#include "StockRadarDraw.h"
#include "CPad.h"
#include "CTimer.h"
#include "CTheZones.h"
#include "CZone.h"
#include "CAudioEngine.h"
#include "eAudioEvents.h"
#include "common.h"
#include "LanguageManager/LanguageManager.h"

#include <windows.h>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <cstdint>
#include <string>

namespace
{
    const char* ButtonKeys[pMainMenu::Layout::Count] = {
        "UI_CONTINUE",
        "UI_MAP",
        "UI_GAME",
        "UI_MESSAGES",
        "UI_STATS",
        "UI_SETTINGS",
        "UI_EXIT"
    };

    const char* HoverTexNames[pMainMenu::Layout::Count] = {
        pMainMenu::Tex::Start,
        pMainMenu::Tex::Game,
        pMainMenu::Tex::Game,
        pMainMenu::Tex::Settings,
        pMainMenu::Tex::Settings,
        pMainMenu::Tex::Settings,
        pMainMenu::Tex::Exit
    };

    void PlayFe(int ev)
    {
        AudioEngine.ReportFrontendAudioEvent(ev, 0.0f, 1.0f);
    }

    constexpr float kWorldBound = 3000.0f;
    constexpr uintptr_t kRwD3D9RasterExtOffset = 0xB4E9E0;

    LPDIRECT3DTEXTURE9 NativeRwTexture(RwTexture* rwTex)
    {
        if (!rwTex)
            return nullptr;
        RwRaster* raster = RwTextureGetRaster(rwTex);
        if (!raster)
            return nullptr;
        const int extOff = *reinterpret_cast<int*>(kRwD3D9RasterExtOffset);
        if (extOff <= 0)
            return nullptr;
        auto* ext = reinterpret_cast<unsigned char*>(raster) + extOff;
        return *reinterpret_cast<LPDIRECT3DTEXTURE9*>(ext);
    }

    const char* LegendGxtKey(int sprite)
    {
        switch (sprite)
        {
        case -5: return "LG_56"; // player interest
        case -4: return "LG_55"; // threat
        case -3: return "LG_54"; // friend
        case -2: return "LG_50"; // object
        case -1: return "LG_49"; // destination
        case RADAR_SPRITE_MAP_HERE:      return "LG_01";
        case RADAR_SPRITE_AIRYARD:       return "LG_02";
        case RADAR_SPRITE_AMMUGUN:       return "LG_03";
        case RADAR_SPRITE_BARBERS:       return "LG_04";
        case RADAR_SPRITE_BIGSMOKE:      return "LG_05";
        case RADAR_SPRITE_BOATYARD:      return "LG_06";
        case RADAR_SPRITE_BURGERSHOT:    return "LG_07";
        case RADAR_SPRITE_BULLDOZER:     return "LG_66";
        case RADAR_SPRITE_CATALINAPINK:  return "LG_09";
        case RADAR_SPRITE_CESARVIAPANDO: return "LG_10";
        case RADAR_SPRITE_CHICKEN:       return "LG_11";
        case RADAR_SPRITE_CJ:            return "LG_12";
        case RADAR_SPRITE_CRASH1:        return "LG_13";
        case RADAR_SPRITE_DINER:         return "LG_67";
        case RADAR_SPRITE_EMMETGUN:      return "LG_15";
        case RADAR_SPRITE_ENEMYATTACK:   return "LG_16";
        case RADAR_SPRITE_FIRE:          return "LG_17";
        case RADAR_SPRITE_GIRLFRIEND:    return "LG_18";
        case RADAR_SPRITE_HOSTPITAL:     return "LG_19";
        case RADAR_SPRITE_LOGOSYNDICATE: return "LG_20";
        case RADAR_SPRITE_MADDOG:        return "LG_21";
        case RADAR_SPRITE_MAFIACASINO:   return "LG_22";
        case RADAR_SPRITE_MCSTRAP:       return "LG_23";
        case RADAR_SPRITE_MODGARAGE:     return "LG_24";
        case RADAR_SPRITE_OGLOC:         return "LG_25";
        case RADAR_SPRITE_PIZZA:         return "LG_26";
        case RADAR_SPRITE_POLICE:        return "LG_27";
        case RADAR_SPRITE_PROPERTYG:     return "LG_28";
        case RADAR_SPRITE_PROPERTYR:     return "LG_29";
        case RADAR_SPRITE_RACE:          return "LG_30";
        case RADAR_SPRITE_RYDER:         return "LG_31";
        case RADAR_SPRITE_SAVEGAME:      return "LG_32";
        case RADAR_SPRITE_SCHOOL:        return "LG_33";
        case RADAR_SPRITE_QMARK:         return "LG_63";
        case RADAR_SPRITE_SWEET:         return "LG_35";
        case RADAR_SPRITE_TATTOO:        return "LG_36";
        case RADAR_SPRITE_THETRUTH:      return "LG_37";
        case RADAR_SPRITE_WAYPOINT:      return "LG_64";
        case RADAR_SPRITE_TORENORANCH:   return "LG_39";
        case RADAR_SPRITE_TRIADS:        return "LG_40";
        case RADAR_SPRITE_TRIADSCASINO:  return "LG_41";
        case RADAR_SPRITE_TSHIRT:        return "LG_42";
        case RADAR_SPRITE_WOOZIE:        return "LG_43";
        case RADAR_SPRITE_ZERO:          return "LG_44";
        case RADAR_SPRITE_DATEDISCO:     return "LG_45";
        case RADAR_SPRITE_DATEDRINK:     return "LG_46";
        case RADAR_SPRITE_DATEFOOD:      return "LG_47";
        case RADAR_SPRITE_TRUCK:         return "LG_48";
        case RADAR_SPRITE_CASH:          return "LG_51";
        case RADAR_SPRITE_FLAG:          return "LG_52";
        case RADAR_SPRITE_GYM:           return "LG_53";
        case RADAR_SPRITE_IMPOUND:       return "LG_57";
        case RADAR_SPRITE_RUNWAY:        return "LG_65";
        case RADAR_SPRITE_GANGB:         return "LG_58";
        case RADAR_SPRITE_GANGP:         return "LG_59";
        case RADAR_SPRITE_GANGY:         return "LG_60";
        case RADAR_SPRITE_GANGN:         return "LG_61";
        case RADAR_SPRITE_GANGG:         return "LG_62";
        case RADAR_SPRITE_SPRAY:         return "LG_34";
        default: return nullptr;
        }
    }

    CVector PlayerCentreForMap()
    {
        return ((CVector(__cdecl*)(int))0x56E400)(0);
    }
}

pMainMenu::pMainMenu() = default;

pMainMenu::~pMainMenu()
{
    Shutdown();
}

bool pMainMenu::Initialize(LPDIRECT3DDEVICE9 pDevice, Draw* pDraw, TxdManager* pTxd,
                           HookManager* pHooks, MainMenu* pSharedMainMenu, Shader* pShader,
                           GameSettings* pSettings)
{
    if (m_bInitialized)
        return true;
    if (!pDevice || !pDraw || !pTxd || !pHooks || !pSharedMainMenu || !pSettings)
        return false;

    m_pDevice = pDevice;
    m_pDraw = pDraw;
    m_pTxd = pTxd;
    m_pHooks = pHooks;
    m_pMainMenu = pSharedMainMenu;
    m_pShader = pShader;
    m_pSettings = pSettings;
    m_pBackground = nullptr;

    if (!m_mapTxd.IsInitialized())
        m_mapTxd.Initialize(pDevice);

    StockRadarDraw::EnsureHooksInstalled();
    LoadBackground();
    m_bInitialized = true;
    return true;
}

void pMainMenu::Shutdown()
{
    HideOsCursor();

    m_pBackground = nullptr;
    m_pLogo = nullptr;
    for (int i = 0; i < Layout::Count; ++i)
        m_pHover[i] = nullptr;
    m_pExitBtnIdle[0] = m_pExitBtnIdle[1] = nullptr;
    m_pExitBtnHover[0] = m_pExitBtnHover[1] = nullptr;
    ReleaseMapTextures();
    m_mapTxd.Shutdown();

    m_nHovered = -1;
    m_nHoverSoundId = -1;
    m_nActive = -1;
    m_bLmbWasDown = false;
    m_bSwallowClick = false;
    m_bWasFocused = true;
    m_bPauseWasOpen = false;
    m_bExitConfirm = false;
    m_bOsCursorHeld = false;

    // Menu TXD owned by Main.cpp / MainMenu — do not unload here
    m_pDevice = nullptr;
    m_pDraw = nullptr;
    m_pTxd = nullptr;
    m_pHooks = nullptr;
    m_pMainMenu = nullptr;
    m_pSettings = nullptr;
    m_pShader = nullptr;
    m_pMapChunks = nullptr;
    m_bInitialized = false;
}

void pMainMenu::OnDeviceLost()
{
    m_pBackground = nullptr;
    m_pLogo = nullptr;
    for (int i = 0; i < Layout::Count; ++i)
        m_pHover[i] = nullptr;
    m_pExitBtnIdle[0] = m_pExitBtnIdle[1] = nullptr;
    m_pExitBtnHover[0] = m_pExitBtnHover[1] = nullptr;
    ReleaseMapTextures();
    m_mapTxd.Shutdown();

    m_bSwallowClick = false;
    m_bWasFocused = false;
    m_bExitConfirm = false;

    m_pDevice = nullptr;
    m_pDraw = nullptr;
    m_pTxd = nullptr;
    m_pShader = nullptr;
    m_pMapChunks = nullptr;
    m_bInitialized = false;
}

bool pMainMenu::LoadBackground()
{
    if (m_pBackground)
    {
        if (m_pTxd && m_pTxd->IsTxdLoaded() && !m_pLogo)
            m_pLogo = m_pTxd->GetTexture(Tex::Logo);
        return true;
    }

    if (!m_pTxd || !m_pTxd->IsInitialized())
        return false;

    if (!m_pTxd->IsTxdLoaded() && !m_pTxd->LoadTxd(PLUGIN_PATH(Path::MenuTxd)))
        return false;

    m_pBackground = m_pTxd->GetTexture(Tex::Background);
    if (!m_pLogo)
        m_pLogo = m_pTxd->GetTexture(Tex::Logo);
    LoadHoverTextures();
    return m_pBackground != nullptr;
}

bool pMainMenu::LoadHoverTextures()
{
    if (!m_pTxd || !m_pTxd->IsTxdLoaded())
        return false;

    bool ok = true;
    for (int i = 0; i < Layout::Count; ++i)
    {
        if (!m_pHover[i])
            m_pHover[i] = m_pTxd->GetTexture(HoverTexNames[i]);
        if (!m_pHover[i])
            ok = false;
    }
    if (!m_pExitBtnIdle[0])
        m_pExitBtnIdle[0] = m_pTxd->GetTexture(Tex::ExitNoHoverCancel);
    if (!m_pExitBtnHover[0])
        m_pExitBtnHover[0] = m_pTxd->GetTexture(Tex::ExitHoverCancel);
    if (!m_pExitBtnIdle[1])
        m_pExitBtnIdle[1] = m_pTxd->GetTexture(Tex::ExitNoHoverAccept);
    if (!m_pExitBtnHover[1])
        m_pExitBtnHover[1] = m_pTxd->GetTexture(Tex::ExitHoverAccept);
    return ok;
}

void pMainMenu::ReleaseMapTextures()
{
    for (int i = 0; i < Layout::MapTileCount; ++i)
        m_pRadar[i] = nullptr;
    for (int i = 0; i < Layout::MapTileCount; ++i)
    {
        if (m_pRadarStock[i])
        {
            m_pRadarStock[i]->Release();
            m_pRadarStock[i] = nullptr;
        }
    }
    m_bMapTxdReady = false;
    m_bStockRadarReady = false;
    m_nMapWarmIndex = 0;
    ReleaseFogMaskRT();
    ReleaseZonesTex();
    if (m_mapTxd.IsInitialized())
        m_mapTxd.UnloadTxd();
}

void pMainMenu::ReleaseFogMaskRT()
{
    if (m_pFogMaskSurf)
    {
        m_pFogMaskSurf->Release();
        m_pFogMaskSurf = nullptr;
    }
    if (m_pFogMaskTex)
    {
        m_pFogMaskTex->Release();
        m_pFogMaskTex = nullptr;
    }
    m_nFogMaskSize = 0;
}

bool pMainMenu::EnsureFogMaskRT()
{
    if (!m_pDevice)
        return false;

    const int size = Layout::MapFogCells * Layout::MapFogMaskPx;
    if (m_pFogMaskTex && m_nFogMaskSize == size)
        return true;

    ReleaseFogMaskRT();

    // Lockable CPU mask — no render-target / pixel-shader path (unstable under SA frontend state)
    if (FAILED(m_pDevice->CreateTexture(size, size, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED,
                                        &m_pFogMaskTex, nullptr))
        || !m_pFogMaskTex)
        return false;

    m_nFogMaskSize = size;
    return true;
}

bool pMainMenu::EnsureZonesTex()
{
    if (!m_pDevice)
        return false;
    if (m_pZonesTex)
        return true;

    const int n = Layout::MapFogCells;
    if (FAILED(m_pDevice->CreateTexture(n, n, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED,
                                        &m_pZonesTex, nullptr))
        || !m_pZonesTex)
        return false;
    return true;
}

void pMainMenu::ReleaseZonesTex()
{
    if (m_pZonesTex)
    {
        m_pZonesTex->Release();
        m_pZonesTex = nullptr;
    }
}

bool pMainMenu::UploadZonesTex()
{
    if (!EnsureZonesTex() || !m_pZonesTex || !CTheZones::ExploredTerritoriesArray)
        return false;

    D3DLOCKED_RECT lr{};
    if (FAILED(m_pZonesTex->LockRect(0, &lr, nullptr, 0)) || !lr.pBits)
        return false;

    const int n = Layout::MapFogCells;
    const char* visited = CTheZones::ExploredTerritoriesArray;
    for (int y = 0; y < n; ++y)
    {
        auto* row = reinterpret_cast<DWORD*>(static_cast<BYTE*>(lr.pBits) + y * lr.Pitch);
        for (int x = 0; x < n; ++x)
            row[x] = (visited[x * n + y] != 0) ? 0xFFFFFFFF : 0x00000000;
    }

    m_pZonesTex->UnlockRect(0);
    return true;
}

void pMainMenu::ResetMapView()
{
    m_bMapFullscreen = false;
    m_bMapLegend = false;
    m_fMapPanX = 0.0f;
    m_fMapPanY = 0.0f;
    m_fMapZoom = Layout::MapZoomDefault;
    m_fMapZoomTarget = Layout::MapZoomDefault;
    m_bMapZoomFocus = false;
    m_bMapPress = false;
    m_bMapDragging = false;
    m_bMapPressOpenedFs = false;
    m_bMapClickWorldValid = false;
    if (m_pHooks)
        m_pHooks->SetPauseMapFullscreen(false);
}

void pMainMenu::CenterMapOnPlayer(float screenW, float screenH)
{
    if (screenW < 1.0f || screenH < 1.0f)
        GetScreenSize(screenW, screenH);
    if (screenW < 1.0f || screenH < 1.0f)
        return;

    const MapLayout lay = ComputeMapLayout(screenW, screenH);
    if (lay.mapSize <= 1.0f)
        return;

    const CVector world = PlayerCentreForMap();
    // Match blip plane: px = mapCx + half*(wx/3000), py = mapCy - half*(wy/3000)
    m_fMapPanX = -0.5f * lay.mapSize * (world.x / kWorldBound);
    m_fMapPanY =  0.5f * lay.mapSize * (world.y / kWorldBound);
    ClampMapPan(lay.areaR - lay.areaL, lay.areaB - lay.areaT, lay.mapSize);
}

void pMainMenu::EnterMapFullscreen()
{
    if (m_bMapFullscreen)
        return;
    m_bMapFullscreen = true;
    m_fMapZoom = Layout::MapZoomDefault;
    m_fMapZoomTarget = Layout::MapZoomDefault;
    m_bMapZoomFocus = false;
    m_bMapPress = false;
    m_bMapDragging = false;
    m_bMapPressOpenedFs = false;
    if (m_pHooks)
        m_pHooks->SetPauseMapFullscreen(true);
    CenterMapOnPlayer();
    PlayFe(AE_FRONTEND_SELECT);
}

void pMainMenu::ExitMapFullscreen()
{
    if (!m_bMapFullscreen)
        return;
    m_bMapFullscreen = false;
    m_bMapLegend = false;
    m_fMapZoom = Layout::MapZoomDefault;
    m_fMapZoomTarget = Layout::MapZoomDefault;
    m_bMapZoomFocus = false;
    m_bMapPress = false;
    m_bMapDragging = false;
    m_bMapPressOpenedFs = false;
    m_bMapClickWorldValid = false;
    if (m_pHooks)
        m_pHooks->SetPauseMapFullscreen(false);
    CenterMapOnPlayer();
    PlayFe(AE_FRONTEND_BACK);
    ConsumeEscJustPressed();
}

pMainMenu::MapLayout pMainMenu::ComputeMapLayout(float screenW, float screenH) const
{
    MapLayout out{};
    if (m_bMapFullscreen)
    {
        out.areaL = 0.0f;
        out.areaT = 0.0f;
        out.areaR = screenW;
        out.areaB = screenH;
    }
    else
    {
        out.areaL = screenW * Layout::MapLeftFrac;
        out.areaT = 0.0f;
        out.areaR = screenW;
        out.areaB = screenH;
    }

    const float aw = out.areaR - out.areaL;
    const float ah = out.areaB - out.areaT;
    const float base = (aw < ah) ? aw : ah;
    const float zoom = (m_fMapZoom < 0.01f) ? 0.01f : m_fMapZoom;
    out.mapSize = base * zoom;
    out.mapL = out.areaL + (aw - out.mapSize) * 0.5f + m_fMapPanX;
    out.mapT = out.areaT + (ah - out.mapSize) * 0.5f + m_fMapPanY;
    return out;
}

void pMainMenu::ClampMapPan(float areaW, float areaH, float mapSize)
{
    // Stop when map edge (±3000) meets the view edge, with slight world overscroll
    // to ±MapPanWorldX / ±MapPanWorldY. Old formula used 0.5*mapSize and ignored the
    // viewport — that let the map slide until its edge sat at screen center (felt unlimited).
    if (mapSize < 1.0f || areaW < 1.0f || areaH < 1.0f)
        return;

    const float overscrollX = 0.5f * mapSize * ((Layout::MapPanWorldX - kWorldBound) / kWorldBound);
    const float overscrollY = 0.5f * mapSize * ((Layout::MapPanWorldY - kWorldBound) / kWorldBound);

    float maxPanX = 0.5f * (mapSize - areaW) + overscrollX;
    float maxPanY = 0.5f * (mapSize - areaH) + overscrollY;
    if (maxPanX < overscrollX)
        maxPanX = overscrollX;
    if (maxPanY < overscrollY)
        maxPanY = overscrollY;
    if (maxPanX < 0.0f)
        maxPanX = 0.0f;
    if (maxPanY < 0.0f)
        maxPanY = 0.0f;

    if (m_fMapPanX > maxPanX)
        m_fMapPanX = maxPanX;
    if (m_fMapPanX < -maxPanX)
        m_fMapPanX = -maxPanX;
    if (m_fMapPanY > maxPanY)
        m_fMapPanY = maxPanY;
    if (m_fMapPanY < -maxPanY)
        m_fMapPanY = -maxPanY;
}

bool pMainMenu::MapScreenToWorld(float screenX, float screenY, const MapLayout& lay, float& outX, float& outY) const
{
    if (lay.mapSize < 1.0f)
        return false;

    const float mapCx = lay.mapL + lay.mapSize * 0.5f;
    const float mapCy = lay.mapT + lay.mapSize * 0.5f;
    const float half = lay.mapSize * 0.5f;
    // Match blip plane: px = mapCx + half*(wx/3000), py = mapCy - half*(wy/3000)
    outX = (screenX - mapCx) / half * kWorldBound;
    outY = -(screenY - mapCy) / half * kWorldBound;
    return true;
}

void pMainMenu::ClearMapWaypoint()
{
    if (!FrontEndMenuManager.m_nTargetBlipIndex)
        return;
    CRadar::ClearBlip(FrontEndMenuManager.m_nTargetBlipIndex);
    FrontEndMenuManager.m_nTargetBlipIndex = 0;
}

void pMainMenu::PlaceMapWaypoint(float worldX, float worldY)
{
    ClearMapWaypoint();

    CVector pos(worldX, worldY, 0.0f);
    if (pos.x < -kWorldBound) pos.x = -kWorldBound;
    if (pos.x >  kWorldBound) pos.x =  kWorldBound;
    if (pos.y < -kWorldBound) pos.y = -kWorldBound;
    if (pos.y >  kWorldBound) pos.y =  kWorldBound;

    // Same path as stock / gta-reversed menu waypoint
    const int handle = CRadar::SetCoordBlip(BLIP_COORD, pos, 0, BLIP_DISPLAY_BOTH, nullptr);
    if (handle == -1)
        return;
    CRadar::SetBlipSprite(handle, RADAR_SPRITE_WAYPOINT);
    FrontEndMenuManager.m_nTargetBlipIndex = handle;
}

void pMainMenu::ApplyMapZoomFocus(float screenW, float screenH)
{
    if (!m_bMapZoomFocus)
        return;

    const MapLayout lay = ComputeMapLayout(screenW, screenH);
    const float aw = lay.areaR - lay.areaL;
    const float ah = lay.areaB - lay.areaT;
    if (lay.mapSize <= 1.0f)
        return;

    // Keep the anchored map point under the screen focus pixel
    const float centerL = lay.areaL + (aw - lay.mapSize) * 0.5f;
    const float centerT = lay.areaT + (ah - lay.mapSize) * 0.5f;
    m_fMapPanX = m_fMapZoomFocusSX - m_fMapZoomFocusRelX * lay.mapSize - centerL;
    m_fMapPanY = m_fMapZoomFocusSY - m_fMapZoomFocusRelY * lay.mapSize - centerT;
    ClampMapPan(aw, ah, lay.mapSize);
}

bool pMainMenu::MapTilesReady() const
{
    if (m_pMapChunks && m_pMapChunks->GetReadyTileCount() >= Layout::MapTileCount)
        return true;
    if (RadarConfig::GetCustomRadarTxd())
        return m_bMapTxdReady;
    return m_bStockRadarReady;
}

void pMainMenu::WarmMapResources()
{
    if (!m_pDevice)
        return;

    // Tiny 10×10 zones tex — do early so first fog draw does not hitch.
    EnsureZonesTex();

    if (m_pMapChunks)
    {
        if (RadarConfig::GetCustomRadarTxd())
        {
            if (m_pMapChunks->GetReadyTileCount() < Layout::MapTileCount)
                m_pMapChunks->LoadAllChunks();
        }
        else
            m_pMapChunks->EnsureStockChunks();
        return;
    }

    // No radar manager yet — spread TXD→D3D conversion across pause frames.
    WarmMapTexturesStep(24);
}

bool pMainMenu::WarmMapTexturesStep(int budget)
{
    if (budget < 1)
        budget = 1;

    if (RadarConfig::GetCustomRadarTxd())
    {
        if (m_bMapTxdReady)
            return true;
        if (!m_pDevice)
            return false;
        if (!m_mapTxd.IsInitialized() && !m_mapTxd.Initialize(m_pDevice))
            return false;
        if (!m_mapTxd.IsTxdLoaded() && !m_mapTxd.LoadTxd(PLUGIN_PATH(Path::MapTxd)))
            return false;

        int done = 0;
        while (m_nMapWarmIndex < Layout::MapTileCount && done < budget)
        {
            const int i = m_nMapWarmIndex++;
            char name[16]{};
            std::snprintf(name, sizeof(name), "radar%02d", i);
            m_pRadar[i] = m_mapTxd.GetTexture(name);
            ++done;
        }
        if (m_nMapWarmIndex >= Layout::MapTileCount)
        {
            int loaded = 0;
            for (int i = 0; i < Layout::MapTileCount; ++i)
            {
                if (m_pRadar[i])
                    ++loaded;
            }
            m_bMapTxdReady = loaded > 0;
        }
        return m_bMapTxdReady;
    }

    return EnsureStockRadarTextures();
}

bool pMainMenu::LoadMapTextures()
{
    if (m_bMapTxdReady)
        return true;

    // Prefer already-decoded HUD tiles.
    if (m_pMapChunks && m_pMapChunks->GetReadyTileCount() >= Layout::MapTileCount)
        return true;

    if (!m_pDevice)
        return false;

    if (!m_mapTxd.IsInitialized() && !m_mapTxd.Initialize(m_pDevice))
        return false;

    if (!m_mapTxd.IsTxdLoaded() && !m_mapTxd.LoadTxd(PLUGIN_PATH(Path::MapTxd)))
        return false;

    int loaded = 0;
    for (int i = 0; i < Layout::MapTileCount; ++i)
    {
        char name[16]{};
        std::snprintf(name, sizeof(name), "radar%02d", i);
        m_pRadar[i] = m_mapTxd.GetTexture(name);
        if (m_pRadar[i])
            ++loaded;
    }

    m_nMapWarmIndex = Layout::MapTileCount;
    m_bMapTxdReady = loaded > 0;
    return m_bMapTxdReady;
}

bool pMainMenu::EnsureStockRadarTextures()
{
    if (m_bStockRadarReady)
        return true;
    if (m_pMapChunks)
    {
        m_pMapChunks->EnsureStockChunks();
        if (m_pMapChunks->GetReadyTileCount() > 0)
        {
            m_bStockRadarReady = true;
            return true;
        }
    }
    if (!m_pDevice)
        return false;
    const int loaded = TxdManager::LoadStockRadarTiles(m_pDevice, m_pRadarStock, Layout::MapTileCount);
    m_bStockRadarReady = loaded > 0;
    return m_bStockRadarReady;
}

void pMainMenu::DrawMapPlane(float mapL, float mapT, float mapSize)
{
    if (!m_pDraw || mapSize <= 1.0f)
        return;

    const bool custom = RadarConfig::GetCustomRadarTxd();
    const bool useShared = m_pMapChunks && m_pMapChunks->GetReadyTileCount() > 0;

    if (!useShared)
    {
        if (custom)
        {
            if (!LoadMapTextures())
                return;
        }
        else if (!EnsureStockRadarTextures())
            return;
    }

    const float tileW = mapSize / static_cast<float>(Layout::MapTilesX);
    const float tileH = mapSize / static_cast<float>(Layout::MapTilesY);

    for (int y = 0; y < Layout::MapTilesY; ++y)
    {
        for (int x = 0; x < Layout::MapTilesX; ++x)
        {
            const int idx = y * Layout::MapTilesX + x;
            LPDIRECT3DTEXTURE9 tex = nullptr;
            if (useShared)
                tex = m_pMapChunks->GetChunk(idx);
            else if (custom)
                tex = m_pRadar[idx];
            else
                tex = m_pRadarStock[idx];
            if (!tex)
                continue;
            m_pDraw->DrawTexture(tex,
                                 mapL + static_cast<float>(x) * tileW,
                                 mapT + static_cast<float>(y) * tileH,
                                 tileW, tileH);
        }
    }
}

bool pMainMenu::BuildFogContourMask()
{
    // CPU mask: white = fog, black = clear. Same 1/3 corner rules as orange contour.
    if (!m_pDevice || !EnsureFogMaskRT() || !m_pFogMaskTex)
        return false;
    if (!CTheZones::ExploredTerritoriesArray)
        return false;

    const int size = m_nFogMaskSize;
    D3DLOCKED_RECT lr{};
    if (FAILED(m_pFogMaskTex->LockRect(0, &lr, nullptr, 0)) || !lr.pBits)
        return false;

    constexpr DWORD kFog = 0xFFFFFFFF;
    constexpr DWORD kClear = 0x00000000;

    auto rowPtr = [&](int y) -> DWORD*
    {
        return reinterpret_cast<DWORD*>(static_cast<BYTE*>(lr.pBits) + y * lr.Pitch);
    };

    auto fillRect = [&](int x0, int y0, int w, int h, DWORD c)
    {
        if (w <= 0 || h <= 0)
            return;
        int x1 = x0 + w;
        int y1 = y0 + h;
        if (x0 < 0) x0 = 0;
        if (y0 < 0) y0 = 0;
        if (x1 > size) x1 = size;
        if (y1 > size) y1 = size;
        for (int y = y0; y < y1; ++y)
        {
            DWORD* row = rowPtr(y);
            for (int x = x0; x < x1; ++x)
                row[x] = c;
        }
    };

    if (CTheZones::TotalNumberExploredTerritories >= Layout::MapFogRevealAll)
    {
        fillRect(0, 0, size, size, kClear);
        m_pFogMaskTex->UnlockRect(0);
        return true;
    }

    const char* visited = CTheZones::ExploredTerritoriesArray;
    const int n = Layout::MapFogCells;
    const int cellPx = Layout::MapFogMaskPx;
    float R = static_cast<float>(cellPx) * Layout::MapExploredCorner;
    if (R < 2.0f)
        R = 2.0f;
    if (R > static_cast<float>(cellPx) * 0.45f)
        R = static_cast<float>(cellPx) * 0.45f;

    auto isExplored = [visited, n](int x, int y) -> bool
    {
        if (x < 0 || y < 0 || x >= n || y >= n)
            return false;
        return visited[x * n + y] != 0;
    };

    fillRect(0, 0, size, size, kFog);

    for (int x = 0; x < n; ++x)
    {
        for (int y = 0; y < n; ++y)
        {
            if (!isExplored(x, y))
                continue;
            fillRect(x * cellPx, y * cellPx, cellPx, cellPx, kClear);
        }
    }

    for (int vx = 0; vx <= n; ++vx)
    {
        for (int vy = 0; vy <= n; ++vy)
        {
            const bool f00 = isExplored(vx - 1, vy - 1);
            const bool f10 = isExplored(vx, vy - 1);
            const bool f01 = isExplored(vx - 1, vy);
            const bool f11 = isExplored(vx, vy);
            const int count = (f00 ? 1 : 0) + (f10 ? 1 : 0) + (f01 ? 1 : 0) + (f11 ? 1 : 0);

            int odd = -1;
            if (count == 1)
            {
                if (f00) odd = 0;
                else if (f10) odd = 1;
                else if (f01) odd = 2;
                else odd = 3;
            }
            else if (count == 3)
            {
                if (!f00) odd = 0;
                else if (!f10) odd = 1;
                else if (!f01) odd = 2;
                else odd = 3;
            }
            else
                continue;

            const float cx = static_cast<float>(vx * cellPx);
            const float cy = static_cast<float>(vy * cellPx);

            switch (odd)
            {
            case 0:
                // Tip [cx-R,cx]×[cy-R,cy]; outside arc = fog, inside = clear (convex & concave)
                for (int py = static_cast<int>(floorf(cy - R)); py < static_cast<int>(ceilf(cy)); ++py)
                {
                    if (py < 0 || py >= size) continue;
                    DWORD* row = rowPtr(py);
                    for (int px = static_cast<int>(floorf(cx - R)); px < static_cast<int>(ceilf(cx)); ++px)
                    {
                        if (px < 0 || px >= size) continue;
                        const float dx = (static_cast<float>(px) + 0.5f) - (cx - R);
                        const float dy = (static_cast<float>(py) + 0.5f) - (cy - R);
                        row[px] = (dx * dx + dy * dy <= R * R) ? kClear : kFog;
                    }
                }
                break;
            case 1:
                for (int py = static_cast<int>(floorf(cy - R)); py < static_cast<int>(ceilf(cy)); ++py)
                {
                    if (py < 0 || py >= size) continue;
                    DWORD* row = rowPtr(py);
                    for (int px = static_cast<int>(floorf(cx)); px < static_cast<int>(ceilf(cx + R)); ++px)
                    {
                        if (px < 0 || px >= size) continue;
                        const float dx = (static_cast<float>(px) + 0.5f) - (cx + R);
                        const float dy = (static_cast<float>(py) + 0.5f) - (cy - R);
                        row[px] = (dx * dx + dy * dy <= R * R) ? kClear : kFog;
                    }
                }
                break;
            case 2:
                for (int py = static_cast<int>(floorf(cy)); py < static_cast<int>(ceilf(cy + R)); ++py)
                {
                    if (py < 0 || py >= size) continue;
                    DWORD* row = rowPtr(py);
                    for (int px = static_cast<int>(floorf(cx - R)); px < static_cast<int>(ceilf(cx)); ++px)
                    {
                        if (px < 0 || px >= size) continue;
                        const float dx = (static_cast<float>(px) + 0.5f) - (cx - R);
                        const float dy = (static_cast<float>(py) + 0.5f) - (cy + R);
                        row[px] = (dx * dx + dy * dy <= R * R) ? kClear : kFog;
                    }
                }
                break;
            case 3:
                for (int py = static_cast<int>(floorf(cy)); py < static_cast<int>(ceilf(cy + R)); ++py)
                {
                    if (py < 0 || py >= size) continue;
                    DWORD* row = rowPtr(py);
                    for (int px = static_cast<int>(floorf(cx)); px < static_cast<int>(ceilf(cx + R)); ++px)
                    {
                        if (px < 0 || px >= size) continue;
                        const float dx = (static_cast<float>(px) + 0.5f) - (cx + R);
                        const float dy = (static_cast<float>(py) + 0.5f) - (cy + R);
                        row[px] = (dx * dx + dy * dy <= R * R) ? kClear : kFog;
                    }
                }
                break;
            default:
                break;
            }
        }
    }

    m_pFogMaskTex->UnlockRect(0);
    return true;
}

void pMainMenu::DrawMapFogShader(float mapL, float mapT, float mapSize,
                                float coverL, float coverT, float coverW, float coverH)
{
    if (!m_pShader || !m_pDevice || mapSize <= 1.0f)
        return;

    // Always draw: fully explored land still has sea fog (UV outside 0..1).

    // Match orange outline radius (incl. min thickness clamp) in cell units
    const float cell = mapSize / static_cast<float>(Layout::MapFogCells);
    float R = cell * Layout::MapExploredCorner;
    float t = Layout::MapExploredLineW;
    if (t < 1.0f)
        t = 1.0f;
    if (R < t * 2.0f)
        R = t * 2.0f;
    if (R > cell * 0.45f)
        R = cell * 0.45f;
    const float Rcell = R / cell;

    // UV 0..1 = map square; cover can extend into sea (UV outside 0..1)
    const float u0 = (coverL - mapL) / mapSize;
    const float v0 = (coverT - mapT) / mapSize;
    const float u1 = (coverL + coverW - mapL) / mapSize;
    const float v1 = (coverT + coverH - mapT) / mapSize;

    if (m_pShader->HasZonesFog() && UploadZonesTex())
    {
        m_pShader->DrawFogFromZones(m_pZonesTex, coverL, coverT, coverW, coverH,
                                    Layout::MapFogCells, Rcell, Layout::MapFogColor,
                                    Layout::MapFogSoft, u0, v0, u1, v1);
        return;
    }

    if (!BuildFogContourMask() || !m_pFogMaskTex)
        return;
    m_pShader->DrawFogFromMask(m_pFogMaskTex, mapL, mapT, mapSize, mapSize, Layout::MapFogColor);
}

void pMainMenu::DrawMapExploredOutlines(float mapL, float mapT, float mapSize)
{
    // Contour of ZonesVisited with rounded convex (1) / concave (3) corners.
    // All strokes share one centerline on the explored/fog cell boundary (same thickness).
    if (!m_pDraw || mapSize <= 1.0f || !CTheZones::ExploredTerritoriesArray)
        return;

    const char* visited = CTheZones::ExploredTerritoriesArray;
    const int n = Layout::MapFogCells;
    const float cell = mapSize / static_cast<float>(n);
    float t = Layout::MapExploredLineW;
    if (t < 1.0f)
        t = 1.0f;
    float R = cell * Layout::MapExploredCorner;
    if (R < t * 2.0f)
        R = t * 2.0f;
    if (R > cell * 0.45f)
        R = cell * 0.45f;

    const DWORD col = Layout::MapExploredLine;
    constexpr float kPi = 3.14159265f;

    auto isExplored = [visited, n](int x, int y) -> bool
    {
        if (x < 0 || y < 0 || x >= n || y >= n)
            return false;
        return visited[x * n + y] != 0;
    };

    // Odd-one-out at grid vertex: 1 = convex, 3 = concave — same quarter-arc geometry
    auto cornerOdd = [&](int vx, int vy, int& outOdd) -> bool
    {
        const bool f00 = isExplored(vx - 1, vy - 1);
        const bool f10 = isExplored(vx, vy - 1);
        const bool f01 = isExplored(vx - 1, vy);
        const bool f11 = isExplored(vx, vy);
        const int count = (f00 ? 1 : 0) + (f10 ? 1 : 0) + (f01 ? 1 : 0) + (f11 ? 1 : 0);
        if (count == 1)
        {
            if (f00) outOdd = 0;
            else if (f10) outOdd = 1;
            else if (f01) outOdd = 2;
            else outOdd = 3;
            return true;
        }
        if (count == 3)
        {
            if (!f00) outOdd = 0;
            else if (!f10) outOdd = 1;
            else if (!f01) outOdd = 2;
            else outOdd = 3;
            return true;
        }
        return false;
    };

    auto isRoundCorner = [&](int vx, int vy) -> bool
    {
        int odd = 0;
        return cornerOdd(vx, vy, odd);
    };

    auto drawArc = [&](float cx, float cy, float a0, float a1)
    {
        // Centerline radius R — same boundary as straight edges; thickness centered on it
        const float sweep = std::fabs(a1 - a0);
        const float arcLen = R * sweep;
        int segs = static_cast<int>(std::ceil(arcLen / 1.5f));
        if (segs < 48)
            segs = 48;
        if (segs > 96)
            segs = 96;
        m_pDraw->DrawThickArc(cx, cy, R, a0, a1, t, col, segs);
    };

    // Straight edges on cell boundary centerline, stop at R before rounded corners
    for (int x = 0; x < n; ++x)
    {
        for (int y = 0; y < n; ++y)
        {
            if (!isExplored(x, y))
                continue;

            const float L = mapL + static_cast<float>(x) * cell;
            const float T = mapT + static_cast<float>(y) * cell;
            const float Right = L + cell;
            const float B = T + cell;

            if (!isExplored(x, y - 1))
            {
                float x0 = L;
                float x1 = Right;
                if (isRoundCorner(x, y))
                    x0 += R;
                if (isRoundCorner(x + 1, y))
                    x1 -= R;
                if (x1 > x0)
                    m_pDraw->DrawThickLine(x0, T, x1, T, t, col);
            }
            if (!isExplored(x, y + 1))
            {
                float x0 = L;
                float x1 = Right;
                if (isRoundCorner(x, y + 1))
                    x0 += R;
                if (isRoundCorner(x + 1, y + 1))
                    x1 -= R;
                if (x1 > x0)
                    m_pDraw->DrawThickLine(x0, B, x1, B, t, col);
            }
            if (!isExplored(x - 1, y))
            {
                float y0 = T;
                float y1 = B;
                if (isRoundCorner(x, y))
                    y0 += R;
                if (isRoundCorner(x, y + 1))
                    y1 -= R;
                if (y1 > y0)
                    m_pDraw->DrawThickLine(L, y0, L, y1, t, col);
            }
            if (!isExplored(x + 1, y))
            {
                float y0 = T;
                float y1 = B;
                if (isRoundCorner(x + 1, y))
                    y0 += R;
                if (isRoundCorner(x + 1, y + 1))
                    y1 -= R;
                if (y1 > y0)
                    m_pDraw->DrawThickLine(Right, y0, Right, y1, t, col);
            }
        }
    }

    // Quarter-arcs at convex / concave contour corners (screen Y down)
    for (int vx = 0; vx <= n; ++vx)
    {
        for (int vy = 0; vy <= n; ++vy)
        {
            int odd = 0;
            if (!cornerOdd(vx, vy, odd))
                continue;

            const float cx = mapL + static_cast<float>(vx) * cell;
            const float cy = mapT + static_cast<float>(vy) * cell;

            switch (odd)
            {
            case 0: // NW cell (or NW hole) — arc at its SE
                drawArc(cx - R, cy - R, 0.0f, kPi * 0.5f);
                break;
            case 1: // NE — arc at SW
                drawArc(cx + R, cy - R, kPi, kPi * 0.5f);
                break;
            case 2: // SW — arc at NE
                drawArc(cx - R, cy + R, 0.0f, -kPi * 0.5f);
                break;
            case 3: // SE — arc at NW
                drawArc(cx + R, cy + R, kPi, kPi * 1.5f);
                break;
            default:
                break;
            }
        }
    }
}

void pMainMenu::DrawMapTiles(float screenW, float screenH)
{
    if (!m_pDraw || !IsMapOpen())
        return;

    const MapLayout layout = ComputeMapLayout(screenW, screenH);

    // Shader fog from ZonesVisited (per-pixel arcs = orange contour)
    const bool mapReady = MapTilesReady() || (RadarConfig::GetCustomRadarTxd()
        ? LoadMapTextures()
        : EnsureStockRadarTextures());
    const bool fogReady = mapReady && m_pShader && m_pShader->IsReady();

    if (m_bMapFullscreen)
    {
        m_pDraw->DrawRect(0.0f, 0.0f, screenW, screenH, Layout::MapUnderlay);
        if (mapReady)
        {
            DrawMapPlane(layout.mapL, layout.mapT, layout.mapSize);
            if (fogReady)
                DrawMapFogShader(layout.mapL, layout.mapT, layout.mapSize,
                                 0.0f, 0.0f, screenW, screenH);
        }
        return;
    }

    const float aw = layout.areaR - layout.areaL;
    const float ah = layout.areaB - layout.areaT;

    if (aw > 1.0f && ah > 1.0f && m_pDraw->BeginClipRT(layout.areaL, layout.areaT, aw, ah))
    {
        m_pDraw->DrawRect(0.0f, 0.0f, aw, ah, Layout::MapUnderlay);
        if (mapReady)
        {
            const float ml = layout.mapL - layout.areaL;
            const float mt = layout.mapT - layout.areaT;
            DrawMapPlane(ml, mt, layout.mapSize);
            if (fogReady)
                DrawMapFogShader(ml, mt, layout.mapSize, 0.0f, 0.0f, aw, ah);
        }
        m_pDraw->EndClipRT();
    }
    else
    {
        m_pDraw->DrawRect(layout.areaL, layout.areaT, aw, ah, Layout::MapUnderlay);
        if (mapReady)
        {
            DrawMapPlane(layout.mapL, layout.mapT, layout.mapSize);
            if (fogReady)
                DrawMapFogShader(layout.mapL, layout.mapT, layout.mapSize,
                                 layout.areaL, layout.areaT, aw, ah);
        }
    }
}

void pMainMenu::DrawMapGps(float screenW, float screenH)
{
    if (!m_pDraw || !IsMapOpen() || !RadarConfig::GetGps())
        return;

    const MapLayout layout = ComputeMapLayout(screenW, screenH);
    const float mapR = layout.mapL + layout.mapSize;
    const float mapB = layout.mapT + layout.mapSize;
    const float clipL = (layout.areaL > layout.mapL) ? layout.areaL : layout.mapL;
    const float clipT = (layout.areaT > layout.mapT) ? layout.areaT : layout.mapT;
    const float clipR = (layout.areaR < mapR) ? layout.areaR : mapR;
    const float clipB = (layout.areaB < mapB) ? layout.areaB : mapB;

    GpsRenderer::RenderMap2D(m_pDraw,
        layout.mapL, layout.mapT, layout.mapSize,
        clipL, clipT, clipR, clipB, screenW);
}

void pMainMenu::DrawMapLeftArt(float screenW, float screenH)
{
    if (!m_pDraw || !m_pBackground || !IsMapOpen() || m_bMapFullscreen)
        return;

    const float leftW = screenW * Layout::MapLeftFrac;
    m_pDraw->DrawTexture(m_pBackground, 0.0f, 0.0f, leftW, screenH,
                         0.0f, 0.0f, Layout::MapLeftFrac, 1.0f);
}

void pMainMenu::DrawMapBlips(float screenW, float screenH)
{
    if (!IsMapOpen() || screenW < 1.0f || screenH < 1.0f)
        return;

    const MapLayout layout = ComputeMapLayout(screenW, screenH);
    if (layout.mapSize <= 1.0f)
        return;

    StockRadarPlane plane;
    plane.use3D = false;
    plane.cx = layout.mapL + layout.mapSize * 0.5f;
    plane.cy = layout.mapT + layout.mapSize * 0.5f;
    plane.half = layout.mapSize * 0.5f;

    const float mapR = layout.mapL + layout.mapSize;
    const float mapB = layout.mapT + layout.mapSize;
    plane.clipL = (layout.areaL > layout.mapL) ? layout.areaL : layout.mapL;
    plane.clipT = (layout.areaT > layout.mapT) ? layout.areaT : layout.mapT;
    plane.clipR = (layout.areaR < mapR) ? layout.areaR : mapR;
    plane.clipB = (layout.areaB < mapB) ? layout.areaB : mapB;
    if (plane.clipR < plane.clipL)
        plane.clipR = plane.clipL;
    if (plane.clipB < plane.clipT)
        plane.clipB = plane.clipT;

    const bool wasMapLoaded = FrontEndMenuManager.m_bStandardInput;

    StockRadarDraw::SetPlane(plane);
    StockRadarDraw::Begin();
    FrontEndMenuManager.m_bDrawRadarOrMap = true;
    FrontEndMenuManager.m_bStandardInput = false;

    CRadar::InitFrontEndMap();
    StockRadarDraw::Draw(RadarConfig::GetShowGangZones(), true);

    StockRadarDraw::End();
    FrontEndMenuManager.m_bStandardInput = wasMapLoaded;
    FrontEndMenuManager.m_bDrawRadarOrMap = false;
}

void pMainMenu::GetMapHoverPlaceNameUtf8(float screenW, float screenH, char* out, size_t outChars) const
{
    const char* def = LanguageManager::Get("ZONE_DEFAULT");
    if (!out || outChars == 0)
        return;
    strncpy_s(out, outChars, def ? def : "", _TRUNCATE);

    float cx = 0.0f, cy = 0.0f;
    if (!GetCursorPosClient(screenW, screenH, cx, cy))
        return;

    const MapLayout layout = ComputeMapLayout(screenW, screenH);
    if (cx < layout.areaL || cx >= layout.areaR || cy < layout.areaT || cy >= layout.areaB)
        return;

    float wx = 0.0f, wy = 0.0f;
    if (!MapScreenToWorld(cx, cy, layout, wx, wy))
        return;

    if (CTheZones::TotalNumberExploredTerritories < Layout::MapFogRevealAll
        && !CTheZones::GetCurrentZoneLockedOrUnlocked(wx, wy))
        return;

    CZone* zone = CTheZones::FindSmallestZoneForPosition(CVector(wx, wy, 0.0f), false);
    if (!zone)
        return;

    const char* name = LanguageManager::LookupZone(zone->m_szTextKey);
    if (!name)
        name = LanguageManager::LookupZone(zone->m_szLabel);
    if (!name || !name[0])
        return;
    strncpy_s(out, outChars, name, _TRUNCATE);
}

void pMainMenu::DrawMapHintBar(float screenW, float screenH)
{
    if (!m_pDraw || !m_bMapFullscreen)
        return;

    const float sx = screenW / Layout::RefW;
    const float sy = screenH / Layout::RefH;
    const float padX = Layout::MapHintPadX * sx;
    const float padY = Layout::MapHintPadY * sy;
    // Static right/bottom anchors (DE: right margin fixed; width follows text)
    const float right = screenW - Layout::MapHintInsetX * sx;
    const float bottom = screenH - Layout::MapHintInsetY * sy;

    // Control hints — measure first, then size the rect to the text
    const char* kHints = LanguageManager::Get("MAP_HINTS");

    const int hintFontH = static_cast<int>(Layout::MapHintFont * sy + 0.5f);
    m_pDraw->EnsureFontHeight(hintFontH > 0 ? hintFontH : 1);
    const float hintTextW = m_pDraw->GetTextWidth(kHints, 1.0f);
    const float hintTextH = m_pDraw->GetFontHeight(1.0f);

    float h = hintTextH + padY * 2.0f;
    const float minH = Layout::MapHintH * sy;
    if (h < minH)
        h = minH;
    float w = hintTextW + padX * 2.0f;
    if (w < 1.0f)
        w = 1.0f;

    const float x = right - w;
    const float y = bottom - h;
    m_pDraw->DrawRect(x, y, w, h, Layout::MapHintColor);

    m_pDraw->DrawString(x + padX, y + padY, right - padX, y + h - padY,
                        0xFFFFFFFF, kHints, 1.0f, 1.0f,
                        DT_RIGHT | DT_VCENTER | DT_SINGLELINE | DT_NOCLIP);

    // Place name above the bar — same right edge as hints (DE)
    char placeName[256];
    GetMapHoverPlaceNameUtf8(screenW, screenH, placeName, sizeof(placeName));
    const int zoneFontH = static_cast<int>(Layout::MapZoneFont * sy + 0.5f);
    m_pDraw->EnsureFontHeight(zoneFontH > 0 ? zoneFontH : 1);
    const float zoneH = m_pDraw->GetFontHeight();
    const float zoneGap = Layout::MapZoneGap * sy;
    const float zoneTextW = m_pDraw->GetTextWidth(placeName, 1.0f);
    // Name can be wider than the hint bar — still right-anchored to the same edge
    const float zoneLeft = right - padX - (zoneTextW > 0.0f ? zoneTextW : w);
    m_pDraw->DrawString(zoneLeft, y - zoneGap - zoneH, right - padX, y - zoneGap * 0.25f,
                        0xFFFFFFFF, placeName, 1.0f, 1.0f,
                        DT_RIGHT | DT_BOTTOM | DT_SINGLELINE | DT_NOCLIP);
}

pMainMenu::ButtonRect pMainMenu::GetMapLegendRect(float screenW, float screenH) const
{
    const float sx = screenW / Layout::RefW;
    const float sy = screenH / Layout::RefH;
    const float l = Layout::LegendX * sx;
    const float t = Layout::LegendY * sy;
    return { l, t, l + Layout::LegendW * sx, t + Layout::LegendH * sy };
}

bool pMainMenu::IsCursorOnMapLegend(float cursorX, float cursorY, float screenW, float screenH) const
{
    if (!m_bMapLegend || !m_bMapFullscreen)
        return false;
    return GetMapLegendRect(screenW, screenH).Contains(cursorX, cursorY);
}

void pMainMenu::HandleMapLegendToggle()
{
    const bool tab = (GetAsyncKeyState(VK_TAB) & 0x8000) != 0;
    const bool pressed = tab && !m_bTabWasDown;
    m_bTabWasDown = tab;

    if (!pressed || !m_bMapFullscreen || m_bExitConfirm)
        return;
    if (m_pSettings && m_pSettings->IsRebindWaiting())
        return;

    m_bMapLegend = !m_bMapLegend;
    PlayFe(m_bMapLegend ? AE_FRONTEND_SELECT : AE_FRONTEND_BACK);
}

void pMainMenu::DrawMapLegend(float screenW, float screenH)
{
    if (!m_pDraw || !m_bMapFullscreen || !m_bMapLegend)
        return;

    const auto* list = reinterpret_cast<const int16_t*>(CRadar::MapLegendList);
    const int rawCount = CRadar::MapLegendCounter;
    if (!list || rawCount <= 0)
        return;

    int16_t sprites[175];
    int count = 0;
    const int cap = static_cast<int>(sizeof(sprites) / sizeof(sprites[0]));
    const int n = (rawCount < cap) ? rawCount : cap;
    for (int i = 0; i < n; ++i)
    {
        const int16_t sprite = list[i];
        if (!LegendGxtKey(sprite))
            continue;
        sprites[count++] = sprite;
    }
    if (count <= 0)
        return;

    const ButtonRect box = GetMapLegendRect(screenW, screenH);
    const float sx = screenW / Layout::RefW;
    const float sy = screenH / Layout::RefH;
    const float panelW = box.right - box.left;
    const float panelH = box.bottom - box.top;
    const float rowH = panelH / static_cast<float>(count);
    const float padX = Layout::LegendPadX * sx;
    float icon = Layout::LegendIcon * sy;
    if (icon > rowH * 0.72f)
        icon = rowH * 0.72f;
    const float iconGap = Layout::LegendIconGap * sx;

    int fontH = static_cast<int>(Layout::LegendFont * sy + 0.5f);
    const int maxFont = static_cast<int>(rowH * 0.55f + 0.5f);
    if (fontH > maxFont)
        fontH = maxFont;
    m_pDraw->EnsureFontHeight(fontH > 0 ? fontH : 1);

    BlipManager::PushDeIcons();

    for (int i = 0; i < count; ++i)
    {
        const float y = box.top + rowH * static_cast<float>(i);
        const DWORD bg = (i & 1) ? Layout::LegendZebraB : Layout::LegendZebraA;
        m_pDraw->DrawRect(box.left, y, panelW, rowH, bg);

        const int sprite = sprites[i];
        const float iconX = box.left + padX;
        const float iconY = y + (rowH - icon) * 0.5f;

        if (sprite < 0)
        {
            const int colorIdx = -sprite;
            DWORD col = 0xFFFF3333;
            if (colorIdx >= 0 && colorIdx < 6 && CRadar::ArrowBlipColour)
            {
                const CRGBA c = CRadar::ArrowBlipColour[colorIdx];
                col = D3DCOLOR_ARGB(c.a ? c.a : 255, c.r, c.g, c.b);
            }
            m_pDraw->DrawCircleAA(iconX + icon * 0.5f, iconY + icon * 0.5f, icon * 0.32f, col);
        }
        else if (sprite > 0 && sprite <= BlipManager::MAX_BLIP_ID && CRadar::RadarBlipSprites)
        {
            if (LPDIRECT3DTEXTURE9 tex = NativeRwTexture(CRadar::RadarBlipSprites[sprite].m_pTexture))
                m_pDraw->DrawTexture(tex, iconX, iconY, icon, icon);
        }

        const char* key = LegendGxtKey(sprite);
        const char* label = LanguageManager::Get(key);
        const float textL = iconX + icon + iconGap;
        m_pDraw->DrawString(textL, y, box.right - padX, y + rowH,
                            0xFFFFFFFF, label ? label : "", 1.0f, 1.0f,
                            DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOCLIP);
    }

    BlipManager::PopDeIcons();
}

void pMainMenu::GetScreenSize(float& outW, float& outH) const
{
    Ui::GetScreenSizeViewport(m_pDevice, outW, outH);
}

bool pMainMenu::GetCursorPosClient(float screenW, float screenH, float& outX, float& outY) const
{
    return Ui::GetCursorPosClient(screenW, screenH, outX, outY);
}

void pMainMenu::ShowOsCursor()
{
    FrontEndMenuManager.m_bShowMouse = false;
    ClipCursor(nullptr);

    if (auto* input = InputManager::GetInstance())
        input->SetCursorOverride(true);

    SetCursor(LoadCursor(nullptr, IDC_ARROW));

    // Alt-Tab / focus loss can hide the OS cursor while our hold flag stays true
    CURSORINFO ci{ sizeof(ci) };
    if (GetCursorInfo(&ci) && (ci.flags & CURSOR_SHOWING))
    {
        m_bOsCursorHeld = true;
        return;
    }

    while (ShowCursor(TRUE) < 0)
    {
    }
    m_bOsCursorHeld = true;
}

void pMainMenu::HideOsCursor()
{
    FrontEndMenuManager.m_bShowMouse = false;

    if (auto* input = InputManager::GetInstance())
        input->SetCursorOverride(false);

    if (!m_bOsCursorHeld)
        return;

    Ui::HideOsCursor();
    m_bOsCursorHeld = false;
}

pMainMenu::ButtonRect pMainMenu::GetButtonRect(int index, float screenW, float screenH) const
{
    const float sx = screenW / Layout::RefW;
    const float sy = screenH / Layout::RefH;

    const float cx = Layout::CenterX * sx;
    const float cy = Layout::CenterYFromTop(index) * sy;
    const float halfW = Layout::HoverWidth * sx * 0.5f;
    const float hitH = (Layout::HoverHeight > Layout::RowStepY - 8.0f)
        ? (Layout::RowStepY - 8.0f)
        : Layout::HoverHeight;
    const float halfH = hitH * sy * 0.5f;

    return { cx - halfW, cy - halfH, cx + halfW, cy + halfH };
}

pMainMenu::ButtonRect pMainMenu::GetExitConfirmBtnRect(int which, float screenW, float screenH) const
{
    const float sx = screenW / Layout::RefW;
    const float sy = screenH / Layout::RefH;
    const float btnW = Layout::ExitConfirmBtnW * sx;
    const float btnH = Layout::ExitConfirmBtnH * sy;
    const float gap = Layout::ExitConfirmBtnGap * sx;
    const float pairW = btnW * 2.0f + gap;
    const float left0 = screenW * 0.5f - pairW * 0.5f;
    const float top = screenH * 0.5f + Layout::ExitConfirmBtnDown * btnH - btnH * 0.5f;
    const float left = left0 + static_cast<float>(which) * (btnW + gap);
    return { left, top, left + btnW, top + btnH };
}

int pMainMenu::HitTestButton(float cursorX, float cursorY, float screenW, float screenH) const
{
    int best = -1;
    float bestDist2 = 1.0e30f;
    for (int i = 0; i < Layout::Count; ++i)
    {
        const ButtonRect box = GetButtonRect(i, screenW, screenH);
        if (!box.Contains(cursorX, cursorY))
            continue;

        const float mx = (box.left + box.right) * 0.5f;
        const float my = (box.top + box.bottom) * 0.5f;
        const float dx = cursorX - mx;
        const float dy = cursorY - my;
        const float d2 = dx * dx + dy * dy;
        if (d2 < bestDist2)
        {
            bestDist2 = d2;
            best = i;
        }
    }
    return best;
}

void pMainMenu::SyncActiveFromPanel()
{
    if (!m_pMainMenu)
        return;

    switch (m_pMainMenu->GetOpenPanel())
    {
    case MainMenu::Panel::Game:
        m_nActive = static_cast<int>(Button::Game);
        break;
    case MainMenu::Panel::Settings:
        m_nActive = static_cast<int>(Button::Settings);
        break;
    default:
        if (m_nActive == static_cast<int>(Button::Game)
            || m_nActive == static_cast<int>(Button::Settings))
            m_nActive = static_cast<int>(Button::Map);
        break;
    }
}

void pMainMenu::UpdateHoverSound(float screenW, float screenH)
{
    if (m_bMapFullscreen || m_bExitConfirm)
    {
        m_nHoverSoundId = -1;
        return;
    }

    float cx = 0.0f, cy = 0.0f;
    int id = -1;
    if (GetCursorPosClient(screenW, screenH, cx, cy))
    {
        const int btn = HitTestButton(cx, cy, screenW, screenH);
        if (btn >= 0)
            id = 100 + btn; // same id scheme as MainMenu::HitTestHoverSoundId
    }

    if (id >= 0 && id != m_nHoverSoundId)
        PlayFe(AE_FRONTEND_HIGHLIGHT);

    m_nHoverSoundId = id;
}

void pMainMenu::ConsumeEscJustPressed()
{
    CPad::OldKeyState.esc = CPad::NewKeyState.esc;
}

void pMainMenu::LeavePauseSession()
{
    CloseExitConfirm();
    if (m_pMainMenu)
        m_pMainMenu->ClosePanel();
    ResetMapView();
    m_nActive = -1;
    m_bPauseWasOpen = false;
    HideOsCursor();
}

void pMainMenu::ResumeGame()
{
    LeavePauseSession();
    ConsumeEscJustPressed();
    if (m_pHooks)
        m_pHooks->RequestResumeGame();
}

void pMainMenu::HandleEscape()
{
    // ESC already applied in HookManager::Process_Detour (MenuActive cleared).
    // Here: one-shot UI teardown + sound when that flag is set.
    if (!m_pHooks)
        return;

    if (!m_pHooks->ConsumePauseEscClose())
        return;

    if (m_pSettings && m_pSettings->IsRebindWaiting())
        return;

    PlayFe(AE_FRONTEND_BACK);
    LeavePauseSession();
    // MenuActive already false; just clear pad edge / cursor leftovers
    ConsumeEscJustPressed();
}

void pMainMenu::OnButtonActivated(int index)
{
    PlayFe(AE_FRONTEND_SELECT);

    switch (static_cast<Button>(index))
    {
    case Button::Continue:
        ResumeGame();
        break;

    case Button::Map:
        if (m_pMainMenu)
            m_pMainMenu->ClosePanel();
        ResetMapView();
        CenterMapOnPlayer();
        m_nActive = index;
        break;

    case Button::Messages:
    case Button::Stats:
        if (m_pMainMenu)
            m_pMainMenu->ClosePanel();
        ResetMapView();
        m_nActive = index;
        break;

    case Button::Game:
        if (m_pMainMenu)
        {
            m_pMainMenu->OpenGamePanel();
            m_pMainMenu->SwallowNextClick();
        }
        ResetMapView();
        m_nActive = index;
        break;

    case Button::Settings:
        if (m_pMainMenu)
        {
            m_pMainMenu->OpenSettingsPanel();
            m_pMainMenu->SwallowNextClick();
        }
        ResetMapView();
        m_nActive = index;
        break;

    case Button::Exit:
        OpenExitConfirm();
        break;

    default:
        break;
    }
}

void pMainMenu::UpdateMapZoom(float screenW, float screenH)
{
    if (!IsMapOpen())
        return;

    int wheel = 0;
    if (auto* input = InputManager::GetInstance())
        wheel = input->ConsumeMouseWheelDelta();

    // Zoom only after map click (fullscreen). Strip mode: discard wheel.
    if (!m_bMapFullscreen)
        return;

    float cx = 0.0f, cy = 0.0f;
    const bool haveCursor = GetCursorPosClient(screenW, screenH, cx, cy);

    if (wheel != 0)
    {
        // Discrete notches (WHEEL_DELTA = 120) — each notch = one step
        int notches = wheel / WHEEL_DELTA;
        if (notches == 0)
            notches = (wheel > 0) ? 1 : -1;

        for (int i = 0; i < notches; ++i)
            m_fMapZoomTarget *= Layout::MapZoomStep;
        for (int i = 0; i > notches; --i)
            m_fMapZoomTarget /= Layout::MapZoomStep;
    }

    if (m_fMapZoomTarget < Layout::MapZoomMin)
        m_fMapZoomTarget = Layout::MapZoomMin;
    if (m_fMapZoomTarget > Layout::MapZoomMax)
        m_fMapZoomTarget = Layout::MapZoomMax;

    if (wheel != 0)
    {

        // Freeze focus under cursor for the whole inertial settle
        const MapLayout before = ComputeMapLayout(screenW, screenH);
        if (haveCursor && before.mapSize > 1.0f)
        {
            m_fMapZoomFocusSX = cx;
            m_fMapZoomFocusSY = cy;
            m_fMapZoomFocusRelX = (cx - before.mapL) / before.mapSize;
            m_fMapZoomFocusRelY = (cy - before.mapT) / before.mapSize;
            // If cursor is outside the map quad, zoom toward view center of map
            if (m_fMapZoomFocusRelX < 0.0f || m_fMapZoomFocusRelX > 1.0f
                || m_fMapZoomFocusRelY < 0.0f || m_fMapZoomFocusRelY > 1.0f)
            {
                m_fMapZoomFocusSX = before.mapL + before.mapSize * 0.5f;
                m_fMapZoomFocusSY = before.mapT + before.mapSize * 0.5f;
                m_fMapZoomFocusRelX = 0.5f;
                m_fMapZoomFocusRelY = 0.5f;
            }
            m_bMapZoomFocus = true;
        }
    }

    m_fMapZoom += (m_fMapZoomTarget - m_fMapZoom) * Layout::MapZoomLerp;
    if (std::fabs(m_fMapZoomTarget - m_fMapZoom) < 0.001f)
    {
        m_fMapZoom = m_fMapZoomTarget;
        m_bMapZoomFocus = false;
    }

    if (m_bMapZoomFocus)
        ApplyMapZoomFocus(screenW, screenH);
}

void pMainMenu::HandleMapInput(float screenW, float screenH)
{
    if (!IsMapOpen())
    {
        m_bMapPress = false;
        m_bMapDragging = false;
        m_bMapPressOpenedFs = false;
        m_bMapClickWorldValid = false;
        return;
    }

    float cx = 0.0f, cy = 0.0f;
    if (!GetCursorPosClient(screenW, screenH, cx, cy))
        return;

    const MapLayout layout = ComputeMapLayout(screenW, screenH);
    bool overArea = cx >= layout.areaL && cx < layout.areaR
        && cy >= layout.areaT && cy < layout.areaB;
    if (overArea && !m_bMapPress && IsCursorOnMapLegend(cx, cy, screenW, screenH))
        overArea = false;

    // Left menu buttons win in strip mode
    if (!m_bMapFullscreen && !m_bMapPress && HitTestButton(cx, cy, screenW, screenH) >= 0)
        return;

    HWND hwnd = (RsGlobal.ps && RsGlobal.ps->window) ? RsGlobal.ps->window : nullptr;
    const bool focused = !hwnd || GetForegroundWindow() == hwnd;
    if (!focused)
    {
        m_bMapPress = false;
        m_bMapDragging = false;
        m_bMapPressOpenedFs = false;
        m_bRmbWasDown = (GetAsyncKeyState(VK_RBUTTON) & 0x8000) != 0;
        return;
    }

    const bool lmb = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
    const bool rmb = (GetAsyncKeyState(VK_RBUTTON) & 0x8000) != 0;

    // RMB — remove waypoint (edge)
    if (rmb && !m_bRmbWasDown && overArea && m_bMapFullscreen && !m_bSwallowClick)
    {
        if (FrontEndMenuManager.m_nTargetBlipIndex)
        {
            ClearMapWaypoint();
            PlayFe(AE_FRONTEND_BACK);
        }
    }
    m_bRmbWasDown = rmb;

    if (lmb && !m_bLmbWasDown)
    {
        if (m_bSwallowClick)
            return;
        if (overArea)
        {
            m_bMapPressOpenedFs = false;
            m_bMapClickWorldValid = MapScreenToWorld(cx, cy, layout, m_fMapClickWorldX, m_fMapClickWorldY);

            // Press on map in strip mode → hide left UI immediately (then pan/zoom)
            if (!m_bMapFullscreen)
            {
                EnterMapFullscreen();
                m_bMapPressOpenedFs = true;
            }

            m_bMapPress = true;
            m_bMapDragging = false;
            m_fMapDragLastX = cx;
            m_fMapDragLastY = cy;
            m_fMapDragStartX = cx;
            m_fMapDragStartY = cy;
        }
    }
    else if (lmb && m_bMapPress)
    {
        const float dist = std::fabs(cx - m_fMapDragStartX) + std::fabs(cy - m_fMapDragStartY);
        if (!m_bMapDragging && dist >= Layout::MapDragThreshold)
            m_bMapDragging = true;

        if (m_bMapDragging)
        {
            m_fMapPanX += cx - m_fMapDragLastX;
            m_fMapPanY += cy - m_fMapDragLastY;
            m_fMapDragLastX = cx;
            m_fMapDragLastY = cy;
            ClampMapPan(layout.areaR - layout.areaL, layout.areaB - layout.areaT, layout.mapSize);
        }
        else
        {
            m_fMapDragLastX = cx;
            m_fMapDragLastY = cy;
        }
    }
    else if (!lmb && m_bMapPress)
    {
        const bool wasDrag = m_bMapDragging;
        const bool openedFs = m_bMapPressOpenedFs;
        const bool worldOk = m_bMapClickWorldValid;
        const float clickX = m_fMapClickWorldX;
        const float clickY = m_fMapClickWorldY;

        m_bMapPress = false;
        m_bMapDragging = false;
        m_bMapPressOpenedFs = false;
        m_bMapClickWorldValid = false;

        // Click (no pan): place waypoint. Opening strip→FS alone does not place.
        if (!wasDrag && !openedFs && worldOk)
        {
            PlaceMapWaypoint(clickX, clickY);
            PlayFe(AE_FRONTEND_SELECT);
        }
    }
}

void pMainMenu::HandleClicks(float screenW, float screenH)
{
    HandleMapInput(screenW, screenH);

    HWND hwnd = (RsGlobal.ps && RsGlobal.ps->window) ? RsGlobal.ps->window : nullptr;
    const bool focused = !hwnd || GetForegroundWindow() == hwnd;
    if (!focused)
    {
        m_bWasFocused = false;
        m_bLmbWasDown = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
        return;
    }

    if (!m_bWasFocused)
    {
        m_bWasFocused = true;
        const bool lmbHeld = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
        m_bSwallowClick = lmbHeld;
        m_bLmbWasDown = lmbHeld;
        if (lmbHeld)
            return;
    }

    const bool lmb = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
    const bool clicked = lmb && !m_bLmbWasDown;
    const bool released = !lmb && m_bLmbWasDown;
    m_bLmbWasDown = lmb;

    if (m_bMapFullscreen || m_bMapPress || m_bMapDragging)
        return;

    if (!clicked)
    {
        if (released && m_bSwallowClick)
            m_bSwallowClick = false;
        return;
    }

    if (m_bSwallowClick)
    {
        m_bSwallowClick = false;
        return;
    }

    float cx = 0.0f, cy = 0.0f;
    if (!GetCursorPosClient(screenW, screenH, cx, cy))
        return;

    if (m_bExitConfirm)
    {
        if (GetExitConfirmBtnRect(0, screenW, screenH).Contains(cx, cy))
        {
            PlayFe(AE_FRONTEND_BACK);
            CloseExitConfirm();
        }
        else if (GetExitConfirmBtnRect(1, screenW, screenH).Contains(cx, cy))
        {
            PlayFe(AE_FRONTEND_SELECT);
            if (m_pMainMenu)
                m_pMainMenu->RequestExitGame();
        }
        return;
    }

    if (IsMapOpen())
    {
        const MapLayout layout = ComputeMapLayout(screenW, screenH);
        if (cx >= layout.areaL && cx < layout.areaR
            && cy >= layout.areaT && cy < layout.areaB)
            return;
    }

    const int btn = HitTestButton(cx, cy, screenW, screenH);
    if (btn >= 0)
        OnButtonActivated(btn);
}

void pMainMenu::DrawUiText(float left, float top, float right, float bottom, const char* text,
                           DWORD format, bool hovered, float screenW, float screenH,
                           bool onActivePlate, bool solidIdle)
{
    Ui::DrawMenuText(m_pDraw, left, top, right, bottom, text, format, hovered,
                     screenW, screenH, onActivePlate, solidIdle);
}

void pMainMenu::OpenExitConfirm()
{
    if (m_pMainMenu)
        m_pMainMenu->ClosePanel();
    ResetMapView();
    m_nActive = static_cast<int>(Button::Exit);
    m_bExitConfirm = true;
    m_bSwallowClick = true;
    if (m_pHooks)
        m_pHooks->SetPauseExitConfirm(true);
}

void pMainMenu::CloseExitConfirm()
{
    m_bExitConfirm = false;
    if (m_nActive == static_cast<int>(Button::Exit))
        m_nActive = static_cast<int>(Button::Map);
    if (m_pHooks)
        m_pHooks->SetPauseExitConfirm(false);
}

void pMainMenu::DrawExitConfirm(float screenW, float screenH)
{
    if (!m_pDraw || !m_bExitConfirm)
        return;

    float cursorX = 0.0f, cursorY = 0.0f;
    GetCursorPosClient(screenW, screenH, cursorX, cursorY);

    const float sy = screenH / Layout::RefH;
    const int fontH = static_cast<int>(Layout::ExitConfirmFont * sy + 0.5f);
    m_pDraw->EnsureFontHeight(fontH > 0 ? fontH : 1);

    const float textBody = m_pDraw->GetFontHeight();
    const float textCy = screenH * 0.5f - Layout::ExitConfirmTextUp * textBody;
    m_pDraw->DrawString(0.0f, textCy - textBody * 0.5f, screenW, textCy + textBody * 0.5f, 0xFF000000,
                        LanguageManager::Get("UI_ARE_YOU_SURE"), 1.0f, 1.0f,
                        DT_CENTER | DT_VCENTER | DT_NOCLIP | DT_SINGLELINE, false);

    const char* labels[2] = { LanguageManager::Get("UI_CANCEL"), LanguageManager::Get("UI_CONFIRM") };
    for (int i = 0; i < 2; ++i)
    {
        const ButtonRect btn = GetExitConfirmBtnRect(i, screenW, screenH);
        const bool hot = btn.Contains(cursorX, cursorY);
        Ui::DrawTexturedConfirmButton(m_pDraw, btn.left, btn.top, btn.right, btn.bottom,
                                      m_pExitBtnIdle[i], m_pExitBtnHover[i], hot, labels[i],
                                      screenW, screenH);
    }
}

void pMainMenu::DrawLogo(float screenW, float screenH)
{
    if (!m_pDraw || !m_pLogo)
        return;

    const float sx = screenW / Layout::RefW;
    const float sy = screenH / Layout::RefH;
    m_pDraw->DrawTexture(m_pLogo, Layout::LogoPadX * sx, Layout::LogoPadY * sy,
                         Layout::LogoW * sx, Layout::LogoH * sy);
}

void pMainMenu::DrawButtons(float screenW, float screenH)
{
    if (!m_pDraw)
        return;

    float cursorX = 0.0f;
    float cursorY = 0.0f;
    m_nHovered = -1;
    if (!m_bExitConfirm && GetCursorPosClient(screenW, screenH, cursorX, cursorY))
        m_nHovered = HitTestButton(cursorX, cursorY, screenW, screenH);

    const float sx = screenW / Layout::RefW;
    const float sy = screenH / Layout::RefH;
    const int fontH = static_cast<int>(Layout::FontSize * sy + 0.5f);
    m_pDraw->EnsureFontHeight(fontH > 0 ? fontH : 1);

    const float halfW = Layout::HoverWidth * sx * 0.5f;
    const float halfH = Layout::TextHeight * sy * 0.5f;

    for (int i = 0; i < Layout::Count; ++i)
    {
        const ButtonRect box = GetButtonRect(i, screenW, screenH);

        // Sticky active when cursor not on another left button;
        // while hovering another — only that one (MainMenu pattern)
        int showIdx = m_nActive;
        if (m_nHovered >= 0)
            showIdx = m_nHovered;
        const bool showHover = (showIdx >= 0 && i == showIdx);

        if (showHover && m_pHover[i])
            m_pDraw->DrawTexture(m_pHover[i], box.left, box.top,
                                 box.right - box.left, box.bottom - box.top);

        const float cx = Layout::CenterX * sx;
        const float cy = Layout::CenterYFromTop(i) * sy;
        DrawUiText(cx - halfW, cy - halfH, cx + halfW, cy + halfH, LanguageManager::Get(ButtonKeys[i]),
                   DT_CENTER | DT_VCENTER | DT_NOCLIP, false, screenW, screenH, showHover, true);
    }
}

void pMainMenu::Render()
{
    if (!m_bInitialized || !m_pDraw || !m_pHooks)
        return;

    if (m_pHooks->IsCustomMainMenuSession())
        return;

    // ESC may have cleared MenuActive in Process already — still run teardown once
    if (m_pHooks->ConsumePauseEscClose())
    {
        LeavePauseSession();
        return;
    }

    if (!m_pHooks->IsCustomPauseSession())
    {
        if (m_bPauseWasOpen)
            LeavePauseSession();
        return;
    }

    // First frame of this pause session
    if (!m_bPauseWasOpen)
    {
        m_bPauseWasOpen = true;
        m_bSwallowClick = false;
        m_bWasFocused = true;
        m_bLmbWasDown = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
        ConsumeEscJustPressed();
        m_nHoverSoundId = -1;
        m_bTabWasDown = (GetAsyncKeyState(VK_TAB) & 0x8000) != 0;
        ResetMapView();

        // Fresh pause: drop leftover title-screen Game/Settings (New Game confirm).
        // Save pickup keeps the save panel.
        if (m_pMainMenu
            && (FrontEndMenuManager.m_bSaveMenuActive || FrontEndMenuManager.m_bOnlySaveMenu))
        {
            m_pMainMenu->OpenGamePanelForSave();
            m_nActive = static_cast<int>(Button::Game);
        }
        else
        {
            if (m_pMainMenu)
                m_pMainMenu->ClosePanel();
            CenterMapOnPlayer();
            m_nActive = static_cast<int>(Button::Map);
        }
        CloseExitConfirm();

        WarmMapResources();
    }
    else if (!MapTilesReady())
    {
        // Keep decoding in background while user is still on other pause tabs.
        WarmMapResources();
    }

    if (m_pHooks->ConsumePauseMapFullscreenEsc())
        ExitMapFullscreen();
    if (m_pHooks->ConsumePauseExitConfirmEsc())
    {
        PlayFe(AE_FRONTEND_BACK);
        CloseExitConfirm();
    }

    ShowOsCursor();

    if (!LoadBackground())
        return;

    float w = 0.0f;
    float h = 0.0f;
    GetScreenSize(w, h);

    // Backup ESC: FS → strip map; else resume
    if (!(m_pSettings && m_pSettings->IsRebindWaiting())
        && CPad::NewKeyState.esc != 0 && CPad::OldKeyState.esc == 0)
    {
        if (m_bMapFullscreen)
            ExitMapFullscreen();
        else if (m_bExitConfirm)
        {
            PlayFe(AE_FRONTEND_BACK);
            CloseExitConfirm();
        }
        else
        {
            PlayFe(AE_FRONTEND_BACK);
            ResumeGame();
            return;
        }
    }

    SyncActiveFromPanel();
    HandleMapLegendToggle();
    UpdateMapZoom(w, h);
    UpdateHoverSound(w, h);
    HandleClicks(w, h);
    if (!m_pHooks->IsCustomPauseSession())
    {
        LeavePauseSession();
        return;
    }
    if (!m_bInitialized || !m_pDraw)
        return;

    // Map tiles (D3D) → stock blips (RW) → chrome (D3D) so sprites sit on the plane
    m_pDraw->BeginUi();
    if (!IsMapOpen())
        m_pDraw->DrawTexture(m_pBackground, 0.0f, 0.0f, w, h);
    else
    {
        DrawMapTiles(w, h);
        DrawMapGps(w, h);
    }
    m_pDraw->EndUi();

    if (IsMapOpen())
        DrawMapBlips(w, h);

    if (!m_bInitialized || !m_pDraw)
        return;

    m_pDraw->BeginUi();
    if (IsMapOpen())
        DrawMapLeftArt(w, h);
    if (!m_bMapFullscreen && !m_bExitConfirm)
    {
        DrawLogo(w, h);
        DrawButtons(w, h);
    }
    if (m_bMapFullscreen)
    {
        DrawMapHintBar(w, h);
        DrawMapLegend(w, h);
    }

    if (!IsMapOpen() && !m_bExitConfirm && m_pMainMenu && m_pMainMenu->IsInitialized())
        m_pMainMenu->RenderEmbeddedPanels();

    if (m_bExitConfirm)
        DrawExitConfirm(w, h);

    if (m_pDraw)
        m_pDraw->EndUi();

}
