/*****************************************************************************
 *
 *  PROJECT:     The-Definitive-UI
 *  FILE:        source/Radar/render/RenderRadio.cpp
 *  PURPOSE:     Radio station name rendering with the custom font
 *
 *****************************************************************************/

#include "RenderRadio.h"
#include "DxDrawPrimitives.h"
#include "ColorUtils.h"

#include <windows.h>

RenderRadio::RenderRadio(DxDrawPrimitives* pDraw)
    : m_pDraw(pDraw)
    , m_pFont(nullptr)
    , m_pFontSprite(nullptr)
    , m_bShadowEnabled(true)
    , m_shadowColor(tocolor(0, 0, 0, 200))
{
}

RenderRadio::~RenderRadio()
{
    if (m_pFontSprite)
    {
        m_pFontSprite->Release();
        m_pFontSprite = nullptr;
    }
    // Шрифт не освобождаем - он создаётся и управляется внешним кодом (RadarRenderer)
    m_pFont = nullptr;
}

void RenderRadio::SetFont(LPD3DXFONT pFont)
{
    if (m_pFont != pFont)
    {
        m_pFont = pFont;

        // Освобождаем старый спрайт
        if (m_pFontSprite)
        {
            m_pFontSprite->Release();
            m_pFontSprite = nullptr;
        }

        // Создаём новый спрайт для шрифта
        // Получаем устройство через m_pDraw, т.к. ID3DXFont::GetDevice() требует параметр
        if (m_pFont && m_pDraw)
        {
            LPDIRECT3DDEVICE9 pDevice = m_pDraw->GetDevice();
            if (pDevice)
            {
                D3DXCreateSprite(pDevice, &m_pFontSprite);
            }
        }
    }
}

void RenderRadio::Render(const std::string& stationName, const std::string& trackName,
                         float x, float y, float width, float height,
                         DWORD color, float fontSize)
{
    if (!m_pDraw || !m_pFont || stationName.empty())
        return;

    RECT rect = { (LONG)x, (LONG)y, (LONG)(x + width), (LONG)(y + height) };
    UINT format = DT_LEFT | DT_TOP | DT_NOCLIP;

    // Рисуем название станции
    DrawTextWithShadow(stationName, rect, format, color);

    // Если есть название трека, рисуем его ниже
    if (!trackName.empty())
    {
        RECT trackRect = rect;
        trackRect.top += (LONG)(fontSize + 2);
        DrawTextWithShadow(trackName, trackRect, format, tocolor(200, 200, 200, 255));
    }
}

void RenderRadio::RenderWithBackground(const std::string& stationName, const std::string& trackName,
                                       float x, float y, float width, float height,
                                       DWORD textColor, DWORD backgroundColor,
                                       float fontSize)
{
    if (!m_pDraw || stationName.empty())
        return;

    // Рисуем фон (полупрозрачный прямоугольник)
    m_pDraw->dxDrawRectangle(x, y, width, height, backgroundColor);

    // Рисуем текст с отступом
    float padding = 8.0f;
    Render(stationName, trackName,
           x + padding, y + padding,
           width - padding * 2.0f, height - padding * 2.0f,
           textColor, fontSize);
}

/**
 * Отрисовка текста с контуром (outline)
 * Контур рисуется смещением текста на 1 пиксель в 8 направлениях
 */
void RenderRadio::RenderWithOutline(const std::string& stationName, const std::string& trackName,
                                    float x, float y, float width, float height,
                                    DWORD textColor, DWORD outlineColor,
                                    float fontSize)
{
    if (!m_pDraw || !m_pFont || stationName.empty())
        return;

    // Inside RadarRenderer BeginRadarZone — no own StateBlock (reuse parent).
    RECT rect = { (LONG)x, (LONG)y, (LONG)(x + width), (LONG)(y + height) };
    UINT format = DT_CENTER | DT_TOP | DT_NOCLIP;

    if (m_bShadowEnabled && m_pFontSprite)
    {
        const int offsets[8][2] = {
            {-1, -1}, {0, -1}, {1, -1},
            {-1,  0},          {1,  0},
            {-1,  1}, {0,  1}, {1,  1}
        };

        if (SUCCEEDED(m_pFontSprite->Begin(D3DXSPRITE_ALPHABLEND)))
        {
            for (int i = 0; i < 8; i++)
            {
                RECT outlineRect = rect;
                outlineRect.left += offsets[i][0];
                outlineRect.top += offsets[i][1];
                outlineRect.right += offsets[i][0];
                outlineRect.bottom += offsets[i][1];
                DrawUtf8(m_pFontSprite, stationName.c_str(), outlineRect, format, outlineColor);
            }

            DrawUtf8(m_pFontSprite, stationName.c_str(), rect, format, textColor);
            m_pFontSprite->End();
        }
    }
    else
    {
        DrawUtf8(nullptr, stationName.c_str(), rect, format, textColor);
    }

    if (!trackName.empty())
    {
        RECT trackRect = rect;
        trackRect.top += (LONG)(fontSize + 2);

        if (m_bShadowEnabled && m_pFontSprite)
        {
            const int offsets[8][2] = {
                {-1, -1}, {0, -1}, {1, -1},
                {-1,  0},          {1,  0},
                {-1,  1}, {0,  1}, {1,  1}
            };

            if (SUCCEEDED(m_pFontSprite->Begin(D3DXSPRITE_ALPHABLEND)))
            {
                for (int i = 0; i < 8; i++)
                {
                    RECT outlineRect = trackRect;
                    outlineRect.left += offsets[i][0];
                    outlineRect.top += offsets[i][1];
                    outlineRect.right += offsets[i][0];
                    outlineRect.bottom += offsets[i][1];
                    DrawUtf8(m_pFontSprite, trackName.c_str(), outlineRect, format, outlineColor);
                }
                DrawUtf8(m_pFontSprite, trackName.c_str(), trackRect, format, tocolor(200, 200, 200, 255));
                m_pFontSprite->End();
            }
        }
        else
        {
            DrawUtf8(nullptr, trackName.c_str(), trackRect, format, tocolor(200, 200, 200, 255));
        }
    }
}

void RenderRadio::DrawUtf8(LPD3DXSPRITE sprite, const char* utf8, RECT& rect, UINT format, DWORD color)
{
    if (!m_pFont || !utf8 || !utf8[0])
        return;

    wchar_t wide[256]{};
    if (MultiByteToWideChar(CP_UTF8, 0, utf8, -1, wide, 256) <= 0)
        return;
    m_pFont->DrawTextW(sprite, wide, -1, &rect, format, color);
}

void RenderRadio::DrawTextWithShadow(const std::string& text, RECT& rect, UINT format, DWORD color)
{
    if (!m_pFont || text.empty())
        return;

    if (m_bShadowEnabled && m_pFontSprite)
    {
        RECT shadowRect = rect;
        shadowRect.left += 1;
        shadowRect.top += 1;
        shadowRect.right += 1;
        shadowRect.bottom += 1;

        if (SUCCEEDED(m_pFontSprite->Begin(D3DXSPRITE_ALPHABLEND)))
        {
            DrawUtf8(m_pFontSprite, text.c_str(), shadowRect, format, m_shadowColor);
            m_pFontSprite->End();
        }
    }

    if (m_pFontSprite)
    {
        if (SUCCEEDED(m_pFontSprite->Begin(D3DXSPRITE_ALPHABLEND)))
        {
            DrawUtf8(m_pFontSprite, text.c_str(), rect, format, color);
            m_pFontSprite->End();
        }
    }
    else
    {
        DrawUtf8(nullptr, text.c_str(), rect, format, color);
    }
}
