#include "ModPaths.h"

#include <Windows.h>

#include <cstring>

namespace
{
    std::string GetModuleDirectory()
    {
        char path[MAX_PATH]{};
        HMODULE module = nullptr;
        if (!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                                reinterpret_cast<LPCSTR>(&GetModuleDirectory), &module))
            return {};

        GetModuleFileNameA(module, path, MAX_PATH);
        std::string full(path);
        const auto slash = full.find_last_of("\\/");
        if (slash == std::string::npos)
            return {};
        return full.substr(0, slash + 1);
    }

    bool JoinPath(char* out, size_t cap, const std::string& base, const char* relative)
    {
        if (!out || cap == 0 || !relative)
            return false;
        const int written = sprintf_s(out, cap, "%s%s", base.c_str(), relative);
        return written > 0;
    }
}

namespace ModPaths
{
    std::string GetPluginDirectory()
    {
        return GetModuleDirectory();
    }

    std::string GetModDataDirectory()
    {
        return GetPluginDirectory() + "The-Definitive-UI.SA\\";
    }

    std::string GetShaderDirectory()
    {
        return GetModDataDirectory() + "shaders\\";
    }

    std::string GetDictionaryDirectory()
    {
        return GetModDataDirectory() + "ui-text\\dictionaries\\";
    }

    bool BuildModPath(char* out, size_t cap, const char* relativePath)
    {
        return JoinPath(out, cap, GetModDataDirectory(), relativePath);
    }

    bool BuildDictionaryPath(char* out, size_t cap, const char* fileName)
    {
        return JoinPath(out, cap, GetDictionaryDirectory(), fileName);
    }

    bool BuildShaderPath(char* out, size_t cap, const char* fileName)
    {
        return JoinPath(out, cap, GetShaderDirectory(), fileName);
    }
}
