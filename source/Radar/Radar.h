/*****************************************************************************
 *
 *  PROJECT:     The-Definitive-UI
 *  FILE:        source/Radar/Radar.h
 *  PURPOSE:     HUD radar facade — wraps RadarRenderer, host owns lifecycle
 *
 *****************************************************************************/

#pragma once

#include <d3d9.h>

class RadarRenderer;
class MapChunkManager;

class Radar
{
public:
    struct Path
    {
        static constexpr const char* MapTxd      = "The-Definitive-UI.SA\\map.txd";
        static constexpr const char* BlipTxd     = "The-Definitive-UI.SA\\blip.txd";
        static constexpr const char* BlipLinePng = "The-Definitive-UI.SA\\blip\\line.png";
        static constexpr const char* BlipRingPng = "The-Definitive-UI.SA\\blip\\RingPlane.png";
    };

    Radar();
    ~Radar();

    bool Initialize(LPDIRECT3DDEVICE9 pDevice);
    void Shutdown();
    void Render();
    void MarkRadioNameVisible();

    bool IsInitialized() const { return m_bInitialized; }
    MapChunkManager* GetMapChunkManager() const;

private:
    LPDIRECT3DDEVICE9 m_pDevice;
    RadarRenderer*    m_pRenderer;
    bool              m_bInitialized;
};
