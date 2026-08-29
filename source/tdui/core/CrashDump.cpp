/*****************************************************************************
 *
 *  PROJECT:     The-Definitive-UI
 *  FILE:        source/Utils/CrashDump.cpp
 *  PURPOSE:     Write a minidump next to the ASI on crash
 *
 *****************************************************************************/

#include "pch.h"
#include "CrashDump.h"

#include <dbghelp.h>
#include <cstdio>

#pragma comment(lib, "dbghelp.lib")

namespace
{
    LPTOP_LEVEL_EXCEPTION_FILTER s_prevFilter = nullptr;
    PVOID s_veh = nullptr;
    volatile LONG s_written = 0;

    HMODULE ThisModule()
    {
        HMODULE mod = nullptr;
        GetModuleHandleExW(
            GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            reinterpret_cast<LPCWSTR>(&ThisModule),
            &mod);
        return mod;
    }

    bool PathFromModule(HMODULE mod, wchar_t* out, size_t outChars)
    {
        if (!out || outChars < 8)
            return false;

        wchar_t modulePath[MAX_PATH] = {};
        if (!GetModuleFileNameW(mod, modulePath, MAX_PATH))
            return false;

        wchar_t* slash = modulePath;
        for (wchar_t* p = modulePath; *p; ++p)
        {
            if (*p == L'\\' || *p == L'/')
                slash = p + 1;
        }

        SYSTEMTIME st{};
        GetLocalTime(&st);
        const int n = _snwprintf_s(
            out, outChars, _TRUNCATE,
            L"%.*sThe-Definitive-UI-crash-%04u%02u%02u-%02u%02u%02u.dmp",
            static_cast<int>(slash - modulePath), modulePath,
            st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
        return n > 0;
    }

    bool ShouldDump(DWORD code)
    {
        switch (code)
        {
        case EXCEPTION_ACCESS_VIOLATION:
        case EXCEPTION_IN_PAGE_ERROR:
        case EXCEPTION_ILLEGAL_INSTRUCTION:
        case EXCEPTION_STACK_OVERFLOW:
        case EXCEPTION_ARRAY_BOUNDS_EXCEEDED:
        case EXCEPTION_INT_DIVIDE_BY_ZERO:
        case EXCEPTION_PRIV_INSTRUCTION:
            return true;
        default:
            return false;
        }
    }

    bool WriteDumpFile(const wchar_t* path, EXCEPTION_POINTERS* info)
    {
        const HANDLE file = CreateFileW(
            path, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (file == INVALID_HANDLE_VALUE)
            return false;

        MINIDUMP_EXCEPTION_INFORMATION mei{};
        mei.ThreadId = GetCurrentThreadId();
        mei.ExceptionPointers = info;
        mei.ClientPointers = FALSE;

        MINIDUMP_TYPE types[] = {
            static_cast<MINIDUMP_TYPE>(
                MiniDumpWithIndirectlyReferencedMemory | MiniDumpWithThreadInfo | MiniDumpWithUnloadedModules),
            static_cast<MINIDUMP_TYPE>(MiniDumpWithDataSegs | MiniDumpWithThreadInfo | MiniDumpWithHandleData),
            MiniDumpNormal
        };

        bool ok = false;
        for (MINIDUMP_TYPE type : types)
        {
            SetFilePointer(file, 0, nullptr, FILE_BEGIN);
            SetEndOfFile(file);
            if (MiniDumpWriteDump(
                    GetCurrentProcess(),
                    GetCurrentProcessId(),
                    file,
                    type,
                    info ? &mei : nullptr,
                    nullptr,
                    nullptr))
            {
                ok = true;
                break;
            }
        }

        CloseHandle(file);
        return ok;
    }

    LONG CALLBACK VectoredHandler(EXCEPTION_POINTERS* info)
    {
        if (info && info->ExceptionRecord && ShouldDump(info->ExceptionRecord->ExceptionCode))
            CrashDump::Write(info);
        return EXCEPTION_CONTINUE_SEARCH;
    }

    LONG WINAPI UnhandledFilter(EXCEPTION_POINTERS* info)
    {
        CrashDump::Write(info);
        if (s_prevFilter)
            return s_prevFilter(info);
        return EXCEPTION_CONTINUE_SEARCH;
    }
}

void CrashDump::Write(EXCEPTION_POINTERS* info)
{
    if (InterlockedCompareExchange(&s_written, 1, 0) != 0)
        return;

    wchar_t path[MAX_PATH] = {};
    if (PathFromModule(ThisModule(), path, MAX_PATH) && WriteDumpFile(path, info))
        return;
    if (PathFromModule(nullptr, path, MAX_PATH))
        WriteDumpFile(path, info);
}

int CrashDump::Filter(EXCEPTION_POINTERS* info)
{
    Write(info);
    return EXCEPTION_CONTINUE_SEARCH;
}

void CrashDump::Install()
{
    if (!s_veh)
        s_veh = AddVectoredExceptionHandler(1, VectoredHandler);
    s_prevFilter = SetUnhandledExceptionFilter(UnhandledFilter);
}
