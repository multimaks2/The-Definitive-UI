/*****************************************************************************
 *
 *  PROJECT:     The-Definitive-UI
 *  FILE:        source/Help/HelpRenderer.cpp
 *  PURPOSE:     CHud::DrawHelpText (0x58B6E0) — timers from stock, Draw font like radio
 *
 *****************************************************************************/

#include "HelpRenderer.h"
#include "Draw.h"
#include "ColorUtils.h"
#include "SaveSlots.h"
#include "HelpGxt.h"

#include "plugin.h"
#include "CHud.h"
#include "CMessages.h"
#include "CTimer.h"
#include "CStats.h"
#include "RenderWare.h"

#include <algorithm>
#include <cstring>

namespace
{
    constexpr float kBaseW  = 640.0f;
    constexpr float kBaseH  = 448.0f;
    constexpr float kPosX   = 34.0f;
    constexpr float kPosY   = 28.0f;
    constexpr int   kFadeMs = 1000;
    constexpr int   kMsgLen = 400;
    constexpr int   kRadioFontH = 32;
    constexpr int   kOutlineOff[8][2] = {
        { -1, -1 }, { 0, -1 }, { 1, -1 },
        { -1,  0 },            { 1,  0 },
        { -1,  1 }, { 0,  1 }, { 1,  1 }
    };

    float StretchX(float v)
    {
        return v * static_cast<float>(RsGlobal.maximumWidth) / kBaseW;
    }

    float StretchY(float v)
    {
        return v * static_cast<float>(RsGlobal.maximumHeight) / kBaseH;
    }

    bool StringsEqual(const char* a, const char* b)
    {
        return CMessages::StringCompare(a, b, kMsgLen) != 0;
    }

    int HelpFontHeight()
    {
        const int h = static_cast<int>(kRadioFontH * static_cast<float>(RsGlobal.maximumHeight) / 1080.0f + 0.5f);
        return h < 16 ? 16 : h;
    }
}

HelpRenderer::HelpRenderer()
    : m_pDraw(nullptr)
{
}

void HelpRenderer::SetDraw(Draw* draw)
{
    m_pDraw = draw;
}

int HelpRenderer::TimeStepMs()
{
    int dt = static_cast<int>(CTimer::ms_fTimeStep * 0.02f * 1000.0f);
    return dt < 1 ? 1 : dt;
}

void HelpRenderer::ResetMessage()
{
    CHud::m_pHelpMessage[0] = 0;
    CHud::m_pHelpMessageToPrint[0] = 0;
    CHud::m_pLastHelpMessage[0] = 0;
    CHud::m_nHelpMessageState = 0;
    CHud::m_nHelpMessageFadeTimer = 0;
    CHud::m_nHelpMessageTimer = 0;
    CHud::m_bHelpMessagePermanent = false;
    CHud::m_bHelpMessageQuick = false;
    CHud::m_nHelpMessageStatId = 0;
    CHud::m_nHelpMessageMaxStatValue = 1000;
    CHud::m_fHelpMessageStatUpdateValue = 0.0f;
    CHud::m_fHelpMessageBoxWidth = 200.0f;
}

