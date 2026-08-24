/*****************************************************************************
 *
 *  PROJECT:     The-Definitive-UI
 *  FILE:        source/Config/Config.cpp
 *  PURPOSE:     Global plugin configuration (radar ini + future settings)
 *
 *****************************************************************************/

#include "Config.h"
#include "plugin.h"
#include "Patch.h"
#include "CPostEffects.h"
#include <Windows.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>

Config::Config()
{
}

Config::~Config()
{
}

namespace RadarConfig
{
    static const char* s_configFileName = "The-Definitive-UI.SA.ini";
    static std::string s_configPath;
    static std::map<std::string, std::string> s_values;

    static bool s_shapeCircle      = true;
    static bool s_showGangZones    = true;
    static bool s_radarRender      = true;
    static bool s_menuRender       = true;
    static bool s_deIcons          = true;
    static bool s_customRadarTxd   = true;
    static bool s_updatedHelp      = true;
    static bool s_radioText        = true;
    static bool s_gps              = true;
    static bool s_heatHaze         = true;
    static bool s_speedBlur        = true;
    static int  s_windowMode       = -1; // -1 unset (no patch), 0 windowed, 1 fullscreen, 2 borderless
    static int  s_windowWidth      = 0;
    static int  s_windowHeight     = 0;
    static int  s_colorDepth       = 0; // 0 unset, 16 or 32
    static std::string s_uiLanguage = "Russian";
    static std::string s_zoomKeyStr = "G";
    static int  s_zoomKeyVk        = 'G';
    static int  s_circleSize       = 265;
    static int  s_squareSizeX      = 265;
    static int  s_squareSizeY      = 265;
    static int  s_borderThickness  = 7;
    static int  s_offsetX          = 85;
    static int  s_offsetY          = 55;
    static int  s_backgroundColorR = 123;
    static int  s_backgroundColorG = 196;
    static int  s_backgroundColorB = 249;
    static int  s_backgroundColorA = 255;  // 255 = полностью непрозрачный
    static int  s_circleColorR = 255;
    static int  s_circleColorG = 255;
    static int  s_circleColorB = 255;
    static int  s_circleColorA = 255;
    static int  s_borderColorR = 0;
    static int  s_borderColorG = 0;
    static int  s_borderColorB = 0;
    static int  s_borderColorA = 255;

    static int CommentLangIndex()
    {
        const char* s = s_uiLanguage.c_str();
        if (_stricmp(s, "French") == 0)   return 1;
        if (_stricmp(s, "German") == 0)   return 2;
        if (_stricmp(s, "Italian") == 0)  return 3;
        if (_stricmp(s, "Spanish") == 0)  return 4;
        if (_stricmp(s, "Russian") == 0)  return 5;
        return 0; // American
    }

