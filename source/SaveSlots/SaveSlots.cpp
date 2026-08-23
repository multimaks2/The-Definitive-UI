/*****************************************************************************
 *
 *  PROJECT:     The-Definitive-UI
 *  FILE:        source/SaveSlots/SaveSlots.cpp
 *  PURPOSE:     Save-slot list + 1C / SanLTD / CP1251 title decoding
 *
 *****************************************************************************/

#include "SaveSlots.h"

#include "LanguageManager.h"
#include "plugin.h"
#include "C_PcSave.h"
#include "CGenericGameStorage.h"
#include "CText.h"

#include <windows.h>
#include <cstdio>
#include <cstring>
#include <vector>

namespace
{
    std::string WideToUtf8(const wchar_t* wide)
    {
        if (!wide || !wide[0])
            return {};
        char utf8[1024];
        if (WideCharToMultiByte(CP_UTF8, 0, wide, -1, utf8, 1024, nullptr, nullptr) <= 0)
            return {};
        return utf8;
    }

    std::string BytesToUtf8(const char* sz, UINT cp, DWORD flags = 0)
    {
        if (!sz || !sz[0])
            return {};
        wchar_t wide[512];
        const int n = MultiByteToWideChar(cp, flags, sz, -1, wide, 512);
        if (n <= 0)
            return {};
        return WideToUtf8(wide);
    }

