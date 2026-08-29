/*****************************************************************************
 *
 *  PROJECT:     The-Definitive-UI
 *  FILE:        source/LanguageManager/LanguageManager.cpp
 *  PURPOSE:     UI / zone dictionaries (loaded from external files)
 *
 *****************************************************************************/

#include "LanguageManager.h"

#include "DictionaryStore.h"
#include "ModPaths.h"
#include "HelpGxt.h"

#include "plugin.h"
#include "C_PcSave.h"
#include "CMenuManager.h"
#include "Config.h"

#include <windows.h>
#include <cstdio>
#include <cstring>
#include <cctype>

LanguageManager::LanguageManager() = default;
LanguageManager::~LanguageManager() = default;

namespace
{
    LanguageManager::Lang g_lang = LanguageManager::Lang::Russian;

    DictionaryStore g_uiDict;
    DictionaryStore g_zoneDict;
    bool g_dictLoaded = false;

    void EnsureDictionariesLoaded()
    {
        if (g_dictLoaded)
            return;
        g_dictLoaded = true;

        char path[MAX_PATH]{};
        if (ModPaths::BuildDictionaryPath(path, sizeof(path), "ui.txt"))
            g_uiDict.LoadFile(path);
        if (ModPaths::BuildDictionaryPath(path, sizeof(path), "zones.txt"))
            g_zoneDict.LoadFile(path);
    }

    const char* LookupDict(const DictionaryStore& dict, const char* key, LanguageManager::Lang lang)
    {
        EnsureDictionariesLoaded();
        if (!key || !key[0])
            return nullptr;

        const int li = static_cast<int>(lang);
        if (li < 0 || li >= static_cast<int>(LanguageManager::Lang::Count))
            return nullptr;

        if (const char* s = dict.Get(key, li))
            return s;
        if (const char* s = dict.Get(key, static_cast<int>(LanguageManager::Lang::American)))
            return s;
        return nullptr;
    }

    void NormZoneKey(const char* in, char* out, size_t outChars)
    {
        if (!out || outChars == 0)
            return;
        out[0] = 0;
        if (!in)
            return;
        size_t n = 0;
        for (; in[n] && n + 1 < outChars && n < 8; ++n)
        {
            const unsigned char c = static_cast<unsigned char>(in[n]);
            if (c == 0 || c <= ' ')
                break;
            out[n] = static_cast<char>(std::toupper(c));
        }
        out[n] = 0;
    }

    const char* LookupZoneKey(const char* raw)
    {
        char key[16]{};
        NormZoneKey(raw, key, sizeof(key));
        if (!key[0])
            return nullptr;

        if (const char* s = LookupDict(g_zoneDict, key, g_lang))
            return s;

        int n = static_cast<int>(std::strlen(key));
        while (n > 2)
        {
            const unsigned char c = static_cast<unsigned char>(key[n - 1]);
            if ((c >= '0' && c <= '9') || (c >= 'A' && c <= 'C' && n > 3
                                           && key[n - 2] >= '0' && key[n - 2] <= '9'))
            {
                key[--n] = 0;
                if (const char* s = LookupDict(g_zoneDict, key, g_lang))
                    return s;
                continue;
            }
            break;
        }
        return nullptr;
    }

    const char* LangKey(LanguageManager::Lang lang)
    {
        switch (lang)
        {
        case LanguageManager::Lang::American: return "American";
        case LanguageManager::Lang::French:   return "French";
        case LanguageManager::Lang::German:   return "German";
        case LanguageManager::Lang::Italian:  return "Italian";
        case LanguageManager::Lang::Spanish:  return "Spanish";
        case LanguageManager::Lang::Russian:     return "Russian";
        case LanguageManager::Lang::Portuguese:  return "Portuguese";
        case LanguageManager::Lang::Brazilian:   return "Brazilian";
        default:                                 return "Russian";
        }
    }

    LanguageManager::Lang ParseLang(const char* s)
    {
        if (!s || !s[0])
            return LanguageManager::Lang::Russian;
        if (_stricmp(s, "American") == 0 || _stricmp(s, "English") == 0)
            return LanguageManager::Lang::American;
        if (_stricmp(s, "French") == 0)
            return LanguageManager::Lang::French;
        if (_stricmp(s, "German") == 0)
            return LanguageManager::Lang::German;
        if (_stricmp(s, "Italian") == 0)
            return LanguageManager::Lang::Italian;
        if (_stricmp(s, "Spanish") == 0)
            return LanguageManager::Lang::Spanish;
        if (_stricmp(s, "Russian") == 0)
            return LanguageManager::Lang::Russian;
        if (_stricmp(s, "Portuguese") == 0 || _stricmp(s, "PortuguesePT") == 0 || _stricmp(s, "pt") == 0)
            return LanguageManager::Lang::Portuguese;
        if (_stricmp(s, "Brazilian") == 0 || _stricmp(s, "PortugueseBR") == 0
            || _stricmp(s, "pt-BR") == 0 || _stricmp(s, "pt_BR") == 0)
            return LanguageManager::Lang::Brazilian;
        return LanguageManager::Lang::Russian;
    }

