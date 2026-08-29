/*****************************************************************************
 *  PROJECT:     The-Definitive-UI
 *  FILE:        source/Radar/mapmanager/RadarOverlayCompat.cpp
 *  PURPOSE:     Dispatch third-party plugin-sdk radar overlay / blips callbacks
 *****************************************************************************/

#include "RadarOverlayCompat.h"
#include "StockRadarDraw.h"

#include "CGangWars.h"
#include "CRadar.h"

#include <Windows.h>
#include <cstdint>
#include <cstring>

namespace
{
    constexpr uintptr_t kHudOverlayCallSite = 0x5869BF;
    constexpr uintptr_t kPauseOverlayCallSite = 0x5759E4;
    // plugin-sdk drawBlipsEvent: HUD = H_JUMP, pause = H_CALL
    constexpr uintptr_t kHudBlipsCallSite = 0x58AA2D;
    constexpr uintptr_t kPauseBlipsCallSite = 0x575B44;
    constexpr uintptr_t kDrawBlipsFn = 0x588050;
    constexpr unsigned char kCallRel32 = 0xE8;
    constexpr unsigned char kJmpRel32 = 0xE9;

    using OverlayChainFn = void(__cdecl*)(bool);
    using BlipsChainFn = void(__cdecl*)();

    enum class CompatContext
    {
        None,
        HudOverlay,
        PauseOverlay,
        HudBlips,
        PauseBlips
    };

    CompatContext s_context = CompatContext::None;

    bool IsExecutableAddress(uintptr_t address)
    {
        MEMORY_BASIC_INFORMATION mbi{};
        if (!address || !VirtualQuery(reinterpret_cast<void*>(address), &mbi, sizeof(mbi)))
            return false;
        if (mbi.State != MEM_COMMIT || (mbi.Protect & (PAGE_GUARD | PAGE_NOACCESS)))
            return false;

        const DWORD protection = mbi.Protect & 0xFFu;
        return protection == PAGE_EXECUTE
            || protection == PAGE_EXECUTE_READ
            || protection == PAGE_EXECUTE_READWRITE
            || protection == PAGE_EXECUTE_WRITECOPY;
    }

    uintptr_t ResolveRel32Target(uintptr_t callSite)
    {
        const unsigned char op = *reinterpret_cast<const unsigned char*>(callSite);
        if (op != kCallRel32 && op != kJmpRel32)
            return 0;

        int32_t relative = 0;
        std::memcpy(&relative, reinterpret_cast<const void*>(callSite + 1), sizeof(relative));
        const uintptr_t target = callSite + 5 + static_cast<intptr_t>(relative);
        if (target == callSite || !IsExecutableAddress(target))
            return 0;
        return target;
    }

    uintptr_t ResolveChain(uintptr_t preferred, uintptr_t fallback)
    {
        if (uintptr_t target = ResolveRel32Target(preferred))
            return target;
        return ResolveRel32Target(fallback);
    }

    class ReentryGuard
    {
    public:
        explicit ReentryGuard(CompatContext context)
            : m_entered(s_context == CompatContext::None)
        {
            if (m_entered)
                s_context = context;
        }

        ~ReentryGuard()
        {
            if (m_entered)
                s_context = CompatContext::None;
        }

        bool Entered() const { return m_entered; }

    private:
        bool m_entered;
    };

    class GangOverlayGuard
    {
    public:
        explicit GangOverlayGuard(bool suppress)
            : m_suppress(suppress)
            , m_saved(CGangWars::bGangWarsActive)
        {
            if (m_suppress)
                CGangWars::bGangWarsActive = false;
        }

        ~GangOverlayGuard()
        {
            if (m_suppress)
                CGangWars::bGangWarsActive = m_saved;
        }

    private:
        bool m_suppress;
        bool m_saved;
    };

    class PipelineSyncGuard
    {
    public:
        ~PipelineSyncGuard()
        {
            StockRadarDraw::SanitizeDrawState();
        }
    };

    void InvokeOverlay(uintptr_t preferred, uintptr_t fallback, CompatContext context,
                       bool inMenu, bool drawStockGangOverlay)
    {
        ReentryGuard reentry(context);
        if (!reentry.Entered())
            return;

        const uintptr_t target = ResolveChain(preferred, fallback);
        if (!target)
            return;

        PipelineSyncGuard pipelineSync;
        GangOverlayGuard gangOverlay(!drawStockGangOverlay);

        try
        {
            reinterpret_cast<OverlayChainFn>(target)(inMenu);
        }
        catch (...)
        {
        }
    }

    // Do NOT hook CRadar::DrawBlips (0x588050). A 5-byte steal cuts into
    // `and esp, -8` (83 E4 F8) and the gateway jumps to NULL / garbage.
    // Call the live call-site trampoline instead: stock DrawBlips + AFTER
    // (CopNThreat). Unpatched site resolves to DrawBlips itself.
    void InvokeBlips(uintptr_t preferred, uintptr_t fallback, CompatContext context)
    {
        ReentryGuard reentry(context);
        if (!reentry.Entered())
            return;

        PipelineSyncGuard pipelineSync;

        const uintptr_t target = ResolveChain(preferred, fallback);
        if (!target)
        {
            CRadar::DrawBlips();
            return;
        }

        try
        {
            reinterpret_cast<BlipsChainFn>(target)();
        }
        catch (...)
        {
        }
    }
}

void RadarOverlayCompat::InvokeHudOverlay()
{
    InvokeOverlay(kHudOverlayCallSite, kPauseOverlayCallSite, CompatContext::HudOverlay, false, false);
}

void RadarOverlayCompat::InvokePauseMapOverlay(bool drawStockGangOverlay)
{
    InvokeOverlay(kPauseOverlayCallSite, kHudOverlayCallSite, CompatContext::PauseOverlay, true, drawStockGangOverlay);
}

void RadarOverlayCompat::InvokeHudBlips()
{
    InvokeBlips(kHudBlipsCallSite, kPauseBlipsCallSite, CompatContext::HudBlips);
}

void RadarOverlayCompat::InvokePauseMapBlips()
{
    InvokeBlips(kPauseBlipsCallSite, kHudBlipsCallSite, CompatContext::PauseBlips);
}

bool RadarOverlayCompat::IsInvokingHudOverlay()
{
    return s_context == CompatContext::HudOverlay;
}

bool RadarOverlayCompat::IsInvokingPauseMapOverlay()
{
    return s_context == CompatContext::PauseOverlay;
}

bool RadarOverlayCompat::IsInvokingHudBlips()
{
    return s_context == CompatContext::HudBlips;
}

bool RadarOverlayCompat::IsInvokingPauseMapBlips()
{
    return s_context == CompatContext::PauseBlips;
}
