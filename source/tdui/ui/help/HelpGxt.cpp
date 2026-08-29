/*****************************************************************************
 *
 *  PROJECT:     The-Definitive-UI
 *  FILE:        source/Help/HelpGxt.cpp
 *  PURPOSE:     Help phrases dict + TEXT/*.gxt fallback; track key via CText::Get
 *
 *****************************************************************************/

#include "HelpGxt.h"
#include "DictionaryStore.h"
#include "LanguageManager.h"
#include "ModPaths.h"
#include "SaveSlots.h"

#include "plugin.h"
#include "CMessages.h"
#include "CKeyGen.h"
#include "CText.h"

#include <windows.h>
#include <array>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <string>
#include <unordered_map>
#include <vector>

namespace
{
    constexpr uintptr_t kCTextGet      = 0x6A0050;
    constexpr uintptr_t kSetHelp       = 0x588BE0;
    constexpr uintptr_t kSetHelpNumber = 0x588E30;
    constexpr int kLangCount = static_cast<int>(LanguageManager::Lang::Count);
    constexpr int kGetRing = 32;
    constexpr int kMsgLen = 400;

    struct GetRec
    {
        const char* ptr = nullptr;
        char        key[16]{};
    };

    std::unordered_map<unsigned, std::string> s_maps[kLangCount];
    SafetyHookInline s_hookGet;
    SafetyHookInline s_hookSet;
    SafetyHookInline s_hookSetNum;
    GetRec s_ring[kGetRing];
    int    s_ringI = 0;
    char   s_helpKey[16]{};
    int    s_helpNums[6] = { -1, -1, -1, -1, -1, -1 };
    bool   s_loaded = false;
    bool   s_hooksOn = false;
    SaveSlots::GxtCodec s_keyNameCodec = SaveSlots::GxtCodec::Latin1252;

    unsigned ReadU32(const uint8_t* p)
    {
        unsigned v;
        memcpy(&v, p, 4);
        return v;
    }

    unsigned ReadU16(const uint8_t* p)
    {
        unsigned v = 0;
        memcpy(&v, p, 2);
        return v;
    }

    bool MagicEq(const uint8_t* p, const char* m)
    {
        return p[0] == static_cast<uint8_t>(m[0])
            && p[1] == static_cast<uint8_t>(m[1])
            && p[2] == static_cast<uint8_t>(m[2])
            && p[3] == static_cast<uint8_t>(m[3]);
    }

    void CopyKey(char* dst, size_t cap, const char* src)
    {
        if (!dst || cap < 2)
            return;
        dst[0] = 0;
        if (!src)
            return;
        size_t n = 0;
        for (; src[n] && n + 1 < cap && n < 15; ++n)
        {
            const unsigned char c = static_cast<unsigned char>(src[n]);
            if (c <= ' ')
                break;
            dst[n] = static_cast<char>(std::toupper(c));
        }
        dst[n] = 0;
    }

    void RememberGet(const char* key, const char* ptr)
    {
        if (!key || !key[0] || key[0] == ' ')
            return;
        GetRec& r = s_ring[s_ringI];
        s_ringI = (s_ringI + 1) % kGetRing;
        r.ptr = ptr;
        CopyKey(r.key, sizeof(r.key), key);
    }

    // 0xC1B100 — gGxtString[552]; pickups InsertNumber into this then SetHelpMessage.
    const char* GxtScratch()
    {
        return reinterpret_cast<const char*>(0xC1B100);
    }

    void ResolveHelpKey(const char* text)
    {
        s_helpKey[0] = 0;
        s_helpNums[0] = s_helpNums[1] = s_helpNums[2] = s_helpNums[3]
            = s_helpNums[4] = s_helpNums[5] = -1;
        if (!text || !text[0])
            return;

        for (int i = 0; i < kGetRing; ++i)
        {
            const int idx = (s_ringI - 1 - i + kGetRing * 2) % kGetRing;
            const GetRec& rec = s_ring[idx];
            if (rec.key[0] && rec.ptr == text)
            {
                CopyKey(s_helpKey, sizeof(s_helpKey), rec.key);
                return;
            }
        }

        // InsertNumber dest is not the TheText pointer — last Get is the label.
        if (text == GxtScratch())
        {
            const int last = (s_ringI - 1 + kGetRing) % kGetRing;
            if (s_ring[last].key[0])
                CopyKey(s_helpKey, sizeof(s_helpKey), s_ring[last].key);
        }
    }

