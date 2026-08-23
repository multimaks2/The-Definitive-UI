/*****************************************************************************
 *
 *  PROJECT:     The-Definitive-UI
 *  FILE:        source/Radar/Radar.cpp
 *  PURPOSE:     HUD radar facade - wraps RadarRenderer, host owns lifecycle
 *
 *****************************************************************************/

#include "Radar.h"
#include "RadarRenderer.h"
#include "Config.h"

Radar::Radar()
    : m_pDevice(nullptr)
    , m_pRenderer(nullptr)
    , m_bInitialized(false)
{
}

Radar::~Radar()
{
    Shutdown();
}

bool Radar::Initialize(LPDIRECT3DDEVICE9 pDevice)
{
    if (m_bInitialized)
        return true;

    if (!pDevice)
        return false;

    m_pRenderer = new RadarRenderer();
    if (!m_pRenderer->Initialize(pDevice))
    {
        delete m_pRenderer;
        m_pRenderer = nullptr;
        return false;
    }

    const bool shapeCircle = RadarConfig::GetShapeCircle();
    m_pRenderer->SetRadarShapeCircle(shapeCircle);
    m_pRenderer->SetBorderShapeCircle(shapeCircle);
    m_pRenderer->SetShowGangZones(RadarConfig::GetShowGangZones());

    m_pDevice = pDevice;
    m_bInitialized = true;
    return true;
}

void Radar::Shutdown()
{
    if (m_pRenderer)
    {
        m_pRenderer->Shutdown();
        delete m_pRenderer;
        m_pRenderer = nullptr;
    }
    m_pDevice = nullptr;
    m_bInitialized = false;
}

void Radar::Render()
{
    if (!m_bInitialized || !m_pRenderer)
        return;

    auto* device = m_pDevice;
    if (!device)
        return;

    const HRESULT hr = device->TestCooperativeLevel();
    if (FAILED(hr) && hr != D3DERR_DEVICENOTRESET)
        return;

    m_pRenderer->Render();
}

void Radar::MarkRadioNameVisible()
{
    if (m_pRenderer)
        m_pRenderer->MarkRadioNameVisible();
}

MapChunkManager* Radar::GetMapChunkManager() const
{
    return m_pRenderer ? m_pRenderer->GetMapChunkManager() : nullptr;
}
