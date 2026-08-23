/*****************************************************************************
 *
 *  PROJECT:     The-Definitive-UI
 *  FILE:        source/Radar/mapmanager/gangzones/GangZoneRenderer.h
 *  PURPOSE:     Gang zone overlay - cached rects with stock pulsing alpha
 *
 *****************************************************************************/

#pragma once

#include <d3d9.h>
#include <d3dx9.h>
#include <vector>
#include <functional>
#include "GangZoneTypes.h"

class GangZoneRenderer
{
public:
    static const unsigned int UPDATE_INTERVAL = 1000;
    static const float        MAX_RENDER_DISTANCE;
    static const float        MAX_ZONE_SIZE;
    static const float        ZONE_OVERLAP;

    using dxDrawImage3DCallback = std::function<void(const D3DXVECTOR3&, const D3DXVECTOR3&, const D3DXVECTOR2&,
                                                     const D3DXVECTOR3&, const D3DXVECTOR3&, float, float, float,
                                                     LPDIRECT3DTEXTURE9, DWORD)>;

    GangZoneRenderer(LPDIRECT3DDEVICE9 pDevice);

    void UpdateCache();
    void Render(const D3DXVECTOR3& cameraPos, const D3DXVECTOR3& cameraRot, float fov, float nearPlane, float farPlane,
               float cameraPosX, float cameraPosY,
               LPDIRECT3DTEXTURE9 fillTexture, dxDrawImage3DCallback drawCallback);

    void SetEnabled(bool enabled) { m_enabled = enabled; }
    bool IsEnabled() const { return m_enabled; }

private:
    LPDIRECT3DDEVICE9     m_pDevice;
    std::vector<GangZone> m_cachedZones;
    unsigned int          m_lastUpdateTime;
    bool                  m_enabled;
};