void HelpRenderer::UpdateState()
{
    if (!CHud::m_pHelpMessage[0])
    {
        if (CHud::m_nHelpMessageState == 2 || CHud::m_nHelpMessageState == 4)
        {
            CHud::m_nHelpMessageState = 3;
            CHud::m_nHelpMessageFadeTimer = kFadeMs;
        }
        else if (CHud::m_nHelpMessageState != 3)
        {
            ResetMessage();
            return;
        }
    }
    else if (!StringsEqual(CHud::m_pHelpMessage, CHud::m_pLastHelpMessage))
    {
        switch (CHud::m_nHelpMessageState)
        {
        case 0:
            CHud::m_nHelpMessageState = 2;
            CHud::m_nHelpMessageTimer = 0;
            CHud::m_nHelpMessageFadeTimer = 0;
            CHud::m_fHelpMessageTime =
                static_cast<float>(CMessages::GetStringLength(CHud::m_pHelpMessage)) / 20.0f + 3.0f;
            if (CHud::m_bHelpMessageQuick)
                CHud::m_fHelpMessageTime = 1.5f;
            CMessages::StringCopy(CHud::m_pHelpMessageToPrint, CHud::m_pHelpMessage, kMsgLen);
            break;
        case 1:
        case 2:
        case 3:
        case 4:
            CHud::m_nHelpMessageState = 3;
            CHud::m_nHelpMessageFadeTimer = kFadeMs;
            CHud::m_nHelpMessageTimer = 5;
            CMessages::StringCopy(CHud::m_pLastHelpMessage, CHud::m_pHelpMessage, kMsgLen);
            return;
        default:
            break;
        }
        CMessages::StringCopy(CHud::m_pLastHelpMessage, CHud::m_pHelpMessage, kMsgLen);
    }

    const int dt = TimeStepMs();

    switch (CHud::m_nHelpMessageState)
    {
    case 1:
        CHud::m_nHelpMessageFadeTimer = kFadeMs;
        break;
    case 2:
        CHud::m_nHelpMessageFadeTimer += dt;
        CHud::m_nHelpMessageTimer += dt;
        if (CHud::m_nHelpMessageFadeTimer >= kFadeMs)
        {
            CHud::m_nHelpMessageFadeTimer = kFadeMs;
            CHud::m_nHelpMessageState = CHud::m_bHelpMessagePermanent ? 1 : 4;
        }
        break;
    case 3:
        CHud::m_nHelpMessageFadeTimer -= dt;
        CHud::m_nHelpMessageTimer += dt;
        if (CHud::m_nHelpMessageFadeTimer <= 0)
        {
            const bool more = CHud::m_pHelpMessage[0] != 0
                && !StringsEqual(CHud::m_pHelpMessage, CHud::m_pHelpMessageToPrint);
            CHud::m_nHelpMessageFadeTimer = 0;
            CHud::m_nHelpMessageState = 0;
            CHud::m_pLastHelpMessage[0] = 0;
            if (!more)
                ResetMessage();
        }
        break;
    case 4:
        CHud::m_nHelpMessageFadeTimer = kFadeMs;
        CHud::m_nHelpMessageTimer += dt;
        if (!CHud::m_bHelpMessagePermanent
            && static_cast<float>(CHud::m_nHelpMessageTimer) >= CHud::m_fHelpMessageTime * 1000.0f)
        {
            CHud::m_nHelpMessageState = 3;
            CHud::m_nHelpMessageFadeTimer = kFadeMs;
        }
        break;
    default:
        break;
    }
}

unsigned char HelpRenderer::MessageAlpha()
{
    switch (CHud::m_nHelpMessageState)
    {
    case 1:
    case 4:
        return 255;
    case 2:
    case 3:
    {
        int a = CHud::m_nHelpMessageFadeTimer * 255 / kFadeMs;
        if (a < 0)
            a = 0;
        if (a > 255)
            a = 255;
        return static_cast<unsigned char>(a);
    }
    default:
        return 0;
    }
}

void HelpRenderer::GxtToUtf8(const char* gxt, char* utf8, size_t cap)
{
    if (!utf8 || cap < 2)
        return;
    utf8[0] = 0;

    if (HelpGxt::Format(gxt, utf8, cap))
        return;

    const std::string decoded = SaveSlots::DecodeHudText(gxt);
    if (decoded.empty())
        return;
    strncpy_s(utf8, cap, decoded.c_str(), _TRUNCATE);
}