    void ParseLiveNumbers(const char* live)
    {
        if (!live)
            return;
        int slot = 0;
        const unsigned char* p = reinterpret_cast<const unsigned char*>(live);
        while (*p && slot < 6)
        {
            if (*p == '~')
            {
                ++p;
                while (*p && *p != '~')
                    ++p;
                if (*p == '~')
                    ++p;
                continue;
            }
            if (*p >= '0' && *p <= '9')
            {
                int v = 0;
                while (*p >= '0' && *p <= '9')
                {
                    v = v * 10 + (*p - '0');
                    ++p;
                }
                s_helpNums[slot++] = v;
                continue;
            }
            ++p;
        }
    }

    void IngestTkeyTdat(const uint8_t* file, size_t size, size_t tkeyHdr,
                        bool wide, std::unordered_map<unsigned, std::string>& raw)
    {
        if (tkeyHdr + 8 > size || !MagicEq(file + tkeyHdr, "TKEY"))
            return;
        const unsigned tkeySize = ReadU32(file + tkeyHdr + 4);
        const size_t tkeyData = tkeyHdr + 8;
        if (tkeyData + tkeySize > size)
            return;

        size_t tdatHdr = tkeyData + tkeySize;
        if (tdatHdr + 8 > size || !MagicEq(file + tdatHdr, "TDAT"))
            return;
        const unsigned tdatSize = ReadU32(file + tdatHdr + 4);
        const size_t tdatData = tdatHdr + 8;
        if (tdatData + tdatSize > size)
            return;

        const unsigned nEnt = tkeySize / 8;
        for (unsigned i = 0; i < nEnt; ++i)
        {
            const size_t e = tkeyData + static_cast<size_t>(i) * 8;
            const unsigned off = ReadU32(file + e);
            const unsigned hash = ReadU32(file + e + 4);
            if (off >= tdatSize)
                continue;

            if (!wide)
            {
                const char* s = reinterpret_cast<const char*>(file + tdatData + off);
                size_t maxLen = tdatSize - off;
                size_t n = 0;
                while (n < maxLen && s[n])
                    ++n;
                raw[hash] = std::string(s, n);
            }
            else
            {
                const uint8_t* s = file + tdatData + off;
                std::wstring w;
                for (size_t p = off; p + 1 < tdatSize; p += 2)
                {
                    const unsigned ch = ReadU16(file + tdatData + p);
                    if (ch == 0)
                        break;
                    w.push_back(static_cast<wchar_t>(ch));
                }
                if (w.empty())
                    continue;
                const int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, nullptr, 0, nullptr, nullptr);
                if (n <= 1)
                    continue;
                std::string u(static_cast<size_t>(n - 1), '\0');
                WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, u.data(), n, nullptr, nullptr);
                raw[hash] = std::move(u);
            }
        }
    }

    void ParseGxtBytes(const uint8_t* file, size_t size, std::unordered_map<unsigned, std::string>& raw, bool& wideOut)
    {
        raw.clear();
        wideOut = false;
        if (size < 16)
            return;

        const unsigned encoding = ReadU16(file + 2);
        wideOut = (encoding == 16);

        size_t pos = 4;
        while (pos + 8 <= size)
        {
            const uint8_t* mag = file + pos;
            const unsigned chunkSize = ReadU32(file + pos + 4);
            const size_t payload = pos + 8;
            if (payload + chunkSize > size)
                break;

            if (MagicEq(mag, "TABL"))
            {
                const unsigned n = chunkSize / 12;
                for (unsigned i = 0; i < n; ++i)
                {
                    const unsigned off = ReadU32(file + payload + static_cast<size_t>(i) * 12 + 8);
                    if (off + 8 > size)
                        continue;
                    // Vanilla MAIN: 8-byte table name then TKEY. SanLTD MAIN: TKEY at off.
                    if (MagicEq(file + off, "TKEY"))
                        IngestTkeyTdat(file, size, off, wideOut, raw);
                    else
                        IngestTkeyTdat(file, size, off + 8, wideOut, raw);
                }
            }
            else if (MagicEq(mag, "TKEY"))
            {
                IngestTkeyTdat(file, size, pos, wideOut, raw);
            }

            pos = payload + chunkSize;
        }
    }

    bool ReadWholeFile(const char* path, std::vector<uint8_t>& out)
    {
        out.clear();
        FILE* f = nullptr;
        if (fopen_s(&f, path, "rb") != 0 || !f)
            return false;
        if (fseek(f, 0, SEEK_END) != 0)
        {
            fclose(f);
            return false;
        }
        const long sz = ftell(f);
        if (sz < 16 || sz > 8 * 1024 * 1024)
        {
            fclose(f);
            return false;
        }
        if (fseek(f, 0, SEEK_SET) != 0)
        {
            fclose(f);
            return false;
        }
        out.resize(static_cast<size_t>(sz));
        const bool ok = fread(out.data(), 1, out.size(), f) == out.size();
        fclose(f);
        return ok;
    }

    void GameTextDir(char* out, size_t cap)
    {
        out[0] = 0;
        char exe[MAX_PATH]{};
        if (!GetModuleFileNameA(nullptr, exe, MAX_PATH) || !exe[0])
            return;
        char* slash = strrchr(exe, '\\');
        if (!slash)
            slash = strrchr(exe, '/');
        if (!slash)
            return;
        *slash = 0;
        sprintf_s(out, cap, "%s\\text", exe);
    }

    void CommitRaw(const std::unordered_map<unsigned, std::string>& raw, bool wide,
                   SaveSlots::GxtCodec codec, LanguageManager::Lang lang)
    {
        const int li = static_cast<int>(lang);
        if (li < 0 || li >= kLangCount)
            return;
        auto& dst = s_maps[li];
        dst.reserve(dst.size() + raw.size());
        for (const auto& kv : raw)
        {
            if (wide)
                dst[kv.first] = kv.second;
            else
                dst[kv.first] = SaveSlots::DecodeGxtKeepTokens(kv.second.c_str(), codec);
        }
    }

    bool LoadOneFile(const char* path, LanguageManager::Lang lang, bool detectRuPack,
                     const SaveSlots::GxtCodec* forceCodec = nullptr, bool touchKeyCodec = true)
    {
        std::vector<uint8_t> bytes;
        if (!ReadWholeFile(path, bytes))
            return false;

        std::unordered_map<unsigned, std::string> raw;
        bool wide = false;
        ParseGxtBytes(bytes.data(), bytes.size(), raw, wide);
        if (raw.empty())
            return false;

        SaveSlots::GxtCodec codec = SaveSlots::GxtCodec::Latin1252;
        if (forceCodec)
            codec = *forceCodec;
        else if (!wide && detectRuPack)
        {
            std::string sample;
            sample.reserve(4096);
            int n = 0;
            auto take = [&](bool preferHelp) {
                for (const auto& kv : raw)
                {
                    if (kv.second.size() < 8)
                        continue;
                    if (preferHelp && kv.second.find("~k~~") == std::string::npos
                        && kv.second.size() < 40)
                        continue;
                    sample.append(kv.second);
                    sample.push_back(' ');
                    if (++n >= 40)
                        break;
                }
            };
            take(true);
            if (n < 8)
                take(false);
            codec = SaveSlots::DetectGxtCodec(sample.c_str());
        }

        if (detectRuPack && !wide
            && (codec == SaveSlots::GxtCodec::OneC || codec == SaveSlots::GxtCodec::SanLtd))
        {
            CommitRaw(raw, wide, codec, LanguageManager::Lang::Russian);
            if (s_maps[static_cast<int>(LanguageManager::Lang::American)].empty())
                CommitRaw(raw, wide, codec, LanguageManager::Lang::American);
            if (touchKeyCodec)
                s_keyNameCodec = codec;
            return true;
        }

        CommitRaw(raw, wide, codec, lang);
        if (detectRuPack && touchKeyCodec)
            s_keyNameCodec = codec;
        return true;
    }

    void AsiDir(char* out, size_t cap)
    {
        out[0] = 0;
        HMODULE mod = nullptr;
        if (!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS
                                | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                                reinterpret_cast<LPCSTR>(&HelpGxt::Load), &mod)
            || !mod)
            return;
        char file[MAX_PATH]{};
        if (!GetModuleFileNameA(mod, file, MAX_PATH) || !file[0])
            return;
        char* slash = strrchr(file, '\\');
        if (!slash)
            slash = strrchr(file, '/');
        if (!slash)
            return;
        *slash = 0;
        strncpy_s(out, cap, file, _TRUNCATE);
    }

    void TryLoadSanLtdRussian()
    {
        const SaveSlots::GxtCodec san = SaveSlots::GxtCodec::SanLtd;
        char path[MAX_PATH]{};
        auto tryPath = [&](const char* p) -> bool {
            return p && p[0] && LoadOneFile(p, LanguageManager::Lang::Russian, false, &san, false);
        };

        if (ModPaths::BuildModPath(path, sizeof(path), "ui-text\\sanltd_american.gxt")
            && tryPath(path))
            return;

        char asi[MAX_PATH]{};
        AsiDir(asi, sizeof(asi));
        if (asi[0])
        {
            sprintf_s(path, "%s\\sanltd_american.gxt", asi);
            if (tryPath(path))
                return;
        }

        char textDir[MAX_PATH]{};
        GameTextDir(textDir, sizeof(textDir));
        if (textDir[0])
        {
            char* slash = strrchr(textDir, '\\');
            if (slash)
                *slash = 0;
            sprintf_s(path, "%s\\modloader\\SanLTD 0.56 Russifier\\text\\american.gxt", textDir);
            if (tryPath(path))
                return;
            sprintf_s(path, "%s Classic\\modloader\\SanLTD 0.56 Russifier\\text\\american.gxt", textDir);
            if (tryPath(path))
                return;
        }
    }

    void StripTokens(const char* in, char* out, size_t cap)
    {
        if (!out || cap < 2)
            return;
        out[0] = 0;
        if (!in)
            return;
        size_t o = 0;
        for (size_t i = 0; in[i] && o + 1 < cap; )
        {
            if (in[i] == '~')
            {
                size_t j = i + 1;
                while (in[j] && in[j] != '~')
                    ++j;
                if (!in[j])
                    break;
                i = j + 1;
                continue;
            }
            out[o++] = in[i++];
        }
        out[o] = 0;
    }

    bool HasHighBytes(const char* s)
    {
        if (!s)
            return false;
        for (const unsigned char* p = reinterpret_cast<const unsigned char*>(s); *p; ++p)
        {
            if (*p >= 0x80)
                return true;
        }
        return false;
    }

    const char* DictKeyLabel(const char* key, const char* ruFallback, const char* enFallback)
    {
        if (const char* s = LanguageManager::Get(key))
        {
            if (s[0] && std::strcmp(s, key) != 0)
                return s;
        }
        const bool ru = LanguageManager::GetLanguage() == LanguageManager::Lang::Russian;
        return ru ? ruFallback : enFallback;
    }

    const char* PrettyKeyName(const char* raw)
    {
        if (!raw || !raw[0])
            return nullptr;
        char up[64]{};
        size_t n = 0;
        bool prevSpace = false;
        for (size_t i = 0; raw[i] && n + 1 < sizeof(up); ++i)
        {
            unsigned char c = static_cast<unsigned char>(raw[i]);
            if (c == ' ' || c == '\t')
            {
                if (n == 0 || prevSpace)
                    continue;
                prevSpace = true;
                up[n++] = ' ';
                continue;
            }
            prevSpace = false;
            if (c >= 'a' && c <= 'z')
                c = static_cast<unsigned char>(c - 32);
            up[n++] = static_cast<char>(c);
        }
        while (n && up[n - 1] == ' ')
            up[--n] = 0;
        up[n] = 0;

        const bool ru = LanguageManager::GetLanguage() == LanguageManager::Lang::Russian;

        if (std::strcmp(up, "UP") == 0)
            return ru ? "Вверх" : "Up";
        if (std::strcmp(up, "DOWN") == 0)
            return ru ? "Вниз" : "Down";
        if (std::strcmp(up, "LEFT") == 0)
            return ru ? "Влево" : "Left";
        if (std::strcmp(up, "RIGHT") == 0)
            return ru ? "Вправо" : "Right";
        if (std::strcmp(up, "SPACE") == 0 || std::strcmp(up, "SPACEBAR") == 0 || std::strcmp(up, "ZPO6EL") == 0)
            return DictKeyLabel("UI_KEY_SPACE", "Пробел", "Space");
        if (std::strcmp(up, "LMB") == 0)
            return DictKeyLabel("FEC_MSL", "ЛКМ", "LMB");
        if (std::strcmp(up, "RMB") == 0)
            return DictKeyLabel("FEC_MSR", "ПКМ", "RMB");
        if (std::strcmp(up, "MMB") == 0)
            return DictKeyLabel("FEC_MSM", "СКМ", "MMB");
        if (std::strcmp(up, "MS WHEEL UP") == 0 || std::strcmp(up, "KOLECNKO BVERX") == 0 || std::strcmp(up, "KOLECNKO BBEPX") == 0)
            return DictKeyLabel("FEC_MWF", "Колесико вверх", "Wheel Up");
        if (std::strcmp(up, "MS WHEEL DN") == 0 || std::strcmp(up, "MS WHEEL DOWN") == 0 || std::strcmp(up, "KOLECNKO BHN3") == 0)
            return DictKeyLabel("FEC_MWB", "Колесико вниз", "Wheel Down");
        if (std::strcmp(up, "MOUSEWHEEL") == 0)
            return ru ? "Колесико" : "Mouse Wheel";

        static const struct { const char* a; const char* b; } kMapCommon[] = {
            { "RETURN", "Enter" }, { "ENTER", "Enter" }, { "ENT", "Enter" },
            { "BSPACE", "Backspace" }, { "BACKSPACE", "Backspace" },
            { "TAB", "Tab" }, { "ESC", "Esc" }, { "ESCAPE", "Esc" },
            { "LSHIFT", "Left Shift" }, { "RSHIFT", "Right Shift" }, { "SHIFT", "Shift" },
            { "LCTRL", "Left Ctrl" }, { "RCTRL", "Right Ctrl" }, { "CTRL", "Ctrl" },
            { "LALT", "Left Alt" }, { "RALT", "Right Alt" }, { "ALT", "Alt" },
            { "LWIN", "Left Win" }, { "RWIN", "Right Win" }, { "WINCLICK", "Win" },
            { "CAPSLOCK", "Caps Lock" }, { "NUMLOCK", "Num Lock" },
            { "SCROLL LOCK", "Scroll Lock" }, { "BREAK", "Break" },
            { "INS", "Insert" }, { "INSERT", "Insert" }, { "DEL", "Delete" }, { "DELETE", "Delete" },
            { "HOME", "Home" }, { "END", "End" }, { "PGUP", "Page Up" }, { "PGDN", "Page Down" },
            { "L.CTRL", "Left Ctrl" }, { "P.CTRL", "Right Ctrl" }, { "R.CTRL", "Right Ctrl" },
        };
        for (const auto& e : kMapCommon)
        {
            if (std::strcmp(up, e.a) == 0)
                return e.b;
        }

        if (ru)
        {
            static const struct { const char* a; const char* b; } kMapRu[] = {
                // 1C GXT stores arrow names as lookalikes, not English UP/DOWN.
                { "BBEPX", "Вверх" }, { "BVERX", "Вверх" }, { "BHN3", "Вниз" },
                { "HALEBO", "Влево" }, { "HAZPABO", "Вправо" },
                { "ВВЕРХ", "Вверх" }, { "ВНИЗ", "Вниз" },
                { "НАЛЕВО", "Влево" }, { "ВЛЕВО", "Влево" },
                { "НАПРАВО", "Вправо" }, { "ВПРАВО", "Вправо" },
                { "ПРОБЕЛ", "Пробел" },
                { "КОЛЕСИКО ВВЕРХ", "Колесико вверх" }, { "КОЛЕСИКО ВНИЗ", "Колесико вниз" },
                { "Л.CTRL", "Left Ctrl" }, { "П.CTRL", "Right Ctrl" },
                { "Л.SHIFT", "Left Shift" }, { "П.SHIFT", "Right Shift" },
                { "Л.ALT", "Left Alt" }, { "П.ALT", "Right Alt" },
            };
            for (const auto& e : kMapRu)
            {
                if (std::strcmp(up, e.a) == 0)
                    return e.b;
            }
        }
        return nullptr;
    }

    bool HasEnglishKeyToken(const char* s)
    {
        if (!s || !s[0])
            return false;
        char up[64]{};
        size_t n = 0;
        for (size_t i = 0; s[i] && n + 1 < sizeof(up); ++i)
        {
            unsigned char c = static_cast<unsigned char>(s[i]);
            if (c >= 'a' && c <= 'z')
                c = static_cast<unsigned char>(c - 32);
            if ((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == ' ' || c == '.')
                up[n++] = static_cast<char>(c);
        }
        up[n] = 0;
        static const char* kTok[] = {
            "CTRL", "SHIFT", "ALT", "ENTER", "RETURN", "SPACE", "TAB", "ESC",
            "DELETE", "INSERT", "HOME", "END", "PGUP", "PGDN", "LMB", "RMB", "MMB",
            "BACKSPACE", "CAPS", "WIN"
        };
        for (const char* t : kTok)
        {
            if (std::strstr(up, t))
                return true;
        }
        return false;
    }

    std::string AsciiKeyTail(const char* s)
    {
        char up[64]{};
        size_t n = 0;
        for (size_t i = 0; s && s[i] && n + 1 < sizeof(up); ++i)
        {
            unsigned char c = static_cast<unsigned char>(s[i]);
            if (c >= 'a' && c <= 'z')
                c = static_cast<unsigned char>(c - 32);
            if ((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == ' ')
            {
                if (c == ' ' && (n == 0 || up[n - 1] == ' '))
                    continue;
                up[n++] = static_cast<char>(c);
            }
        }
        while (n && up[n - 1] == ' ')
            up[--n] = 0;
        up[n] = 0;
        return up;
    }

    std::string DecodeInsertedKeys(const char* buf)
    {
        if (!buf || !buf[0])
            return {};

        char stripped[kMsgLen]{};
        StripTokens(buf, stripped, sizeof(stripped));
        if (!stripped[0])
            return {};

        if (const char* pretty = PrettyKeyName(stripped))
            return pretty;

        const bool ru = LanguageManager::GetLanguage() == LanguageManager::Lang::Russian;
        if (!HasHighBytes(stripped))
        {
            if (ru && std::strlen(stripped) >= 3)
            {
                const std::string one = SaveSlots::DecodeGxtKeepTokens(
                    stripped, SaveSlots::GxtCodec::OneC);
                if (const char* pretty = PrettyKeyName(one.c_str()))
                    return pretty;
            }
            return stripped;
        }

        if (!ru)
            return SaveSlots::DecodeGxtKeepTokens(buf, s_keyNameCodec);

        // SanLTD arrows: 0x8B 0x8B EPX → ВВЕРХ. CTRL keeps Latin (Л.CTRL).
        const std::string high = SaveSlots::DecodeSanLtdHighBytes(stripped);
        if (const char* pretty = PrettyKeyName(high.c_str()))
            return pretty;
        if (HasEnglishKeyToken(high.c_str()))
        {
            const std::string tail = AsciiKeyTail(high.c_str());
            if (const char* pretty = PrettyKeyName(tail.c_str()))
                return pretty;
            return high;
        }

        const std::string san = SaveSlots::DecodeGxtKeepTokens(
            stripped, SaveSlots::GxtCodec::SanLtd);
        char inner[kMsgLen]{};
        StripTokens(san.c_str(), inner, sizeof(inner));
        if (const char* pretty = PrettyKeyName(inner))
            return pretty;
        return san.empty() ? std::string(stripped) : san;
    }

    const std::string* LookupHash(LanguageManager::Lang lang, unsigned hash)
    {
        const int li = static_cast<int>(lang);
        if (li < 0 || li >= kLangCount)
            return nullptr;
        auto it = s_maps[li].find(hash);
        if (it != s_maps[li].end() && !it->second.empty())
            return &it->second;
        return nullptr;
    }

    const std::string* LookupKey(const char* key)
    {
        if (!key || !key[0])
            return nullptr;
        const unsigned hash = CKeyGen::GetUppercaseKey(key);
        const auto lang = LanguageManager::GetLanguage();
        if (const std::string* s = LookupHash(lang, hash))
            return s;
        if (lang != LanguageManager::Lang::American)
        {
            if (const std::string* s = LookupHash(LanguageManager::Lang::American, hash))
                return s;
        }
        return nullptr;
    }

    void IngestPhrases()
    {
        char path[MAX_PATH]{};
        if (!ModPaths::BuildDictionaryPath(path, sizeof(path), "help_phrases.txt"))
            return;

        DictionaryStore phrases;
        if (!phrases.LoadFile(path))
            return;

        phrases.Visit([&](const char* key, const std::array<std::string, DictionaryStore::kLangCount>& texts) {
            if (!key || !key[0])
                return;
            const unsigned hash = CKeyGen::GetUppercaseKey(key);
            for (int li = 0; li < kLangCount; ++li)
            {
                if (!texts[static_cast<size_t>(li)].empty())
                    s_maps[li][hash] = texts[static_cast<size_t>(li)];
            }
        });
    }

    void ExpandUtf8(const std::string& templ, std::string& out)
    {
        out.clear();
        out.reserve(templ.size() + 16);
        size_t i = 0;
        while (i < templ.size())
        {
            if (templ[i] != '~')
            {
                out.push_back(templ[i++]);
                continue;
            }

            if (templ.compare(i, 4, "~k~~") == 0)
            {
                const size_t nameEnd = templ.find('~', i + 4);
                if (nameEnd != std::string::npos)
                {
                    char buf[kMsgLen]{};
                    const size_t n = nameEnd + 1 - i;
                    if (n > 0 && n < 64)
                    {
                        memcpy(buf, templ.c_str() + i, n);
                        buf[n] = 0;
                        CMessages::InsertPlayerControlKeysInString(buf);
                        buf[kMsgLen - 1] = 0;
                        const std::string keys = DecodeInsertedKeys(buf);
                        out += keys.empty() ? buf : keys;
                        i = nameEnd + 1;
                        continue;
                    }
                }
            }

            const size_t end = templ.find('~', i + 1);
            if (end == std::string::npos)
            {
                out.push_back(templ[i++]);
                continue;
            }

            const size_t inner = end - (i + 1);
            if (inner == 1)
            {
                const char t = templ[i + 1];
                if (t == 'n' || t == 'N')
                    out.push_back('\n');
                else if (t >= '1' && t <= '6')
                {
                    const int idx = t - '1';
                    if (s_helpNums[idx] >= 0)
                    {
                        char num[16];
                        sprintf_s(num, "%d", s_helpNums[idx]);
                        out += num;
                    }
                }
            }
            i = end + 1;
        }
    }

    const char* __fastcall Get_Detour(CText* self, void*, const char* key)
    {
        const char* r = s_hookGet.thiscall<const char*>(self, key);
        RememberGet(key, r);
        return r;
    }

    void __cdecl SetHelp_Detour(const char* text, bool quick, bool perm, bool brief)
    {
        ResolveHelpKey(text);
        if (text && text[0])
            ParseLiveNumbers(text);
        s_hookSet.ccall<void>(text, quick, perm, brief);
    }

    void __cdecl SetHelpNum_Detour(const char* text, int number, bool quick, bool perm)
    {
        ResolveHelpKey(text);
        s_helpNums[0] = number;
        s_hookSetNum.ccall<void>(text, number, quick, perm);
    }
}

void HelpGxt::Load()
{
    if (s_loaded)
        return;
    s_loaded = true;

    char dir[MAX_PATH]{};
    GameTextDir(dir, sizeof(dir));
    if (dir[0])
    {
        char path[MAX_PATH];
        auto tryLoad = [&](const char* file, LanguageManager::Lang lang, bool detect) {
            sprintf_s(path, "%s\\%s", dir, file);
            LoadOneFile(path, lang, detect);
        };

        tryLoad("american.gxt", LanguageManager::Lang::American, true);
        tryLoad("french.gxt", LanguageManager::Lang::French, false);
        tryLoad("german.gxt", LanguageManager::Lang::German, false);
        tryLoad("italian.gxt", LanguageManager::Lang::Italian, false);
        tryLoad("spanish.gxt", LanguageManager::Lang::Spanish, false);
        tryLoad("russian.gxt", LanguageManager::Lang::Russian, true);
        tryLoad("portuguese.gxt", LanguageManager::Lang::Portuguese, false);
        tryLoad("brazilian.gxt", LanguageManager::Lang::Brazilian, false);
    }

    TryLoadSanLtdRussian();
    // Dictionary last — UTF-8 RU written from SanLTD/1C decode.
    IngestPhrases();
}

void HelpGxt::Install()
{
    Load();
    if (s_hooksOn)
        return;
    s_hookGet = safetyhook::create_inline(reinterpret_cast<void*>(kCTextGet), Get_Detour);
    s_hookSet = safetyhook::create_inline(reinterpret_cast<void*>(kSetHelp), SetHelp_Detour);
    s_hookSetNum = safetyhook::create_inline(reinterpret_cast<void*>(kSetHelpNumber), SetHelpNum_Detour);
    s_hooksOn = static_cast<bool>(s_hookGet) && static_cast<bool>(s_hookSet);
}

void HelpGxt::Shutdown()
{
    s_hookGet.reset();
    s_hookSet.reset();
    s_hookSetNum.reset();
    s_hooksOn = false;
}

const char* HelpGxt::Get(const char* key)
{
    Load();
    if (const std::string* s = LookupKey(key))
        return s->c_str();
    return nullptr;
}

bool HelpGxt::Format(const char* liveGxt, char* utf8, size_t cap)
{
    if (!utf8 || cap < 2)
        return false;
    utf8[0] = 0;
    Load();

    if (!s_helpKey[0])
        return false;

    const std::string* templ = LookupKey(s_helpKey);
    if (!templ || templ->empty())
        return false;

    ParseLiveNumbers(liveGxt);
    std::string expanded;
    ExpandUtf8(*templ, expanded);
    if (expanded.empty())
        return false;
    strncpy_s(utf8, cap, expanded.c_str(), _TRUNCATE);
    return utf8[0] != 0;
}

bool HelpGxt::FormatMarkup(const char* gxt, char* utf8, size_t cap)
{
    if (!utf8 || cap < 2)
        return false;
    utf8[0] = 0;
    if (!gxt || !gxt[0])
        return false;

    Load();

    std::string out;
    out.reserve(256);
    size_t i = 0;
    const size_t len = std::strlen(gxt);
    while (i < len)
    {
        if (gxt[i] != '~')
        {
            out.push_back(gxt[i++]);
            continue;
        }

        if (i + 4 < len && gxt[i + 1] == 'k' && gxt[i + 2] == '~' && gxt[i + 3] == '~')
        {
            const size_t nameEnd = std::string(gxt).find('~', i + 4);
            if (nameEnd != std::string::npos)
            {
                char buf[kMsgLen]{};
                const size_t n = nameEnd + 1 - i;
                if (n > 0 && n < 64)
                {
                    memcpy(buf, gxt + i, n);
                    buf[n] = 0;
                    CMessages::InsertPlayerControlKeysInString(buf);
                    buf[kMsgLen - 1] = 0;
                    const std::string keys = DecodeInsertedKeys(buf);
                    out += keys.empty() ? buf : keys;
                    i = nameEnd + 1;
                    continue;
                }
            }
        }

        const size_t end = std::string(gxt).find('~', i + 1);
        if (end == std::string::npos)
        {
            out.push_back(gxt[i++]);
            continue;
        }

        const size_t inner = end - (i + 1);
        if (inner == 1)
        {
            const char t = gxt[i + 1];
            if (t == 'n' || t == 'N')
                out.push_back('\n');
        }
        i = end + 1;
    }

    if (out.empty())
        return false;

    {
        const std::string decoded = SaveSlots::DecodeHudText(out.c_str());
        if (!decoded.empty())
            strncpy_s(utf8, cap, decoded.c_str(), _TRUNCATE);
        else
            strncpy_s(utf8, cap, out.c_str(), _TRUNCATE);
    }
    return utf8[0] != 0;
}
