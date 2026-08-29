#include "DictionaryStore.h"

#include <cctype>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <sstream>

namespace
{
    std::string Trim(std::string s)
    {
        while (!s.empty() && (s.back() == '\r' || s.back() == '\n' || s.back() == ' ' || s.back() == '\t'))
            s.pop_back();
        size_t i = 0;
        while (i < s.size() && (s[i] == ' ' || s[i] == '\t'))
            ++i;
        return s.substr(i);
    }

    void UnescapeValue(std::string& s)
    {
        std::string out;
        out.reserve(s.size());
        for (size_t i = 0; i < s.size(); ++i)
        {
            if (s[i] == '\\' && i + 1 < s.size())
            {
                switch (s[i + 1])
                {
                case 'n': out.push_back('\n'); i++; continue;
                case 'r': out.push_back('\r'); i++; continue;
                case 't': out.push_back('\t'); i++; continue;
                case '\\': out.push_back('\\'); i++; continue;
                case '"': out.push_back('"'); i++; continue;
                default: break;
                }
            }
            out.push_back(s[i]);
        }
        s.swap(out);
    }

    int LangIndexFromTag(const char* tag)
    {
        if (!tag || !tag[0])
            return -1;
        if (_stricmp(tag, "EN") == 0) return 0;
        if (_stricmp(tag, "FR") == 0) return 1;
        if (_stricmp(tag, "DE") == 0) return 2;
        if (_stricmp(tag, "IT") == 0) return 3;
        if (_stricmp(tag, "ES") == 0) return 4;
        if (_stricmp(tag, "RU") == 0) return 5;
        if (_stricmp(tag, "PT") == 0) return 6;
        if (_stricmp(tag, "BR") == 0) return 7;
        return -1;
    }

    bool ParseValueLine(const std::string& line, int& langIndex, std::string& value)
    {
        const size_t eq = line.find('=');
        if (eq == std::string::npos || eq == 0)
            return false;

        std::string tag = line.substr(0, eq);
        value = line.substr(eq + 1);
        langIndex = LangIndexFromTag(tag.c_str());
        if (langIndex < 0)
            return false;
        UnescapeValue(value);
        return true;
    }
}

bool DictionaryStore::LoadFile(const char* path)
{
    if (!path || !path[0])
        return false;

    std::ifstream in(path, std::ios::binary);
    if (!in)
        return false;

    std::string line;
    std::string currentKey;
    std::array<std::string, kLangCount> current{};

    auto commit = [&]() {
        if (!currentKey.empty())
            m_entries[currentKey] = current;
        currentKey.clear();
        current = {};
    };

    while (std::getline(in, line))
    {
        if (!line.empty() && static_cast<unsigned char>(line[0]) == 0xEF && line.size() >= 3
            && static_cast<unsigned char>(line[1]) == 0xBB && static_cast<unsigned char>(line[2]) == 0xBF)
            line.erase(0, 3);

        line = Trim(line);
        if (line.empty() || line[0] == '#')
            continue;

        if (line.front() == '[' && line.back() == ']')
        {
            commit();
            currentKey = line.substr(1, line.size() - 2);
            continue;
        }

        int langIndex = -1;
        std::string value;
        if (!currentKey.empty() && ParseValueLine(line, langIndex, value))
            current[static_cast<size_t>(langIndex)] = std::move(value);
    }

    commit();
    return !m_entries.empty();
}

const char* DictionaryStore::Get(const char* key, int langIndex) const
{
    if (!key || !key[0] || langIndex < 0 || langIndex >= kLangCount)
        return nullptr;

    const auto it = m_entries.find(key);
    if (it == m_entries.end())
        return nullptr;

    const std::string& s = it->second[static_cast<size_t>(langIndex)];
    if (!s.empty())
        return s.c_str();

    const std::string& en = it->second[0];
    return en.empty() ? nullptr : en.c_str();
}