    bool IsValidUtf8(const char* sz)
    {
        if (!sz || !sz[0])
            return false;
        wchar_t wide[512];
        return MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, sz, -1, wide, 512) > 0;
    }

    int CountCyrillicUtf8(const std::string& s)
    {
        int n = 0;
        for (size_t i = 0; i + 1 < s.size(); ++i)
        {
            const unsigned char a = static_cast<unsigned char>(s[i]);
            const unsigned char b = static_cast<unsigned char>(s[i + 1]);
            if ((a == 0xD0 || a == 0xD1) && b >= 0x80 && b <= 0xBF)
                ++n;
        }
        return n;
    }

    int CountLatinAscii(const std::string& s)
    {
        int n = 0;
        for (unsigned char c : s)
        {
            if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z'))
                ++n;
        }
        return n;
    }

    int CountJunkUtf8(const std::string& s)
    {
        int n = 0;
        for (unsigned char c : s)
        {
            if (c == 0x99 || c == 0xA9 || c == 0xAE || c == 0x98 || c == 0x8A)
                ++n;
            if (c == 0xEF && s.size() > 2)
                ++n;
        }
        for (size_t i = 0; i + 2 < s.size(); ++i)
        {
            const unsigned char a = static_cast<unsigned char>(s[i]);
            const unsigned char b = static_cast<unsigned char>(s[i + 1]);
            const unsigned char c = static_cast<unsigned char>(s[i + 2]);
            if (a == 0xE2 && b == 0x84 && c == 0xA2)
                ++n;
            if (a == 0xC2 && (b == 0xA9 || b == 0xAE))
                ++n;
        }
        return n;
    }

    // 1C SA font overlay (GXT byte → Cyrillic). 6OLJWON CMOYK = БОЛЬШОЙ СМОУК.
    // F and s both = Г (1C editors differ); P/g = Р; S/u = Ы.
    wchar_t Map1cChar(unsigned char c)
    {
        switch (c)
        {
        case 'A': case 'a': return L'А';
        case '6':           return L'Б';
        case 'B': case 'b': return L'В';
        case 'F': case 'f': return L'Г';
        case 's':           return L'Г';
        case '@':           return L'Д';
        case 'E': case 'e': return L'Е';
        case '$':           return L'Ё';
        case '^':           return L'Ж';
        case '3':           return L'З';
        case 'I': case 'i': return L'И';
        case 'N':           return L'Й';
        case 'K': case 'k': return L'К';
        case 'L': case 'l': return L'Л';
        case 'M': case 'm': return L'М';
        case 'H': case 'h': return L'Н';
        case 'O': case 'o': return L'О';
        case '<': case 'Z': return L'П';
        case 'P': case 'p': return L'Р';
        case 'g':           return L'Р';
        case 'C': case 'c': return L'С';
        case 'T': case 't': return L'Т';
        case 'Y': case 'y': return L'У';
        case '}': case 'V': return L'Ф';
        case 'X':           return L'Х';
        case 'x': case 'Q': return L'Ц';
        case '~': case '4': return L'Ч';
        case 'W':           return L'Ш';
        case 'q':           return L'Щ';
        case 'u':           return L'Ы';
        case 'S':           return L'Ы';
        case 'J': case 'j': return L'Ь';
        case 'z':           return L'Э';
        case 'w': case 'U': return L'Ю';
        case 'R':           return L'Я';
        default:            return 0;
        }
    }

    std::string Decode1cGxt(const char* sz)
    {
        if (!sz || !sz[0])
            return {};
        wchar_t wide[512];
        int n = 0;
        for (const unsigned char* p = reinterpret_cast<const unsigned char*>(sz); *p && n < 510; ++p)
        {
            const wchar_t mapped = Map1cChar(*p);
            wide[n++] = mapped ? mapped : static_cast<wchar_t>(*p);
        }
        wide[n] = 0;
        return WideToUtf8(wide);
    }

    // SanLTD / MSKL: Latin lookalikes stay (O→о, p→р…); unique letters sit in GTA's
    // 0x80–0xAF western-accent slots. Same bytes kEu maps to Ç/ñ/™.
    wchar_t MapSanLtdChar(unsigned char c)
    {
        switch (c)
        {
        case 'A': return L'А'; case 'a': return L'а';
        case 'B': return L'В'; case 'b': return L'в';
        case 'C': return L'С'; case 'c': return L'с';
        case 'E': return L'Е'; case 'e': return L'е';
        case 'H': return L'Н';
        case 'K': return L'К'; case 'k': return L'к';
        case 'M': return L'М'; case 'm': return L'м';
        case 'O': return L'О'; case 'o': return L'о';
        case 'P': return L'Р'; case 'p': return L'р';
        case 'T': return L'Т'; case 't': return L'т';
        case 'X': return L'Х'; case 'x': return L'х';
        case 'Y': return L'У'; case 'y': return L'у';
        case 0x80: return L'Б'; case 0x81: return L'Ф'; case 0x82: return L'Г';
        case 0x83: return L'Д'; case 0x85: return L'И'; case 0x86: return L'Й';
        case 0x87: return L'Л'; case 0x88: return L'З'; case 0x89: return L'Ц';
        case 0x8B: return L'В'; case 0x8C: return L'П'; case 0x8D: return L'Ч';
        case 0x8E: return L'Ш'; case 0x8F: return L'Т'; case 0x91: return L'Ы';
        case 0x93: return L'Э'; case 0x94: return L'У'; case 0x95: return L'Я';
        case 0x96: return L'М'; case 0x97: return L'б'; case 0x99: return L'г';
        case 0x9A: return L'д'; case 0x9B: return L'ж'; case 0x9C: return L'и';
        case 0x9D: return L'й'; case 0x9E: return L'л'; case 0x9F: return L'з';
        case 0xA0: return L'ц'; case 0xA1: return L'щ'; case 0xA2: return L'в';
        case 0xA3: return L'п'; case 0xA4: return L'ч'; case 0xA5: return L'ш';
        case 0xA6: return L'т'; case 0xA7: return L'ъ'; case 0xA8: return L'ы';
        case 0xA9: return L'ь'; case 0xAA: return L'э'; case 0xAB: return L'ю';
        case 0xAC: return L'я'; case 0xAD: return L'Н'; case 0xAE: return L'н';
        case 0xAF: return L'м';
        default:   return 0;
        }
    }

    std::string DecodeSanLtd(const char* sz)
    {
        if (!sz || !sz[0])
            return {};
        wchar_t wide[512];
        int n = 0;
        const unsigned char* p = reinterpret_cast<const unsigned char*>(sz);
        while (*p && n < 510)
        {
            if (*p == '~')
            {
                wide[n++] = L'~';
                ++p;
                while (*p && *p != '~' && n < 510)
                    wide[n++] = static_cast<wchar_t>(*p++);
                if (*p == '~' && n < 510)
                {
                    wide[n++] = L'~';
                    ++p;
                }
                continue;
            }
            const wchar_t mapped = MapSanLtdChar(*p);
            wide[n++] = mapped ? mapped : static_cast<wchar_t>(*p);
            ++p;
        }
        wide[n] = 0;
        return WideToUtf8(wide);
    }
}

