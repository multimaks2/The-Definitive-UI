#pragma once

#include <string>

namespace ModPaths
{
    std::string GetPluginDirectory();
    std::string GetModDataDirectory();
    std::string GetShaderDirectory();
    std::string GetDictionaryDirectory();

    bool BuildModPath(char* out, size_t cap, const char* relativePath);
    bool BuildDictionaryPath(char* out, size_t cap, const char* fileName);
    bool BuildShaderPath(char* out, size_t cap, const char* fileName);
}