void HelpRenderer::DrawHelp(const char* utf8, float x, float y, float wrapW,
                            bool hasStat, unsigned char alpha) const
{
    ID3DXFont* font = m_pDraw->GetFont();
    if (!font)
        return;

    wchar_t wide[512]{};
    if (MultiByteToWideChar(CP_UTF8, 0, utf8, -1, wide, 512) <= 0)
        return;

    if (wrapW < 8.0f)
        wrapW = 8.0f;

    m_pDraw->BeginUi();

    RECT calc = { 0, 0, static_cast<LONG>(wrapW + 0.5f), 0 };
    font->DrawTextW(nullptr, wide, -1, &calc, DT_CALCRECT | DT_WORDBREAK | DT_LEFT | DT_TOP, 0xFFFFFFFF);
    const float textH = static_cast<float>(calc.bottom);
    if (textH < 1.0f)
    {
        m_pDraw->EndUi();
        return;
    }

    const float padX = StretchX(8.0f);
    const float padY = StretchY(6.0f);
    float boxW = wrapW + padX * 2.0f;
    float boxH = textH + padY * 2.0f;

    float barX = 0.0f, barY = 0.0f, barW = 0.0f, barH = 0.0f;
    if (hasStat)
    {
        barW = StretchX(100.0f);
        barH = StretchY(10.0f);
        barX = x;
        barY = y + textH + StretchY(4.0f);
        boxH += barH + StretchY(6.0f);
        if (boxW < barW + padX * 2.0f)
            boxW = barW + padX * 2.0f;
    }

    const float boxX = x - padX;
    const float boxY = y - padY;
    const DWORD fmt = DT_LEFT | DT_TOP | DT_WORDBREAK;
    const DWORD outline = tocolor(0, 0, 0, alpha);
    const DWORD textCol = tocolor(255, 255, 255, alpha);

    m_pDraw->DrawRect(boxX, boxY, boxW, boxH, tocolor(0, 0, 0, static_cast<unsigned char>(alpha * 180 / 255)));

    if (hasStat && barW > 1.0f)
    {
        float maxv = static_cast<float>(CHud::m_nHelpMessageMaxStatValue);
        if (maxv < 1.0f)
            maxv = 1.0f;
        float pct = std::clamp(CStats::GetStatValue(CHud::m_nHelpMessageStatId) / maxv, 0.0f, 1.0f);
        const bool up = CHud::m_fHelpMessageStatUpdateValue >= 0.0f;
        m_pDraw->DrawRect(barX, barY, barW, barH, tocolor(40, 40, 40, alpha));
        m_pDraw->DrawRect(barX, barY, barW * pct, barH,
                          up ? tocolor(72, 180, 72, alpha) : tocolor(180, 48, 48, alpha));
        m_pDraw->DrawRect(barX, barY, barW, StretchY(1.5f), tocolor(0, 0, 0, alpha));
        m_pDraw->DrawRect(barX, barY + barH - StretchY(1.5f), barW, StretchY(1.5f), tocolor(0, 0, 0, alpha));
        m_pDraw->DrawRect(barX, barY, StretchX(1.5f), barH, tocolor(0, 0, 0, alpha));
        m_pDraw->DrawRect(barX + barW - StretchX(1.5f), barY, StretchX(1.5f), barH, tocolor(0, 0, 0, alpha));
    }

    for (int i = 0; i < 8; ++i)
    {
        const float ox = static_cast<float>(kOutlineOff[i][0]);
        const float oy = static_cast<float>(kOutlineOff[i][1]);
        m_pDraw->DrawString(x + ox, y + oy, x + ox + wrapW, y + oy + textH,
                            outline, utf8, 1.0f, 1.0f, fmt);
    }
    m_pDraw->DrawString(x, y, x + wrapW, y + textH, textCol, utf8, 1.0f, 1.0f, fmt);

    m_pDraw->EndUi();
    // EndUi already: VS/PS null + PoisonRwShaderCache
}

void HelpRenderer::Render()
{
    UpdateState();

    const unsigned char alpha = MessageAlpha();
    if (alpha < 1 || !CHud::m_pHelpMessageToPrint[0] || CHud::m_pHelpMessageToPrint[0] == '*')
        return;
    if (!m_pDraw || !m_pDraw->IsInitialized())
        return;

    char utf8[512];
    GxtToUtf8(CHud::m_pHelpMessageToPrint, utf8, sizeof(utf8));
    if (!utf8[0])
        return;

    const int fontH = HelpFontHeight();
    m_pDraw->EnsureFontHeight(fontH);

    float x = StretchX(kPosX);
    const float y = StretchY(kPosY);
    if (CHud::bDrawingVitalStats)
        x = StretchX(kPosX + 155.0f);

    const float wrapW = StretchX(CHud::m_fHelpMessageBoxWidth);
    DrawHelp(utf8, x, y, wrapW, CHud::m_nHelpMessageStatId != 0, alpha);
}