    static const char* GetDesc(const char* key)
    {
        // American, French, German, Italian, Spanish, Russian
        struct Row { const char* key; const char* t[6]; };
        static const Row kRows[] = {
            { "RadarRender", {
                "# Custom HUD radar: 1=on, 0=stock CHud::DrawRadar",
                "# Radar HUD perso: 1=oui, 0=CHud::DrawRadar d'origine",
                "# Eigenes HUD-Radar: 1=an, 0=Stock CHud::DrawRadar",
                "# Radar HUD personalizzato: 1=on, 0=CHud::DrawRadar originale",
                "# Radar HUD personal: 1=sí, 0=CHud::DrawRadar original",
                "# Кастомный радар HUD: 1=вкл, 0=сток CHud::DrawRadar"
            } },
            { "MenuRender", {
                "# Custom menus: 1=on, 0=stock frontend",
                "# Menus persos: 1=oui, 0=frontend d'origine",
                "# Eigene Menüs: 1=an, 0=Stock-Frontend",
                "# Menu personalizzati: 1=on, 0=frontend originale",
                "# Menús personalizados: 1=sí, 0=frontend original",
                "# Кастомные меню: 1=вкл, 0=сток frontend"
            } },
            { "Language", {
                "# UI language: American, French, German, Italian, Spanish, Russian, Portuguese, Brazilian",
                "# Langue de l'UI: American, French, German, Italian, Spanish, Russian, Portuguese, Brazilian",
                "# UI-Sprache: American, French, German, Italian, Spanish, Russian, Portuguese, Brazilian",
                "# Lingua UI: American, French, German, Italian, Spanish, Russian, Portuguese, Brazilian",
                "# Idioma de la UI: American, French, German, Italian, Spanish, Russian, Portuguese, Brazilian",
                "# Язык UI: American, French, German, Italian, Spanish, Russian, Portuguese, Brazilian"
            } },
            { "ZoomKey", {
                "# Radar camera zoom key, default G (latin letter A-Z)",
                "# Touche zoom caméra radar, défaut G (lettre latine A-Z)",
                "# Radar-Kamera-Zoom, Standard G (lateinischer Buchstabe A-Z)",
                "# Tasto zoom camera radar, predef. G (lettera latina A-Z)",
                "# Tecla zoom de cámara del radar, por defecto G (letra latina A-Z)",
                "# Зум камеры радара, по умолчанию G (латинская буква A-Z)"
            } },
            { "DeIcons", {
                "# DE radar/HUD icons: 1=Definitive pack, 0=stock hud.txd (56/57 always stock)",
                "# Icônes radar/HUD DE: 1=pack Definitive, 0=hud.txd d'origine (56/57 toujours stock)",
                "# DE-Radar-/HUD-Icons: 1=Definitive-Pack, 0=Stock hud.txd (56/57 immer Stock)",
                "# Icone radar/HUD DE: 1=pack Definitive, 0=hud.txd originale (56/57 sempre stock)",
                "# Iconos radar/HUD DE: 1=pack Definitive, 0=hud.txd original (56/57 siempre stock)",
                "# Иконки радара/HUD DE: 1=пак Definitive, 0=сток hud.txd (56/57 всегда сток)"
            } },
            { "CustomRadarTxd", {
                "# Radar map tiles: 1=custom map.txd, 0=stock radar00-radar143",
                "# Tuiles radar: 1=map.txd perso, 0=radar00-radar143 d'origine",
                "# Radar-Kacheln: 1=eigenes map.txd, 0=Stock radar00-radar143",
                "# Tessere radar: 1=map.txd personalizzato, 0=radar00-radar143 originali",
                "# Baldosas radar: 1=map.txd personal, 0=radar00-radar143 originales",
                "# Тайлы карты радара: 1=свой map.txd, 0=сток radar00-radar143"
            } },
            { "UpdatedHelp", {
                "# Updated HUD help: 1=Arial + dictionary, 0=stock CFont/GXT",
                "# Aide HUD actualisée: 1=Arial + dictionnaire, 0=CFont/GXT d'origine",
                "# Aktualisierte HUD-Hinweise: 1=Arial + Wörterbuch, 0=Stock CFont/GXT",
                "# Suggerimenti HUD aggiornati: 1=Arial + dizionario, 0=CFont/GXT originale",
                "# Consejos HUD actualizados: 1=Arial + diccionario, 0=CFont/GXT original",
                "# Обновленные подсказки HUD: 1=Arial + словарь, 0=сток CFont/GXT"
            } },
            { "RadioText", {
                "# Radio station name HUD: 1=custom Arial, 0=stock CFont",
                "# Nom de radio HUD: 1=Arial perso, 0=CFont d'origine",
                "# Radio-Stationsname HUD: 1=eigenes Arial, 0=Stock CFont",
                "# Nome stazione radio HUD: 1=Arial personalizzato, 0=CFont originale",
                "# Nombre de radio HUD: 1=Arial personal, 0=CFont original",
                "# Название радиостанции HUD: 1=свой Arial, 0=сток CFont"
            } },
            { "GPS", {
                "# GPS route line on radar and pause map: 1=on, 0=off",
                "# Itinéraire GPS sur le radar et la carte pause: 1=oui, 0=non",
                "# GPS-Route auf Radar und Pausenkarte: 1=an, 0=aus",
                "# Percorso GPS su radar e mappa pausa: 1=on, 0=off",
                "# Ruta GPS en radar y mapa de pausa: 1=sí, 0=no",
                "# Линия GPS на радаре и карте паузы: 1=вкл, 0=выкл"
            } },
            { "HeatHaze", {
                "# Heat haze (CPostEffects::HeatHazeFX): 1=on, 0=off",
                "# Effet de chaleur (CPostEffects::HeatHazeFX): 1=oui, 0=non",
                "# Hitzeflimmern (CPostEffects::HeatHazeFX): 1=an, 0=aus",
                "# Foschia di calore (CPostEffects::HeatHazeFX): 1=on, 0=off",
                "# Efecto de calor (CPostEffects::HeatHazeFX): 1=sí, 0=no",
                "# Эффект жары (CPostEffects::HeatHazeFX): 1=вкл, 0=выкл"
            } },
            { "SpeedBlur", {
                "# Speed blur (CPostEffects::SpeedFX): 1=on, 0=off",
                "# Flou de vitesse (CPostEffects::SpeedFX): 1=oui, 0=non",
                "# Geschwindigkeitsunschärfe (CPostEffects::SpeedFX): 1=an, 0=aus",
                "# Sfocatura velocità (CPostEffects::SpeedFX): 1=on, 0=off",
                "# Desenfoque de velocidad (CPostEffects::SpeedFX): 1=sí, 0=no",
                "# Размытие на скорости (CPostEffects::SpeedFX): 1=вкл, 0=выкл"
            } },
            { "WindowMode", {
                "# Window mode: empty=stock (no patch), 0=windowed, 1=fullscreen, 2=borderless",
                "# Mode fenêtre: vide=stock (sans patch), 0=fenêtré, 1=plein écran, 2=sans bord",
                "# Fenstermodus: leer=Stock (kein Patch), 0=Fenster, 1=Vollbild, 2=rahmenlos",
                "# Modalità finestra: vuoto=stock (nessun patch), 0=finestra, 1=schermo intero, 2=senza bordo",
                "# Modo de ventana: vacío=stock (sin parche), 0=ventana, 1=pantalla completa, 2=sin marco",
                "# Режим окна: пусто=сток (без патча), 0=оконный, 1=полный экран, 2=без рамки"
            } },
            { "WindowWidth", {
                "# Windowed/borderless backbuffer width (used with WindowMode)",
                "# Largeur du backbuffer fenêtré (avec WindowMode)",
                "# Backbuffer-Breite im Fenster (mit WindowMode)",
                "# Larghezza backbuffer in finestra (con WindowMode)",
                "# Ancho del backbuffer en ventana (con WindowMode)",
                "# Ширина backbuffer в оконном режиме (вместе с WindowMode)"
            } },
            { "WindowHeight", {
                "# Windowed/borderless backbuffer height (used with WindowMode)",
                "# Hauteur du backbuffer fenêtré (avec WindowMode)",
                "# Backbuffer-Höhe im Fenster (mit WindowMode)",
                "# Altezza backbuffer in finestra (con WindowMode)",
                "# Alto del backbuffer en ventana (con WindowMode)",
                "# Высота backbuffer в оконном режиме (вместе с WindowMode)"
            } },
            { "ColorDepth", {
                "# Color depth: 16 or 32 (bits). Empty = first listed mode",
                "# Profondeur couleur: 16 ou 32. Vide = premier mode listé",
                "# Farbtiefe: 16 oder 32. Leer = erster Listeneintrag",
                "# Profondità colore: 16 o 32. Vuoto = primo modo in elenco",
                "# Profundidad de color: 16 o 32. Vacío = primer modo de la lista",
                "# Разрядность цвета: 16 или 32. Пусто = первый слот в списке"
            } },
            { "Shape", {
                "# Shape: 1=circle, 0=square",
                "# Forme: 1=cercle, 0=carré",
                "# Form: 1=Kreis, 0=Quadrat",
                "# Forma: 1=cerchio, 0=quadrato",
                "# Forma: 1=círculo, 0=cuadrado",
                "# Форма: 1=круг, 0=квадрат"
            } },
            { "ShowGangZones", {
                "# Show gang zones: 1=yes, 0=no",
                "# Zones de gangs: 1=oui, 0=non",
                "# Ganggebiete anzeigen: 1=ja, 0=nein",
                "# Zone gang: 1=sì, 0=no",
                "# Zonas de bandas: 1=sí, 0=no",
                "# Показать зоны банд: 1=да, 0=нет"
            } },
            { "CircleSize", {
                "# Circle radar size (50-800)",
                "# Taille du radar rond (50-800)",
                "# Größe rundes Radar (50-800)",
                "# Dimensione radar tondo (50-800)",
                "# Tamaño del radar circular (50-800)",
                "# Размер круглого радара (50-800)"
            } },
            { "SquareSizeX", {
                "# Square radar width (50-800)",
                "# Largeur du radar carré (50-800)",
                "# Breite quadratisches Radar (50-800)",
                "# Larghezza radar quadrato (50-800)",
                "# Ancho del radar cuadrado (50-800)",
                "# Ширина квадратного радара (50-800)"
            } },
            { "SquareSizeY", {
                "# Square radar height (50-800)",
                "# Hauteur du radar carré (50-800)",
                "# Höhe quadratisches Radar (50-800)",
                "# Altezza radar quadrato (50-800)",
                "# Alto del radar cuadrado (50-800)",
                "# Высота квадратного радара (50-800)"
            } },
            { "BorderThickness", {
                "# Border thickness (1-50)",
                "# Épaisseur du cadre (1-50)",
                "# Rahmenstärke (1-50)",
                "# Spessore bordo (1-50)",
                "# Grosor del borde (1-50)",
                "# Толщина рамки (1-50)"
            } },
            { "OffsetX", {
                "# Offset from left edge (Full HD: 85)",
                "# Marge depuis le bord gauche (Full HD: 85)",
                "# Abstand vom linken Rand (Full HD: 85)",
                "# Offset dal bordo sinistro (Full HD: 85)",
                "# Margen desde el borde izquierdo (Full HD: 85)",
                "# Отступ от левого края (Full HD: 85)"
            } },
            { "OffsetY", {
                "# Offset from bottom edge (Full HD: 55)",
                "# Marge depuis le bord bas (Full HD: 55)",
                "# Abstand vom unteren Rand (Full HD: 55)",
                "# Offset dal bordo inferiore (Full HD: 55)",
                "# Margen desde el borde inferior (Full HD: 55)",
                "# Отступ от нижнего края (Full HD: 55)"
            } },
            { "BackgroundColor", {
                "# Radar background color RGBA (default: 123, 196, 249, 255 — light blue opaque)",
                "# Couleur de fond du radar RGBA (défaut: 123, 196, 249, 255 — bleu clair opaque)",
                "# Radar-Hintergrundfarbe RGBA (Standard: 123, 196, 249, 255 — hellblau undurchsichtig)",
                "# Colore sfondo radar RGBA (predef.: 123, 196, 249, 255 — azzurro opaco)",
                "# Color de fondo del radar RGBA (por defecto: 123, 196, 249, 255 — azul claro opaco)",
                "# Фоновый цвет радара RGBA (по умолчанию: 123, 196, 249, 255 — голубой непрозрачный)"
            } },
            { "CircleColor", {
                "# Radar circle/square color RGBA (default: 255, 255, 255, 255 — white opaque)",
                "# Couleur cercle/carré RGBA (défaut: 255, 255, 255, 255 — blanc opaque)",
                "# Kreis-/Quadratfarbe RGBA (Standard: 255, 255, 255, 255 — weiß undurchsichtig)",
                "# Colore cerchio/quadrato RGBA (predef.: 255, 255, 255, 255 — bianco opaco)",
                "# Color círculo/cuadrado RGBA (por defecto: 255, 255, 255, 255 — blanco opaco)",
                "# Цвет круга/квадрата радара RGBA (по умолчанию: 255, 255, 255, 255 — белый непрозрачный)"
            } },
            { "BorderColor", {
                "# Radar border color RGBA (default: 0, 0, 0, 255 — black opaque)",
                "# Couleur du cadre RGBA (défaut: 0, 0, 0, 255 — noir opaque)",
                "# Rahmenfarbe RGBA (Standard: 0, 0, 0, 255 — schwarz undurchsichtig)",
                "# Colore bordo RGBA (predef.: 0, 0, 0, 255 — nero opaco)",
                "# Color del borde RGBA (por defecto: 0, 0, 0, 255 — negro opaco)",
                "# Цвет обводки радара RGBA (по умолчанию: 0, 0, 0, 255 — чёрный непрозрачный)"
            } },
        };
        const int li = CommentLangIndex();
        for (const auto& row : kRows)
        {
            if (strcmp(row.key, key) == 0)
            {
                const char* s = row.t[li];
                return (s && s[0]) ? s : row.t[0];
            }
        }
        return "";
    }

