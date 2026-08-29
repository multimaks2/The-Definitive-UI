/*****************************************************************************
 *
 *  PROJECT:     The-Definitive-UI
 *  FILE:        source/Help/HelpRenderer.h
 *  PURPOSE:     CHud::DrawHelpText — Draw Arial + outline (same path as radio)
 *
 *****************************************************************************/

#pragma once

class Draw;

class HelpRenderer
{
public:
    HelpRenderer();
    ~HelpRenderer() = default;

    void SetDraw(Draw* draw);
    void Render();

private:
    static int  TimeStepMs();
    static void ResetMessage();
    static unsigned char MessageAlpha();
    static void UpdateState();
    static void GxtToUtf8(const char* gxt, char* utf8, size_t cap);
    void DrawHelp(const char* utf8, float x, float y, float wrapW,
                  bool hasStat, unsigned char alpha) const;

    Draw* m_pDraw;
};
