/*****************************************************************************
 *
 *  PROJECT:     The-Definitive-UI
 *  FILE:        source/Help/Help.cpp
 *  PURPOSE:     Stock top-left help hints - Draw Arial + outline like radio
 *
 *****************************************************************************/

#include "Help.h"
#include "HelpRenderer.h"

Help::Help()
    : m_pDevice(nullptr)
    , m_pDraw(nullptr)
    , m_pRenderer(nullptr)
    , m_bInitialized(false)
{
}

Help::~Help()
{
    Shutdown();
}

bool Help::Initialize(LPDIRECT3DDEVICE9 device, Draw* draw)
{
    if (m_bInitialized)
        return true;
    if (!device || !draw)
        return false;

    m_pRenderer = new HelpRenderer();
    m_pRenderer->SetDraw(draw);
    m_pDevice = device;
    m_pDraw = draw;
    m_bInitialized = true;
    return true;
}

void Help::Shutdown()
{
    delete m_pRenderer;
    m_pRenderer = nullptr;
    m_pDevice = nullptr;
    m_pDraw = nullptr;
    m_bInitialized = false;
}

void Help::Render()
{
    if (m_bInitialized && m_pRenderer)
        m_pRenderer->Render();
}
