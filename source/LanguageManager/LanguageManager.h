/*****************************************************************************
 *
 *  PROJECT:     The-Definitive-UI
 *  FILE:        source/LanguageManager/LanguageManager.h
 *  PURPOSE:     UI / zone dictionaries
 *
 *****************************************************************************/

#pragma once

#include <string>

class LanguageManager
{
public:
    // Matches settings list: American, French, German, Italian, Spanish, Russian
    enum class Lang : int
    {
        American = 0,
        French,
        German,
        Italian,
        Spanish,
        Russian,
        Count
    };

    LanguageManager();
    ~LanguageManager();

    static Lang GetLanguage();
    static void SetLanguage(Lang lang, bool persist = true);
    static void CycleLanguage(int dir); // -1 / +1
    static void ApplySavedLanguage();
    static int LanguageCount() { return static_cast<int>(Lang::Count); }

    // UI string by key (e.g. "UI_MAP", "MAP_HINTS"). Falls back to American, then key.
    static const char* Get(const char* key);
    // Radio HUD name for station id used by DisplayRadioStationName (1..13). UTF-8.
    static const char* GetRadioStation(int stationId);
    // Zone display name by CZone::m_szTextKey (e.g. "COM"). Uppercase. Fallback SAN_AND.
    static const char* GetZone(const char* zoneKey);
    // Same lookup without default (nullptr if unknown)
    static const char* LookupZone(const char* zoneKey);
    // Language name for settings cycle value
    static const char* GetLanguageName(Lang lang);

private:
    static void ApplyGameLanguage(Lang lang);
};
