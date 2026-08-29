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
#include "RenderWare.h"
#include "ePedState.h"
#include "CBmx.h"
#include "CVehicle.h"
#include "common.h"
#include "safetyhook.hpp"

#include <windows.h>

namespace
{
    constexpr uintptr_t kUpdateStatsWhenCycling = 0x55BB80; // CBmx::PreRender call site 0x6C0DE1, US 1.0
    constexpr uintptr_t kSetHelpMessageStatUpdate = 0x588D40;
    constexpr uintptr_t kStatMessageKeyLookup     = 0x55C620; // strcmp dispatch (crash at 0x55C7BB)

    SafetyHookInline s_updateStatsWhenCycling;
    SafetyHookInline s_setHelpMessageStatUpdate;
    SafetyHookInline s_statMessageKeyLookup;
    bool s_gameplayGuardsInstalled = false;

    bool IsReadableCStr(const char* text, size_t maxLen = 128)
    {
        if (!text)
            return false;

        MEMORY_BASIC_INFORMATION mbi{};
        if (!VirtualQuery(text, &mbi, sizeof(mbi)))
            return false;
        if (mbi.State != MEM_COMMIT)
            return false;
        if (mbi.Protect & (PAGE_NOACCESS | PAGE_GUARD))
            return false;

        const uintptr_t regionEnd =
            reinterpret_cast<uintptr_t>(mbi.BaseAddress) + mbi.RegionSize;
        uintptr_t cur = reinterpret_cast<uintptr_t>(text);
        for (size_t i = 0; i < maxLen; ++i)
        {
            if (cur >= regionEnd)
                return false;
            if (*reinterpret_cast<const char*>(cur) == '\0')
                return true;
            ++cur;
        }
        return false;
    }

    bool IsSafeForGameplayStatMessage()
    {
        CPed* ped = FindPlayerPed();
        if (!ped)
            return false;
        return !GameState::IsPedInVehicleTransition(ped);
    }

    bool IsSafeForCyclingStats(CBmx* bmx)
    {
        if (!bmx || !bmx->m_pDriver)
            return false;
        CPed* ped = FindPlayerPed();
        if (!ped || GameState::IsPedInVehicleTransition(ped))
            return false;
        if (bmx->m_pDriver != ped)
            return false;
        if (!ped->bInVehicle || ped->m_pVehicle != reinterpret_cast<CVehicle*>(bmx))
            return false;
        return true;
    }

    int __cdecl StatMessageKeyLookup_Detour(const char* key)
    {
        if (!IsReadableCStr(key))
            return 0;
        __try
        {
            return s_statMessageKeyLookup.ccall<int>(key);
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            return 0;
        }
    }

    void __cdecl UpdateStatsWhenCycling_Detour(bool sprinting, CBmx* bmx)
    {
        if (!IsSafeForCyclingStats(bmx))
            return;
        __try
        {
            s_updateStatsWhenCycling.ccall<void>(sprinting, bmx);
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
        }
    }

    void __cdecl SetHelpMessageStatUpdate_Detour(unsigned char state, unsigned short statId, float diff, float max)
    {
        if (!IsSafeForGameplayStatMessage())
            return;
        __try
        {
            s_setHelpMessageStatUpdate.ccall<void>(state, statId, diff, max);
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
        }
    }

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

bool GameState::CanRenderRadarPreview()
{
    return RwD3D9GetCurrentD3DDevice() != nullptr;
}

bool GameState::IsPedInVehicleTransition(CPed* ped)
{
    if (!ped)
        return true;
    switch (ped->m_ePedState)
    {
    case PEDSTATE_ENTER_CAR:
    case PEDSTATE_EXIT_CAR:
    case PEDSTATE_OPEN_DOOR:
    case PEDSTATE_CARJACK:
    case PEDSTATE_STEAL_CAR:
    case PEDSTATE_DRAGGED_FROM_CAR:
    case PEDSTATE_ENTER_TRAIN:
    case PEDSTATE_EXIT_TRAIN:
        return true;
    default:
        return false;
    }
}

CVector GameState::SafePlayerCentreForMap()
{
    CPed* ped = FindPlayerPed();
    if (!ped)
        return CVector(0.0f, 0.0f, 0.0f);

    CVector pos = ped->GetPosition();
    if (IsPedInVehicleTransition(ped))
        return pos;

    __try
    {
        pos = FindPlayerCentreOfWorld_NoInteriorShift(0);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
    }
    return pos;
}

void GameState::InstallGameplayGuards()
{
    if (s_gameplayGuardsInstalled)
        return;
    s_gameplayGuardsInstalled = true;

    if (!s_updateStatsWhenCycling)
    {
        s_updateStatsWhenCycling = safetyhook::create_inline(
            reinterpret_cast<void*>(kUpdateStatsWhenCycling), UpdateStatsWhenCycling_Detour);
    }
    if (!s_setHelpMessageStatUpdate)
    {
        s_setHelpMessageStatUpdate = safetyhook::create_inline(
            reinterpret_cast<void*>(kSetHelpMessageStatUpdate), SetHelpMessageStatUpdate_Detour);
    }
    if (!s_statMessageKeyLookup)
    {
        s_statMessageKeyLookup = safetyhook::create_inline(
            reinterpret_cast<void*>(kStatMessageKeyLookup), StatMessageKeyLookup_Detour);
    }
}
