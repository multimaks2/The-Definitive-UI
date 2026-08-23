/*****************************************************************************
 *
 *  PROJECT:     The-Definitive-UI
 *  FILE:        source/Radar/render/RenderRadio.h
 *  PURPOSE:     Radio station name rendering with the custom font
 *
 *****************************************************************************/

#pragma once

#include <d3d9.h>
#include <d3dx9.h>
#include <string>

class DxDrawPrimitives;

/**
 * Рендеринг текста радио (название станции, трек)
 */
class RenderRadio
{
public:
    RenderRadio(DxDrawPrimitives* pDraw);
    ~RenderRadio();

    /**
     * Отрисовка текста радио
     * @param stationName Название радиостанции
     * @param trackName Название трека (опционально)
     * @param x Позиция X (в пикселях от левого края)
     * @param y Позиция Y (в пикселях от верхнего края)
     * @param width Ширина области текста
     * @param height Высота области текста
     * @param color Цвет текста (D3DCOLOR_ARGB)
     * @param fontSize Размер шрифта
     */
    void Render(const std::string& stationName, const std::string& trackName = "",
                float x = 0.0f, float y = 0.0f, float width = 200.0f, float height = 50.0f,
                DWORD color = 0xFFFFFFFF, float fontSize = 14.0f);

    /**
     * Отрисовка текста радио с фоном (полупрозрачная подложка)
     * @param stationName Название радиостанции
     * @param trackName Название трека (опционально)
     * @param x Позиция X
     * @param y Позиция Y
     * @param width Ширина области
     * @param height Высота области
     * @param textColor Цвет текста
     * @param backgroundColor Цвет фона (D3DCOLOR_ARGB)
     * @param fontSize Размер шрифта
     */
    void RenderWithBackground(const std::string& stationName, const std::string& trackName = "",
                              float x = 0.0f, float y = 0.0f, float width = 200.0f, float height = 50.0f,
                              DWORD textColor = 0xFFFFFFFF, DWORD backgroundColor = 0x80000000,
                              float fontSize = 14.0f);

    /**
     * Отрисовка текста радио с контуром (outline)
     * @param stationName Название радиостанции
     * @param trackName Название трека (опционально)
     * @param x Позиция X
     * @param y Позиция Y
     * @param width Ширина области
     * @param height Высота области
     * @param textColor Цвет текста
     * @param outlineColor Цвет контура (D3DCOLOR_ARGB)
     * @param fontSize Размер шрифта
     */
    void RenderWithOutline(const std::string& stationName, const std::string& trackName = "",
                           float x = 0.0f, float y = 0.0f, float width = 200.0f, float height = 50.0f,
                           DWORD textColor = 0xFFFFFFFF, DWORD outlineColor = 0xFF000000,
                           float fontSize = 14.0f);

    /**
     * Установить шрифт для отрисовки
     * @param pFont D3DX шрифт
     */
    void SetFont(LPD3DXFONT pFont);

    /**
     * Получить текущий шрифт
     */
    LPD3DXFONT GetFont() const { return m_pFont; }

    /**
     * Включить/выключить тень текста
     */
    void SetShadowEnabled(bool enabled) { m_bShadowEnabled = enabled; }
    bool IsShadowEnabled() const { return m_bShadowEnabled; }

    /**
     * Установить цвет тени
     */
    void SetShadowColor(DWORD color) { m_shadowColor = color; }
    DWORD GetShadowColor() const { return m_shadowColor; }

private:
    void DrawTextWithShadow(const std::string& text, RECT& rect, UINT format, DWORD color);
    void DrawUtf8(LPD3DXSPRITE sprite, const char* utf8, RECT& rect, UINT format, DWORD color);

    DxDrawPrimitives* m_pDraw;
    LPD3DXFONT        m_pFont;
    LPD3DXSPRITE      m_pFontSprite;
    bool              m_bShadowEnabled;
    DWORD             m_shadowColor;
};