    static void BuildConfigPath()
    {
        if (!s_configPath.empty())
            return;

        if (const char* p = PLUGIN_PATH(s_configFileName))
            s_configPath = p;
    }

    static void Trim(std::string& s)
    {
        size_t start = s.find_first_not_of(" \t\r\n");
        if (start == std::string::npos)
        {
            s.clear();
            return;
        }
        size_t end = s.find_last_not_of(" \t\r\n");
        s = s.substr(start, end - start + 1);
    }

    static int ParseZoomVk(const std::string& raw, std::string& outStr)
    {
        if (raw.size() == 1)
        {
            unsigned char c = static_cast<unsigned char>(raw[0]);
            if (c >= 'a' && c <= 'z')
                c = static_cast<unsigned char>(c - 32);
            if (c >= 'A' && c <= 'Z')
            {
                outStr.assign(1, static_cast<char>(c));
                return static_cast<int>(c);
            }
        }
        outStr = "G";
        return 'G';
    }

    // MTA: RET (C3) at fn start when off; restore the real first opcode when on.
    static constexpr uintptr_t kHeatHazeFX = 0x701780; // CPostEffects::HeatHazeFX
    static constexpr uintptr_t kSpeedFX    = 0x7030A0; // CPostEffects::SpeedFX
    static unsigned char s_heatHazeOrig = 0x83;
    static unsigned char s_speedFxOrig  = 0x83;
    static bool s_fxOrigSaved = false;

