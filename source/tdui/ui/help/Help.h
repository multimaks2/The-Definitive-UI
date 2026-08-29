/*****************************************************************************
 *
 *  PROJECT:     The-Definitive-UI
 *  FILE:        source/Help/Help.h
 *  PURPOSE:     Stock top-left help hints — Draw Arial + outline like radio
 *
 *****************************************************************************/

#pragma once

#include <d3d9.h>

class Draw;
class HelpRenderer;

class Help
{
public:
    Help();
    ~Help();

    bool Initialize(LPDIRECT3DDEVICE9 device, Draw* draw);
    void Shutdown();
    void Render();

    bool IsInitialized() const { return m_bInitialized; }

private:
    LPDIRECT3DDEVICE9 m_pDevice;
    Draw*             m_pDraw;
    HelpRenderer*     m_pRenderer;
    bool              m_bInitialized;
};
