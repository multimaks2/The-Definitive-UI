/*****************************************************************************
 *
 *  PROJECT:     The-Definitive-UI
 *  FILE:        source/SaveSlots/SaveSlots.h
 *  PURPOSE:     Save-slot list + 1C / SanLTD / CP1251 title decoding
 *
 *****************************************************************************/

#pragma once

#include <string>
#include <windows.h>

class SaveSlots
{
public:
    static constexpr int kCount = 8;

    struct Slot
    {
        bool        empty = true;
        bool        corrupted = false;
        std::string name;
        std::string date;
    };

    void Refresh();
    void Ensure();
    bool HasAny() const;
    int  FindMostRecent() const; // -1 if none

    const Slot& Get(int i) const { return m_slots[i]; }
    Slot&       Get(int i) { return m_slots[i]; }

    enum class GxtCodec : int
    {
        Latin1252 = 0,
        OneC,
        SanLtd
    };

    static std::string DecodeSaveText(const char* raw);
    static std::string DecodeHudText(const char* gxt);
    // File-level GXT decode: keep ~k~~TOKEN~ / ~1~ / ~n~. Codec is per-file, not per-draw.
    static GxtCodec DetectGxtCodec(const char* sample);
    static std::string DecodeGxtKeepTokens(const char* raw, GxtCodec codec);
    // SanLTD key labels mix 0x80–0xAF Cyrillic with ASCII (CTRL). Do not remap Latin.
    static std::string DecodeSanLtdHighBytes(const char* raw);
    static std::string AcpToUtf8(const char* sz);
    static std::string FormatSaveDateRu(int day, int month, int year, int hour, int minute, int second);
    static bool ReadSaveSlotMeta(int slot, std::string& outMissionKey,
                                 int& day, int& month, int& year,
                                 int& hour, int& minute, int& second);
    static int  ScoreDecodedText(const std::string& s);
    static bool HasHighBytes(const char* sz);
    static bool LooksLike1cGxt(const char* sz);

private:
    Slot  m_slots[kCount]{};
    bool  m_loaded = false;
    DWORD m_refreshAt = 0;
};