    static void SaveFxOrigBytes()
    {
        if (s_fxOrigSaved)
            return;
        s_heatHazeOrig = plugin::patch::GetUChar(kHeatHazeFX);
        s_speedFxOrig  = plugin::patch::GetUChar(kSpeedFX);
        if (s_heatHazeOrig == 0xC3)
            s_heatHazeOrig = 0x83;
        if (s_speedFxOrig == 0xC3)
            s_speedFxOrig = 0x83;
        s_fxOrigSaved = true;
    }

    static void ApplyFxPatches()
    {
        SaveFxOrigBytes();
        plugin::patch::SetUChar(kHeatHazeFX, s_heatHaze ? s_heatHazeOrig : 0xC3);
        plugin::patch::SetUChar(kSpeedFX, s_speedBlur ? s_speedFxOrig : 0xC3);
        CPostEffects::m_bSpeedFX = s_speedBlur;
    }

    static void WriteConfigFile(FILE* f)
    {
        fprintf(f, "# The-Definitive-UI.SA.ini\n\n");
        fprintf(f, "%s\nRadarRender = %d\n\n", GetDesc("RadarRender"), s_radarRender ? 1 : 0);
        fprintf(f, "%s\nMenuRender = %d\n\n", GetDesc("MenuRender"), s_menuRender ? 1 : 0);
        fprintf(f, "%s\nLanguage = %s\n\n", GetDesc("Language"), s_uiLanguage.c_str());
        fprintf(f, "%s\nZoomKey = %s\n\n", GetDesc("ZoomKey"), s_zoomKeyStr.c_str());
        fprintf(f, "%s\nDeIcons = %d\n\n", GetDesc("DeIcons"), s_deIcons ? 1 : 0);
        fprintf(f, "%s\nCustomRadarTxd = %d\n\n", GetDesc("CustomRadarTxd"), s_customRadarTxd ? 1 : 0);
        fprintf(f, "%s\nUpdatedHelp = %d\n\n", GetDesc("UpdatedHelp"), s_updatedHelp ? 1 : 0);
        fprintf(f, "%s\nRadioText = %d\n\n", GetDesc("RadioText"), s_radioText ? 1 : 0);
        fprintf(f, "%s\nGPS = %d\n\n", GetDesc("GPS"), s_gps ? 1 : 0);
        fprintf(f, "%s\nHeatHaze = %d\n\n", GetDesc("HeatHaze"), s_heatHaze ? 1 : 0);
        fprintf(f, "%s\nSpeedBlur = %d\n\n", GetDesc("SpeedBlur"), s_speedBlur ? 1 : 0);
        if (s_windowMode >= 0)
        {
            fprintf(f, "%s\nWindowMode = %d\n\n", GetDesc("WindowMode"), s_windowMode);
            fprintf(f, "%s\nWindowWidth = %d\n\n", GetDesc("WindowWidth"), s_windowWidth);
            fprintf(f, "%s\nWindowHeight = %d\n\n", GetDesc("WindowHeight"), s_windowHeight);
        }
        else
        {
            fprintf(f, "%s\nWindowMode =\n\n", GetDesc("WindowMode"));
            fprintf(f, "%s\nWindowWidth =\n\n", GetDesc("WindowWidth"));
            fprintf(f, "%s\nWindowHeight =\n\n", GetDesc("WindowHeight"));
        }
        if (s_colorDepth == 16 || s_colorDepth == 32)
            fprintf(f, "%s\nColorDepth = %d\n\n", GetDesc("ColorDepth"), s_colorDepth);
        else
            fprintf(f, "%s\nColorDepth =\n\n", GetDesc("ColorDepth"));
        fprintf(f, "%s\nShape = %d\n\n", GetDesc("Shape"), s_shapeCircle ? 1 : 0);
        fprintf(f, "%s\nShowGangZones = %d\n\n", GetDesc("ShowGangZones"), s_showGangZones ? 1 : 0);
        fprintf(f, "%s\nCircleSize = %d\n\n", GetDesc("CircleSize"), s_circleSize);
        fprintf(f, "%s\nSquareSizeX = %d\n\n", GetDesc("SquareSizeX"), s_squareSizeX);
        fprintf(f, "%s\nSquareSizeY = %d\n\n", GetDesc("SquareSizeY"), s_squareSizeY);
        fprintf(f, "%s\nBorderThickness = %d\n\n", GetDesc("BorderThickness"), s_borderThickness);
        fprintf(f, "%s\nOffsetX = %d\n\n", GetDesc("OffsetX"), s_offsetX);
        fprintf(f, "%s\nOffsetY = %d\n\n", GetDesc("OffsetY"), s_offsetY);
        fprintf(f, "%s\nBackgroundColor = %d, %d, %d, %d\n\n",
            GetDesc("BackgroundColor"),
            s_backgroundColorR, s_backgroundColorG, s_backgroundColorB, s_backgroundColorA);
        fprintf(f, "%s\nCircleColor = %d, %d, %d, %d\n\n",
            GetDesc("CircleColor"),
            s_circleColorR, s_circleColorG, s_circleColorB, s_circleColorA);
        fprintf(f, "%s\nBorderColor = %d, %d, %d, %d\n\n",
            GetDesc("BorderColor"),
            s_borderColorR, s_borderColorG, s_borderColorB, s_borderColorA);
    }