std::string SaveSlots::AcpToUtf8(const char* sz)
{
    if (!sz || !sz[0])
        return {};

    wchar_t wide[512];
    const int n = MultiByteToWideChar(CP_ACP, 0, sz, -1, wide, 512);
    if (n <= 0)
        return {};

    char utf8[1024];
    if (WideCharToMultiByte(CP_UTF8, 0, wide, -1, utf8, 1024, nullptr, nullptr) <= 0)
        return {};

    return utf8;
}

bool SaveSlots::HasHighBytes(const char* sz)
{
    if (!sz)
        return false;
    for (const unsigned char* p = reinterpret_cast<const unsigned char*>(sz); *p; ++p)
    {
        if (*p >= 0x80)
            return true;
    }
    return false;
}

bool SaveSlots::LooksLike1cGxt(const char* sz)
{
    if (!sz)
        return false;
    int hits = 0;
    for (const unsigned char* p = reinterpret_cast<const unsigned char*>(sz); *p; ++p)
    {
        switch (*p)
        {
        case '6': case '3': case 'J': case 'W': case '<': case '@':
        case '^': case '$': case '~': case '}': case 'q': case 'F': case 'f':
            ++hits;
            break;
        default:
            break;
        }
    }
    return hits >= 2;
}

int SaveSlots::ScoreDecodedText(const std::string& s)
{
    if (s.empty())
        return -1000;
    return CountCyrillicUtf8(s) * 4 - CountJunkUtf8(s) * 6 - CountLatinAscii(s) * 2
           + static_cast<int>(s.size());
}

std::string SaveSlots::DecodeSaveText(const char* raw)
{
    if (!raw || !raw[0])
        return {};

    std::string best;
    int bestScore = -100000;

    auto consider = [&](const std::string& s)
    {
        const int sc = ScoreDecodedText(s);
        if (sc > bestScore)
        {
            bestScore = sc;
            best = s;
        }
    };

    if (HasHighBytes(raw))
    {
        if (IsValidUtf8(raw))
            consider(raw);
        consider(BytesToUtf8(raw, 1251));
        consider(AcpToUtf8(raw));
        consider(DecodeSanLtd(raw));
        {
            wchar_t wide[512];
            int n = 0;
            static const wchar_t kEu[0x30] = {
                L'À',L'Á',L'Â',L'Ä',L'Æ',L'Ç',L'È',L'É',L'Ê',L'Ë',L'Ì',L'Í',L'Î',L'Ï',L'Ò',L'Ó',
                L'Ô',L'Ö',L'Ù',L'Ú',L'Û',L'Ü',L'ß',L'à',L'á',L'â',L'ä',L'æ',L'ç',L'è',L'é',L'ê',
                L'ë',L'ì',L'í',L'î',L'ï',L'ò',L'ó',L'ô',L'ö',L'ù',L'ú',L'û',L'ü',L'Ñ',L'ñ',L'¿'
            };
            for (const unsigned char* p = reinterpret_cast<const unsigned char*>(raw); *p && n < 510; ++p)
            {
                if (*p >= 0x80 && *p < 0xB0)
                    wide[n++] = kEu[*p - 0x80];
                else
                    wide[n++] = static_cast<wchar_t>(*p);
            }
            wide[n] = 0;
            consider(WideToUtf8(wide));
        }
    }

    // 1C lookalike titles (6OLJWON CMOYK) only for Russian UI — never remap English BIG SMOKE.
    const bool ruUi = LanguageManager::GetLanguage() == LanguageManager::Lang::Russian;
    if (ruUi && (LooksLike1cGxt(raw) || !HasHighBytes(raw)))
        consider(Decode1cGxt(raw));
    else if (!HasHighBytes(raw))
        consider(AcpToUtf8(raw));

    if (best.empty())
        best = AcpToUtf8(raw);
    return best;
}