    struct UiFallbackEntry
    {
        const char* key;
        const char* text[static_cast<int>(LanguageManager::Lang::Count)];
    };

    const char* LookupBuiltinUi(const char* key, LanguageManager::Lang lang)
    {
        static const UiFallbackEntry kFallback[] = {
            { "SET_BLIP_EDGE_FADE", {
                "Blip Edge Fade", "Fondu des icônes au bord", "Icon-Ausblenden am Rand",
                "Dissolvenza icone al bordo", "Desvanecimiento en el borde",
                "Затухание иконок у края", "Desvanecer ícones na borda", "Desvanecer ícones na borda" } },
            { "SET_BLIP_ICON_SCALE", {
                "Blip Icon Size", "Taille des icônes", "Icon-Größe",
                "Dimensione icone", "Tamaño de iconos",
                "Размер иконок", "Tamanho dos ícones", "Tamanho dos ícones" } },
        };

        if (!key || !key[0])
            return nullptr;
        const int li = static_cast<int>(lang);
        if (li < 0 || li >= static_cast<int>(LanguageManager::Lang::Count))
            return nullptr;

        for (const UiFallbackEntry& e : kFallback)
        {
            if (strcmp(e.key, key) != 0)
                continue;
            if (e.text[li] && e.text[li][0])
                return e.text[li];
            return e.text[0];
        }
        return nullptr;
    }
}

LanguageManager::Lang LanguageManager::GetLanguage()
{
    return g_lang;
}

void LanguageManager::ApplyGameLanguage(Lang lang)
{
    char game = 0;
    switch (lang)
    {
    case Lang::American:
    case Lang::Russian:
    case Lang::Portuguese:
    case Lang::Brazilian: game = 0; break;
    case Lang::French:    game = 1; break;
    case Lang::German:    game = 2; break;
    case Lang::Italian:   game = 3; break;
    case Lang::Spanish:   game = 4; break;
    default:              game = 0; break;
    }
    if (FrontEndMenuManager.m_nPrefsLanguage != game)
    {
        FrontEndMenuManager.m_nPrefsLanguage = game;
        FrontEndMenuManager.InitialiseChangedLanguageSettings(true);
    }
}

void LanguageManager::SetLanguage(Lang lang, bool persist)
{
    if (lang < Lang::American || lang >= Lang::Count)
        lang = Lang::American;
    g_lang = lang;
    ApplyGameLanguage(lang);
    if (persist)
        RadarConfig::SetUiLanguage(LangKey(lang));
}

void LanguageManager::ApplySavedLanguage()
{
    SetLanguage(ParseLang(RadarConfig::GetUiLanguage()), false);
}

void LanguageManager::CycleLanguage(int dir)
{
    int n = static_cast<int>(g_lang) + (dir >= 0 ? 1 : -1);
    const int count = static_cast<int>(Lang::Count);
    if (n < 0) n = count - 1;
    if (n >= count) n = 0;
    SetLanguage(static_cast<Lang>(n));
}

const char* LanguageManager::Get(const char* key)
{
    if (const char* s = LookupDict(g_uiDict, key, g_lang))
        return s;
    if (const char* s = LookupBuiltinUi(key, g_lang))
        return s;
    if (const char* s = HelpGxt::Get(key))
        return s;
    return key ? key : "";
}

const char* LanguageManager::GetRadioStation(int stationId)
{
    const char* key = nullptr;
    switch (stationId)
    {
    case 1:  key = "FEA_R0";  break;
    case 2:  key = "FEA_R1";  break;
    case 3:  key = "FEA_R2";  break;
    case 4:  key = "FEA_R3";  break;
    case 5:  key = "FEA_R4";  break;
    case 6:  key = "FEA_R5";  break;
    case 7:  key = "FEA_R6";  break;
    case 8:  key = "FEA_R7";  break;
    case 9:  key = "FEA_R8";  break;
    case 10: key = "FEA_R9";  break;
    case 11: key = "FEA_R10"; break;
    case 12: key = "FEA_MP3"; break;
    case 13: key = "FEA_NON"; break;
    default: return "";
    }
    return Get(key);
}

const char* LanguageManager::GetZone(const char* zoneKey)
{
    if (const char* s = LookupZoneKey(zoneKey))
        return s;
    return Get("ZONE_DEFAULT");
}

const char* LanguageManager::LookupZone(const char* zoneKey)
{
    return LookupZoneKey(zoneKey);
}

const char* LanguageManager::GetLanguageName(Lang lang)
{
    switch (lang)
    {
    case Lang::American: return Get("LANG_AMERICAN");
    case Lang::French:   return Get("LANG_FRENCH");
    case Lang::German:   return Get("LANG_GERMAN");
    case Lang::Italian:  return Get("LANG_ITALIAN");
    case Lang::Spanish:     return Get("LANG_SPANISH");
    case Lang::Russian:     return Get("LANG_RUSSIAN");
    case Lang::Portuguese:  return Get("LANG_PORTUGUESE");
    case Lang::Brazilian:   return Get("LANG_BRAZILIAN");
    default:                return Get("LANG_AMERICAN");
    }
}