    static bool CreateDefaultConfig()
    {
        if (s_configPath.empty())
            return false;

        FILE* f = nullptr;
        if (fopen_s(&f, s_configPath.c_str(), "w") != 0 || !f)
            return false;

        WriteConfigFile(f);

        fclose(f);
        return true;
    }

    static void ParseLine(const std::string& line)
    {
        if (line.empty() || line[0] == '#' || line[0] == ';')
            return;

        size_t eq = line.find('=');
        if (eq == std::string::npos)
            return;

        std::string key = line.substr(0, eq);
        std::string val = line.substr(eq + 1);
        Trim(key);
        Trim(val);
        if (!key.empty())
            s_values[key] = val;
    }

    static void ApplyParsedValues()
    {
        auto         it = s_values.find("Shape");
        if (it != s_values.end())
            s_shapeCircle = (atoi(it->second.c_str()) != 0);

        it = s_values.find("ShowGangZones");
        if (it != s_values.end())
            s_showGangZones = (atoi(it->second.c_str()) != 0);

        it = s_values.find("RadarRender");
        if (it != s_values.end())
            s_radarRender = (atoi(it->second.c_str()) != 0);

        it = s_values.find("MenuRender");
        if (it != s_values.end())
            s_menuRender = (atoi(it->second.c_str()) != 0);

        it = s_values.find("Language");
        if (it != s_values.end() && !it->second.empty())
            s_uiLanguage = it->second;

        it = s_values.find("ZoomKey");
        if (it != s_values.end())
            s_zoomKeyVk = ParseZoomVk(it->second, s_zoomKeyStr);

        it = s_values.find("DeIcons");
        if (it != s_values.end())
            s_deIcons = (atoi(it->second.c_str()) != 0);

        it = s_values.find("CustomRadarTxd");
        if (it != s_values.end())
            s_customRadarTxd = (atoi(it->second.c_str()) != 0);

        it = s_values.find("UpdatedHelp");
        if (it != s_values.end())
            s_updatedHelp = (atoi(it->second.c_str()) != 0);

        it = s_values.find("RadioText");
        if (it != s_values.end())
            s_radioText = (atoi(it->second.c_str()) != 0);

        it = s_values.find("GPS");
        if (it != s_values.end())
            s_gps = (atoi(it->second.c_str()) != 0);

        it = s_values.find("HeatHaze");
        if (it != s_values.end())
            s_heatHaze = (atoi(it->second.c_str()) != 0);

        it = s_values.find("SpeedBlur");
        if (it != s_values.end())
            s_speedBlur = (atoi(it->second.c_str()) != 0);

        it = s_values.find("WindowMode");
        if (it != s_values.end())
        {
            const char* raw = it->second.c_str();
            if (!raw[0] || _stricmp(raw, "NULL") == 0)
                s_windowMode = -1;
            else
            {
                int v = atoi(raw);
                if (v >= 0 && v <= 2)
                    s_windowMode = v;
            }
        }

        it = s_values.find("WindowWidth");
        if (it != s_values.end() && it->second[0])
        {
            int v = atoi(it->second.c_str());
            if (v >= 640 && v <= 16384)
                s_windowWidth = v;
        }
        it = s_values.find("WindowHeight");
        if (it != s_values.end() && it->second[0])
        {
            int v = atoi(it->second.c_str());
            if (v >= 480 && v <= 16384)
                s_windowHeight = v;
        }
        it = s_values.find("ColorDepth");
        if (it != s_values.end() && it->second[0])
        {
            int v = atoi(it->second.c_str());
            if (v == 16 || v == 32)
                s_colorDepth = v;
        }

        it = s_values.find("CircleSize");
        if (it != s_values.end())
        {
            int v = atoi(it->second.c_str());
            if (v >= 50 && v <= 800)
                s_circleSize = v;
        }

        it = s_values.find("SquareSizeX");
        if (it != s_values.end())
        {
            int v = atoi(it->second.c_str());
            if (v >= 50 && v <= 800)
                s_squareSizeX = v;
        }

        it = s_values.find("SquareSizeY");
        if (it != s_values.end())
        {
            int v = atoi(it->second.c_str());
            if (v >= 50 && v <= 800)
                s_squareSizeY = v;
        }

        it = s_values.find("BorderThickness");
        if (it != s_values.end())
        {
            int v = atoi(it->second.c_str());
            if (v >= 1 && v <= 50)
                s_borderThickness = v;
        }

        it = s_values.find("OffsetX");
        if (it != s_values.end())
        {
            int v = atoi(it->second.c_str());
            if (v >= 0 && v <= 1000)
                s_offsetX = v;
        }

        it = s_values.find("OffsetY");
        if (it != s_values.end())
        {
            int v = atoi(it->second.c_str());
            if (v >= 0 && v <= 1000)
                s_offsetY = v;
        }

        // Parse BackgroundColor (format: "R, G, B" or "R, G, B, A")
        it = s_values.find("BackgroundColor");
        if (it != s_values.end())
        {
            std::string colorStr = it->second;
            // Remove spaces
            for (size_t i = 0; i < colorStr.length(); )
            {
                if (colorStr[i] == ' ')
                    colorStr.erase(i, 1);
                else
                    ++i;
            }
            // Parse R,G,B[,A]
            size_t c1 = colorStr.find(',');
            size_t c2 = colorStr.find(',', c1 + 1);
            if (c1 != std::string::npos && c2 != std::string::npos)
            {
                int r = atoi(colorStr.substr(0, c1).c_str());
                int g = atoi(colorStr.substr(c1 + 1, c2 - c1 - 1).c_str());
                size_t c3 = colorStr.find(',', c2 + 1);
                int b = 0, a = 255;  // default alpha = 255 (opaque)
                
                if (c3 != std::string::npos)
                {
                    // RGBA format
                    b = atoi(colorStr.substr(c2 + 1, c3 - c2 - 1).c_str());
                    a = atoi(colorStr.substr(c3 + 1).c_str());
                }
                else
                {
                    // RGB format
                    b = atoi(colorStr.substr(c2 + 1).c_str());
                }
                
                if (r >= 0 && r <= 255 && g >= 0 && g <= 255 && b >= 0 && b <= 255 && a >= 0 && a <= 255)
                {
                    s_backgroundColorR = r;
                    s_backgroundColorG = g;
                    s_backgroundColorB = b;
                    s_backgroundColorA = a;
                }
            }
        }

        // Parse CircleColor (format: "R, G, B" or "R, G, B, A")
        it = s_values.find("CircleColor");
        if (it != s_values.end())
        {
            std::string colorStr = it->second;
            for (size_t i = 0; i < colorStr.length(); )
            {
                if (colorStr[i] == ' ')
                    colorStr.erase(i, 1);
                else
                    ++i;
            }
            size_t c1 = colorStr.find(',');
            size_t c2 = colorStr.find(',', c1 + 1);
            if (c1 != std::string::npos && c2 != std::string::npos)
            {
                int r = atoi(colorStr.substr(0, c1).c_str());
                int g = atoi(colorStr.substr(c1 + 1, c2 - c1 - 1).c_str());
                size_t c3 = colorStr.find(',', c2 + 1);
                int b = 0, a = 255;

                if (c3 != std::string::npos)
                {
                    b = atoi(colorStr.substr(c2 + 1, c3 - c2 - 1).c_str());
                    a = atoi(colorStr.substr(c3 + 1).c_str());
                }
                else
                {
                    b = atoi(colorStr.substr(c2 + 1).c_str());
                }

                if (r >= 0 && r <= 255 && g >= 0 && g <= 255 && b >= 0 && b <= 255 && a >= 0 && a <= 255)
                {
                    s_circleColorR = r;
                    s_circleColorG = g;
                    s_circleColorB = b;
                    s_circleColorA = a;
                }
            }
        }

        // Parse BorderColor (format: "R, G, B" or "R, G, B, A")
        it = s_values.find("BorderColor");
        if (it != s_values.end())
        {
            std::string colorStr = it->second;
            for (size_t i = 0; i < colorStr.length(); )
            {
                if (colorStr[i] == ' ')
                    colorStr.erase(i, 1);
                else
                    ++i;
            }
            size_t c1 = colorStr.find(',');
            size_t c2 = colorStr.find(',', c1 + 1);
            if (c1 != std::string::npos && c2 != std::string::npos)
            {
                int r = atoi(colorStr.substr(0, c1).c_str());
                int g = atoi(colorStr.substr(c1 + 1, c2 - c1 - 1).c_str());
                size_t c3 = colorStr.find(',', c2 + 1);
                int b = 0, a = 255;

                if (c3 != std::string::npos)
                {
                    b = atoi(colorStr.substr(c2 + 1, c3 - c2 - 1).c_str());
                    a = atoi(colorStr.substr(c3 + 1).c_str());
                }
                else
                {
                    b = atoi(colorStr.substr(c2 + 1).c_str());
                }

                if (r >= 0 && r <= 255 && g >= 0 && g <= 255 && b >= 0 && b <= 255 && a >= 0 && a <= 255)
                {
                    s_borderColorR = r;
                    s_borderColorG = g;
                    s_borderColorB = b;
                    s_borderColorA = a;
                }
            }
        }
    }