static wchar_t Map1cHudChar(unsigned char c)
{
    switch (c)
    {
    case 'D': case 'd': return L'Д';
    case 'N': case 'n': return L'И';
    case 'G':           return L'Ж';
    default:            return Map1cChar(c);
    }
}

static bool IsLatinLetter(unsigned char c)
{
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
}

static std::string Decode1cHudGxt(const char* sz)
{
    if (!sz || !sz[0])
        return {};
    wchar_t wide[512];
    int n = 0;
    const unsigned char* p = reinterpret_cast<const unsigned char*>(sz);
    while (*p && n < 510)
    {
        if (IsLatinLetter(*p))
        {
            const unsigned char* end = p;
            while (IsLatinLetter(*end))
                ++end;
            const bool keepKey = (end - p) == 1;
            while (p < end && n < 510)
            {
                if (keepKey)
                    wide[n++] = static_cast<wchar_t>(*p);
                else
                {
                    const wchar_t mapped = Map1cHudChar(*p);
                    wide[n++] = mapped ? mapped : static_cast<wchar_t>(*p);
                }
                ++p;
            }
            continue;
        }
        const wchar_t mapped = Map1cHudChar(*p);
        wide[n++] = mapped ? mapped : static_cast<wchar_t>(*p);
        ++p;
    }
    wide[n] = 0;
    return WideToUtf8(wide);
}

static void StripGxtTokens(const char* gxt, char* out, size_t cap);

static std::string WideToUtf8Dyn(const std::wstring& w)
{
    if (w.empty())
        return {};
    const int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (n <= 1)
        return {};
    std::string s(static_cast<size_t>(n - 1), '\0');
    WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, s.data(), n, nullptr, nullptr);
    return s;
}

static std::wstring MapOutsideTokens(const char* sz, wchar_t (*mapCh)(unsigned char))
{
    std::wstring w;
    if (!sz || !sz[0])
        return w;
    w.reserve(std::strlen(sz));
    const unsigned char* p = reinterpret_cast<const unsigned char*>(sz);
    while (*p)
    {
        if (*p == '~')
        {
            w.push_back(L'~');
            ++p;
            while (*p && *p != '~')
                w.push_back(static_cast<wchar_t>(*p++));
            if (*p == '~')
            {
                w.push_back(L'~');
                ++p;
            }
            continue;
        }
        const wchar_t mapped = mapCh(*p);
        w.push_back(mapped ? mapped : static_cast<wchar_t>(*p));
        ++p;
    }
    return w;
}

static int CountUtf8Needle(const std::string& s, const char* needle)
{
    if (s.empty() || !needle || !needle[0])
        return 0;
    int n = 0;
    for (size_t at = 0; (at = s.find(needle, at)) != std::string::npos; ++n)
        at += std::strlen(needle);
    return n;
}

