/*****************************************************************************
 *
 *  PROJECT:     The-Definitive-UI
 *  FILE:        source/Help/HelpGxt.h
 *  PURPOSE:     Help HUD — identify by GXT key; text from HelpPhrases.inc + TEXT/*.gxt
 *
 *****************************************************************************/

#pragma once

#include <cstddef>

class HelpGxt
{
public:
    static void Install();
    static void Shutdown();
    static void Load();

    // UTF-8 for current UI language. Key comes from CText::Get / SetHelpMessage, not 1C marks.
    static bool Format(const char* liveGxt, char* utf8, size_t cap);
    // Phrase table / GXT lookup (FEC_*, FET_*). nullptr if unknown.
    static const char* Get(const char* key);
};
