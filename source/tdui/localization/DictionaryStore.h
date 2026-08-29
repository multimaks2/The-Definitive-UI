#pragma once

#include <array>
#include <cstddef>
#include <string>
#include <unordered_map>

class DictionaryStore
{
public:
    static constexpr int kLangCount = 8;

    bool LoadFile(const char* path);
    const char* Get(const char* key, int langIndex) const;
    size_t Size() const { return m_entries.size(); }
    void Clear() { m_entries.clear(); }

    template <typename Fn>
    void Visit(Fn&& fn) const
    {
        for (const auto& kv : m_entries)
            fn(kv.first.c_str(), kv.second);
    }

private:
    std::unordered_map<std::string, std::array<std::string, kLangCount>> m_entries;
};