SaveSlots::GxtCodec SaveSlots::DetectGxtCodec(const char* sample)
{
    if (!sample || !sample[0])
        return GxtCodec::Latin1252;

    char stripped[4096];
    StripGxtTokens(sample, stripped, sizeof(stripped));
    if (!stripped[0])
        return GxtCodec::Latin1252;

    const std::string oneC = Decode1cHudGxt(stripped);
    const std::string san = HasHighBytes(stripped) ? DecodeSanLtd(stripped) : std::string{};
    const std::string lat = BytesToUtf8(stripped, 1252);

    const int ru1 = CountUtf8Needle(oneC, "для") + CountUtf8Needle(oneC, "Для") + CountUtf8Needle(oneC, "ДЛЯ")
        + CountUtf8Needle(oneC, "нажм") + CountUtf8Needle(oneC, "Нажм") + CountUtf8Needle(oneC, "НАЖМ")
        + CountUtf8Needle(oneC, "удерж") + CountUtf8Needle(oneC, "чтобы") + CountUtf8Needle(oneC, "Чтобы")
        + CountUtf8Needle(oneC, "нельзя") + CountUtf8Needle(oneC, "клави")
        + CountUtf8Needle(oneC, "движен") + CountUtf8Needle(oneC, "Движен");
    const int ruS = CountUtf8Needle(san, "для") + CountUtf8Needle(san, "Для") + CountUtf8Needle(san, "ДЛЯ")
        + CountUtf8Needle(san, "нажм") + CountUtf8Needle(san, "удерж") + CountUtf8Needle(san, "чтобы");
    const int en = CountUtf8Needle(lat, "the ") + CountUtf8Needle(lat, "press")
        + CountUtf8Needle(lat, "hold") + CountUtf8Needle(lat, "you ")
        + CountUtf8Needle(lat, "weapon") + CountUtf8Needle(lat, "vehicle");

    if (ruS > ru1 && ruS >= 2 && HasHighBytes(stripped))
        return GxtCodec::SanLtd;
    if (ru1 >= 2 && ru1 >= en)
        return GxtCodec::OneC;
    if (HasHighBytes(stripped) && ruS >= 2)
        return GxtCodec::SanLtd;
    if (!HasHighBytes(stripped) && LooksLike1cGxt(stripped) && ru1 > en)
        return GxtCodec::OneC;
    return GxtCodec::Latin1252;
}

std::string SaveSlots::DecodeGxtKeepTokens(const char* raw, GxtCodec codec)
{
    if (!raw || !raw[0])
        return {};

    auto map1c = [](unsigned char c) -> wchar_t {
        return Map1cHudChar(c);
    };
    auto mapSan = [](unsigned char c) -> wchar_t {
        return MapSanLtdChar(c);
    };

    if (codec == GxtCodec::OneC)
        return WideToUtf8Dyn(MapOutsideTokens(raw, map1c));
    if (codec == GxtCodec::SanLtd)
        return WideToUtf8Dyn(MapOutsideTokens(raw, mapSan));

    std::wstring w;
    const unsigned char* p = reinterpret_cast<const unsigned char*>(raw);
    while (*p)
    {
        if (*p == '~')
        {
            w.push_back(L'~');
            ++p;
            while (*p && *p != '~')
                w.push_back(static_cast<wchar_t>(*p++));
            if (*p == '~')
            {
                w.push_back(L'~');
                ++p;
            }
            continue;
        }
        const unsigned char* run = p;
        while (*p && *p != '~')
            ++p;
        char tmp[512];
        const size_t n = static_cast<size_t>(p - run);
        const size_t take = n < 511 ? n : 511;
        memcpy(tmp, run, take);
        tmp[take] = 0;
        wchar_t wide[512];
        const int wn = MultiByteToWideChar(1252, 0, tmp, static_cast<int>(take), wide, 512);
        if (wn > 0)
            w.append(wide, wide + wn);
        else
        {
            for (size_t i = 0; i < take; ++i)
                w.push_back(static_cast<wchar_t>(tmp[i]));
        }
    }
    return WideToUtf8Dyn(w);
}

std::string SaveSlots::DecodeSanLtdHighBytes(const char* raw)
{
    if (!raw || !raw[0])
        return {};
    auto mapHigh = [](unsigned char c) -> wchar_t {
        return (c >= 0x80) ? MapSanLtdChar(c) : 0;
    };
    return WideToUtf8Dyn(MapOutsideTokens(raw, mapHigh));
}

static void StripGxtTokens(const char* gxt, char* out, size_t cap)
{
    if (!gxt || !out || cap < 2)
    {
        if (out && cap)
            out[0] = 0;
        return;
    }
    size_t o = 0;
    for (size_t i = 0; gxt[i] && o + 1 < cap; )
    {
        if (gxt[i] == '~')
        {
            size_t j = i + 1;
            while (gxt[j] && gxt[j] != '~')
                ++j;
            if (!gxt[j])
                break;
            if (j == i + 2 && (gxt[i + 1] == 'n' || gxt[i + 1] == 'N'))
                out[o++] = '\n';
            i = j + 1;
            continue;
        }
        out[o++] = gxt[i++];
    }
    out[o] = 0;
}

