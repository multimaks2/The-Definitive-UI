/*****************************************************************************
 *
 *  PROJECT:     The-Definitive-UI
 *  FILE:        source/Game/GameState.cpp
 *  PURPOSE:     Stock radar visibility rules - interiors, radar mode, HUD flash
 *
 *****************************************************************************/

#include "GameState.h"

#include "CGame.h"
#include "CPed.h"
#include "CPlayerPed.h"
#include "CMenuManager.h"
#include "CEntryExitManager.h"
#include "CHud.h"
#include "CTimer.h"
#include "common.h"

namespace
{
    constexpr int kRadarBlipsOnly    = 1;
    constexpr int kRadarOff          = 2;
    constexpr int kAreaNormalWorld   = 0;
    constexpr int kExitEnterHide1    = 1;
    constexpr int kExitEnterHide2    = 2;
    constexpr int kRadarFlashMask    = 8; // EachFrames(8): hide when (frame & 8) == 0
}

bool GameState::IsPlayerInInterior()
{
    if (CGame::currArea != kAreaNormalWorld)
        return true;
    CPed* ped = FindPlayerPed();
    return ped && ped->m_nAreaCode != kAreaNormalWorld;
}

bool GameState::ShouldDrawRadar()
{
    if (FrontEndMenuManager.m_nPrefsRadarMode == kRadarOff)
        return false;
    const int enterExit = CEntryExitManager::ms_exitEnterState;
    if (enterExit == kExitEnterHide1 || enterExit == kExitEnterHide2)
        return false;
    if (CHud::m_ItemToFlash == ITEM_RADAR && (CTimer::m_FrameCounter & kRadarFlashMask) == 0)
        return false;
    return true;
}

bool GameState::ShouldDrawRadarRim()
{
    return FrontEndMenuManager.m_nPrefsRadarMode != kRadarBlipsOnly;
}

bool GameState::ShouldDrawRadarMap()
{
    if (!ShouldDrawRadar())
        return false;
    if (!ShouldDrawRadarRim())
        return false;
    if (IsPlayerInInterior())
        return false;
    return true;
}
