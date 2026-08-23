/*****************************************************************************
 *
 *  PROJECT:     The-Definitive-UI
 *  FILE:        source/Utils/CrashDump.h
 *  PURPOSE:     Write a minidump next to the ASI on crash
 *
 *****************************************************************************/

#pragma once

#include <windows.h>

namespace CrashDump
{
    void Install();
    void Write(EXCEPTION_POINTERS* info);
    int  Filter(EXCEPTION_POINTERS* info);
}