std::string SaveSlots::DecodeHudText(const char* gxt)
{
    if (!gxt || !gxt[0] || gxt[0] == '*')
        return {};

    char stripped[512];
    StripGxtTokens(gxt, stripped, sizeof(stripped));
    if (!stripped[0])
        return {};

    std::string best;
    int bestScore = -100000;

    auto consider = [&](const std::string& s)
    {
        const int sc = ScoreDecodedText(s);
        if (sc > bestScore)
        {
            bestScore = sc;
            best = s;
        }
    };

    consider(AcpToUtf8(stripped));
    consider(BytesToUtf8(stripped, 1251));
    if (IsValidUtf8(stripped))
        consider(stripped);

    if (HasHighBytes(stripped))
        consider(DecodeSanLtd(stripped));

    const bool ruUi = LanguageManager::GetLanguage() == LanguageManager::Lang::Russian;
    if (ruUi || LooksLike1cGxt(stripped))
    {
        consider(Decode1cGxt(stripped));
        consider(Decode1cHudGxt(stripped));
        if (ruUi)
            consider(DecodeSanLtd(stripped));
    }

    if (best.empty())
        best = AcpToUtf8(stripped);
    return best;
}

std::string SaveSlots::FormatSaveDateRu(int day, int month, int year, int hour, int minute, int second)
{
    static const char* kKeys[13] = {
        "", "MON_01", "MON_02", "MON_03", "MON_04", "MON_05", "MON_06",
        "MON_07", "MON_08", "MON_09", "MON_10", "MON_11", "MON_12"
    };
    if (month < 1 || month > 12)
        month = 0;
    const char* mon = (month > 0) ? LanguageManager::Get(kKeys[month]) : "???";

    char buf[64];
    sprintf_s(buf, "%02d %s %04d %02d:%02d:%02d",
              day, mon, year, hour, minute, second);
    return buf;
}

bool SaveSlots::ReadSaveSlotMeta(int slot, std::string& outMissionKey,
                                       int& day, int& month, int& year,
                                       int& hour, int& minute, int& second)
{
    outMissionKey.clear();
    day = month = year = hour = minute = second = 0;
    if (slot < 0 || slot >= kCount)
        return false;

    char path[MAX_PATH]{};
    plugin::CallMethod<0x6190A0, C_PcSave*, int, char*>(&PcSaveHelper, slot, path);
    if (!path[0])
        return false;

    FILE* f = nullptr;
    if (fopen_s(&f, path, "rb") != 0 || !f)
        return false;

    if (fseek(f, 0, SEEK_END) != 0)
    {
        fclose(f);
        return false;
    }
    const long fileSize = ftell(f);
    if (fileSize < 0x200 || fileSize > 512 * 1024)
    {
        fclose(f);
        return false;
    }
    if (fseek(f, 0, SEEK_SET) != 0)
    {
        fclose(f);
        return false;
    }

    std::vector<unsigned char> data(static_cast<size_t>(fileSize));
    if (fread(data.data(), 1, data.size(), f) != data.size())
    {
        fclose(f);
        return false;
    }
    fclose(f);

    std::vector<size_t> blocks;
    for (size_t i = 0; i + 5 <= data.size(); ++i)
    {
        if (data[i] == 'B' && data[i + 1] == 'L' && data[i + 2] == 'O'
            && data[i + 3] == 'C' && data[i + 4] == 'K')
        {
            blocks.push_back(i + 5);
            i += 4;
        }
    }

    bool ok = false;
    if (!blocks.empty() && blocks[0] + 0x11E + 16 <= data.size())
    {
        const unsigned char* p = data.data() + blocks[0] + 0x11E;
        unsigned short st[8];
        std::memcpy(st, p, sizeof(st));
        year = st[0];
        month = st[1];
        day = st[3];
        hour = st[4];
        minute = st[5];
        second = st[6];
        if (year >= 2004 && year <= 2100 && month >= 1 && month <= 12 && day >= 1 && day <= 31)
            ok = true;
    }

    if (blocks.size() > 16 && blocks[16] + 0x544 + 8 <= data.size())
    {
        char key[9]{};
        std::memcpy(key, data.data() + blocks[16] + 0x544, 8);
        key[8] = 0;
        bool valid = key[0] != 0;
        for (int i = 0; i < 8 && key[i]; ++i)
        {
            if (key[i] < 32 || key[i] > 126)
            {
                valid = false;
                break;
            }
        }
        if (valid)
            outMissionKey = key;
    }

    return ok;
}