    void Load()
    {
        BuildConfigPath();
        s_values.clear();

        if (s_configPath.empty())
            return;

        FILE* f = nullptr;
        bool exists = (fopen_s(&f, s_configPath.c_str(), "r") == 0 && f);
        if (f)
            fclose(f), f = nullptr;

        if (!exists)
        {
            CreateDefaultConfig();
            if (fopen_s(&f, s_configPath.c_str(), "r") != 0 || !f)
                return;
        }
        else if (fopen_s(&f, s_configPath.c_str(), "r") != 0 || !f)
            return;

        char line[512];
        while (fgets(line, sizeof(line), f))
        {
            std::string s(line);
            Trim(s);
            if (!s.empty() && s[0] == '[')
                continue;
            ParseLine(s);
        }
        fclose(f);

        ApplyParsedValues();
        ApplyFxPatches();
        Save();
    }

    void Save()
    {
        if (s_configPath.empty())
            return;

        FILE* f = nullptr;
        if (fopen_s(&f, s_configPath.c_str(), "w") != 0 || !f)
            return;

        WriteConfigFile(f);

        fclose(f);
    }

    const char* GetConfigPath()
    {
        BuildConfigPath();
        return s_configPath.c_str();
    }

    bool GetShapeCircle() { return s_shapeCircle; }
    bool GetShowGangZones() { return s_showGangZones; }
    bool GetRadarRender() { return s_radarRender; }
    bool GetMenuRender() { return s_menuRender; }
    bool GetDeIcons() { return s_deIcons; }
    bool GetCustomRadarTxd() { return s_customRadarTxd; }
    bool GetUpdatedHelp() { return s_updatedHelp; }
    bool GetRadioText() { return s_radioText; }
    bool GetGps() { return s_gps; }
    bool GetHeatHaze() { return s_heatHaze; }
    bool GetSpeedBlur() { return s_speedBlur; }
    int  GetWindowMode() { return (s_windowMode >= 0) ? s_windowMode : 1; }
    bool HasWindowModeOverride() { return s_windowMode >= 0; }
    int  GetWindowWidth() { return s_windowWidth; }
    int  GetWindowHeight() { return s_windowHeight; }
    int  GetColorDepth() { return s_colorDepth; }
    bool HasWindowResolution() { return s_windowWidth >= 640 && s_windowHeight >= 480; }
    bool HasColorDepth() { return s_colorDepth == 16 || s_colorDepth == 32; }
    const char* GetUiLanguage() { return s_uiLanguage.c_str(); }
    int  GetZoomKeyVk() { return s_zoomKeyVk; }
    int  GetCircleSize() { return s_circleSize; }
    int  GetSquareSizeX() { return s_squareSizeX; }
    int  GetSquareSizeY() { return s_squareSizeY; }
    int  GetBorderThickness() { return s_borderThickness; }
    int  GetOffsetX() { return s_offsetX; }
    int  GetOffsetY() { return s_offsetY; }
    
    void GetBackgroundColor(int& outR, int& outG, int& outB)
    {
        outR = s_backgroundColorR;
        outG = s_backgroundColorG;
        outB = s_backgroundColorB;
    }
    
    void GetBackgroundColor(int& outR, int& outG, int& outB, int& outA)
    {
        outR = s_backgroundColorR;
        outG = s_backgroundColorG;
        outB = s_backgroundColorB;
        outA = s_backgroundColorA;
    }

    void GetCircleColor(int& outR, int& outG, int& outB, int& outA)
    {
        outR = s_circleColorR;
        outG = s_circleColorG;
        outB = s_circleColorB;
        outA = s_circleColorA;
    }

    void GetBorderColor(int& outR, int& outG, int& outB, int& outA)
    {
        outR = s_borderColorR;
        outG = s_borderColorG;
        outB = s_borderColorB;
        outA = s_borderColorA;
    }

    void SetShapeCircle(bool useCircle) { s_shapeCircle = useCircle; }
    void SetShowGangZones(bool value)
    {
        s_showGangZones = value;
        Save();
    }
    void SetDeIcons(bool value)
    {
        s_deIcons = value;
        Save();
    }
    void SetCustomRadarTxd(bool value)
    {
        s_customRadarTxd = value;
        Save();
    }
    void SetUpdatedHelp(bool value)
    {
        s_updatedHelp = value;
        Save();
    }
    void SetRadioText(bool value)
    {
        s_radioText = value;
        Save();
    }
    void SetGps(bool value)
    {
        s_gps = value;
        Save();
    }
    void SetHeatHaze(bool value)
    {
        s_heatHaze = value;
        ApplyFxPatches();
        Save();
    }
    void SetSpeedBlur(bool value)
    {
        s_speedBlur = value;
        ApplyFxPatches();
        Save();
    }
    void SetWindowMode(int value)
    {
        if (value < 0) value = 0;
        if (value > 2) value = 2;
        if (s_windowMode == value)
            return;
        s_windowMode = value;
        Save();
    }
    void ClearWindowMode()
    {
        s_windowMode = -1;
        s_windowWidth = 0;
        s_windowHeight = 0;
        s_colorDepth = 0;
        Save();
    }
    void SetWindowResolution(int width, int height, int depth)
    {
        if (width < 640 || height < 480)
            return;
        int d = s_colorDepth;
        if (depth == 16 || depth == 32)
            d = depth;
        if (s_windowWidth == width && s_windowHeight == height && s_colorDepth == d)
            return;
        s_windowWidth = width;
        s_windowHeight = height;
        s_colorDepth = d;
        Save();
    }
    void SetColorDepth(int depth)
    {
        if (depth != 16 && depth != 32)
            return;
        if (s_colorDepth == depth)
            return;
        s_colorDepth = depth;
        Save();
    }
    void SetUiLanguage(const char* name)
    {
        if (!name || !name[0])
            return;
        s_uiLanguage = name;
        Save();
    }
    void SetCircleSize(int value)
    {
        if (value >= 50 && value <= 800)
            s_circleSize = value;
    }
    void SetSquareSizeX(int value)
    {
        if (value >= 50 && value <= 800)
            s_squareSizeX = value;
    }
    void SetSquareSizeY(int value)
    {
        if (value >= 50 && value <= 800)
            s_squareSizeY = value;
    }
    void SetBorderThickness(int value)
    {
        if (value >= 1 && value <= 50)
            s_borderThickness = value;
    }
    void SetOffsetX(int value)
    {
        if (value >= 0 && value <= 1000)
            s_offsetX = value;
    }
    void SetOffsetY(int value)
    {
        if (value >= 0 && value <= 1000)
            s_offsetY = value;
    }
}