void SaveSlots::Refresh()
{
    PcSaveHelper.PopulateSlotInfo();

    for (int i = 0; i < kCount; ++i)
    {
        Slot& slot = m_slots[i];
        slot = {};

        const int state = reinterpret_cast<int*>(0xC16EBC)[i];
        if (state == SLOT_EMPTY)
        {
            slot.empty = true;
            slot.name = LanguageManager::Get("UI_EMPTY_SLOT");
            continue;
        }

        if (state == SLOT_CORRUPTED)
        {
            slot.corrupted = true;
            slot.name = LanguageManager::Get("UI_CORRUPT_SAVE");
            continue;
        }

        const char* name = CGenericGameStorage::GetNameOfSavedGame(i);
        std::string utf = DecodeSaveText(name);

        std::string missionKey;
        int day = 0, month = 0, year = 0, hour = 0, minute = 0, second = 0;
        const bool metaOk = ReadSaveSlotMeta(i, missionKey, day, month, year, hour, minute, second);

        if (HasHighBytes(name) && !missionKey.empty())
        {
            if (const char* gxt = TheText.Get(missionKey.c_str()))
            {
                std::string fromGxt = DecodeSaveText(gxt);
                if (!fromGxt.empty() && ScoreDecodedText(fromGxt) >= ScoreDecodedText(utf))
                    utf = std::move(fromGxt);
            }
        }

        if (utf.empty())
        {
            slot.empty = true;
            slot.name = LanguageManager::Get("UI_EMPTY_SLOT");
            continue;
        }

        slot.empty = false;
        slot.name = utf;
        if (metaOk)
            slot.date = FormatSaveDateRu(day, month, year, hour, minute, second);
        else
            slot.date = AcpToUtf8(CGenericGameStorage::ms_SlotSaveDate[i].m_sSavedGameDateAndTime);
    }

    m_loaded = true;
    m_refreshAt = GetTickCount();
}

void SaveSlots::Ensure()
{
    const DWORD now = GetTickCount();
    if (!m_loaded || (now - m_refreshAt) > 2000)
        Refresh();
}

bool SaveSlots::HasAny() const
{
    for (int i = 0; i < kCount; ++i)
    {
        if (!m_slots[i].empty && !m_slots[i].corrupted)
            return true;
    }
    return false;
}

int SaveSlots::FindMostRecent() const
{
    int best = -1;
    ULARGE_INTEGER bestTime{};
    bestTime.QuadPart = 0;

    for (int i = 0; i < kCount; ++i)
    {
        if (m_slots[i].empty || m_slots[i].corrupted)
            continue;

        const char* path = CGenericGameStorage::ms_SlotFileName[i].m_sSavedGameName;
        if (!path || !path[0])
        {
            if (best < 0)
                best = i;
            continue;
        }

        WIN32_FILE_ATTRIBUTE_DATA fad{};
        if (!GetFileAttributesExA(path, GetFileExInfoStandard, &fad))
        {
            if (best < 0)
                best = i;
            continue;
        }

        ULARGE_INTEGER ft{};
        ft.LowPart = fad.ftLastWriteTime.dwLowDateTime;
        ft.HighPart = fad.ftLastWriteTime.dwHighDateTime;
        if (best < 0 || ft.QuadPart >= bestTime.QuadPart)
        {
            bestTime = ft;
            best = i;
        }
    }
    return best;
}
