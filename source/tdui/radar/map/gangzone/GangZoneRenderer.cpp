/*****************************************************************************
 *
 *  PROJECT:     The-Definitive-UI
 *  FILE:        source/Radar/mapmanager/gangzones/GangZoneRenderer.cpp
 *  PURPOSE:     Gang zone overlay - cached rects with stock pulsing alpha
 *
 *****************************************************************************/

#include "GangZoneRenderer.h"
#include "CTheZones.h"
#include "CZone.h"
#include "CZoneInfo.h"
#include "CGangWars.h"
#include "CVector.h"
#include "CTimer.h"
#include "CRGBA.h"
#include "ColorUtils.h"
#include "Config.h"
#include <cmath>

const float GangZoneRenderer::MAX_RENDER_DISTANCE = 4000.0f;
const float GangZoneRenderer::MAX_ZONE_SIZE = 600.0f;
const float GangZoneRenderer::ZONE_OVERLAP = 0.001f;

namespace
{
    unsigned char RadarModeFromFlags(char flags)
    {
        return static_cast<unsigned char>((static_cast<unsigned char>(flags) >> 5) & 3);
    }

    DWORD BlinkZoneColor(DWORD argb)
    {
        const unsigned char a = static_cast<unsigned char>((argb >> 24) & 0xFF);
        const unsigned int t = CTimer::m_snTimeInMilliseconds % 1024u;
        // Stock overlay: one sine period per 1024 ms (ESC map).
        // gta-reversed had 1024/2π inverted → ~26 Hz flicker on the HUD radar.
        const float s = (sinf(static_cast<float>(t) * (6.2831853f / 1024.0f)) + 1.0f) * 0.5f;
        const unsigned char na = static_cast<unsigned char>(s * static_cast<float>(a));
        return (argb & 0x00FFFFFF) | (static_cast<DWORD>(na) << 24);
    }
}

GangZoneRenderer::GangZoneRenderer(LPDIRECT3DDEVICE9 pDevice)
    : m_pDevice(pDevice)
    , m_lastUpdateTime(0)
    , m_enabled(false)
{
}

void GangZoneRenderer::UpdateCache()
{
    if (!m_enabled)
        return;

    if (!CGangWars::bGangWarsActive)
    {
        m_cachedZones.clear();
        return;
    }

    m_cachedZones.clear();

    try
    {
        // Do not call FillZonesWithGangColours — it resets RadarMode 2 (war blink)
        // back to 1. Stock already fills colours on war start/end.
        short numZones = CTheZones::TotalNumberOfInfoZones;
        if (numZones <= 0)
            numZones = CTheZones::TotalNumberOfMapZones;
        if (numZones <= 0)
            return;

        for (short i = 0; i < numZones; i++)
        {
            CZone* zone = CTheZones::GetInfoZone(i);
            if (!zone)
                zone = CTheZones::GetMapZone(i);
            if (!zone)
                continue;

            CVector center(
                ((float)zone->m_fX1 + (float)zone->m_fX2) * 0.5f,
                ((float)zone->m_fY1 + (float)zone->m_fY2) * 0.5f,
                0.0f
            );
            CZone* zoneAtCenter = nullptr;
            CZoneInfo* zoneInfo = CTheZones::GetZoneInfo(&center, &zoneAtCenter);
            if (!zoneInfo)
                continue;

            const unsigned char radarMode = RadarModeFromFlags(zoneInfo->m_nFlags);
            const bool fightingHere = (zoneInfo == CGangWars::pZoneInfoToFightOver)
                && CGangWars::GangWarGoingOn();
            if ((!radarMode && !fightingHere) || !CGangWars::CanPlayerStartAGangWarHere(zoneInfo))
                continue;

            const CRGBA& zc = zoneInfo->m_ZoneColor;
            if ((zc.r | zc.g | zc.b) == 0)
                continue;
            const DWORD c = tocolor(zc.r, zc.g, zc.b, zc.a);

            float w = fabsf((float)(zone->m_fX2 - zone->m_fX1));
            float h = fabsf((float)(zone->m_fY2 - zone->m_fY1));
            if (w < 5.0f || h < 5.0f || w > MAX_ZONE_SIZE || h > MAX_ZONE_SIZE)
                continue;

            GangZone gangZone;
            gangZone.x1 = (float)zone->m_fX1;
            gangZone.y1 = (float)zone->m_fY1;
            gangZone.x2 = (float)zone->m_fX2;
            gangZone.y2 = (float)zone->m_fY2;
            gangZone.color = c;
            gangZone.radarMode = (fightingHere || radarMode == 2) ? 2 : radarMode;
            m_cachedZones.push_back(gangZone);
        }
    }
    catch (...) {}
}

void GangZoneRenderer::Render(const D3DXVECTOR3& cameraPos, const D3DXVECTOR3& cameraRot,
                              float fov, float nearPlane, float farPlane,
                              float cameraPosX, float cameraPosY,
                              LPDIRECT3DTEXTURE9 fillTexture,
                              dxDrawImage3DCallback drawCallback)
{
    if (!m_enabled || !RadarConfig::GetShowGangZones() || !m_pDevice || !fillTexture)
        return;

    unsigned int currentTime = CTimer::m_snTimeInMilliseconds;
    const unsigned int interval = CGangWars::GangWarGoingOn() ? 100u : UPDATE_INTERVAL;
    if (currentTime - m_lastUpdateTime > interval || m_cachedZones.empty())
    {
        UpdateCache();
        m_lastUpdateTime = currentTime;
    }

    if (m_cachedZones.empty())
        return;

    try
    {
        for (size_t i = 0; i < m_cachedZones.size(); i++)
        {
            const GangZone& gangZone = m_cachedZones[i];

            float minX = (gangZone.x1 < gangZone.x2) ? gangZone.x1 : gangZone.x2;
            float maxX = (gangZone.x1 > gangZone.x2) ? gangZone.x1 : gangZone.x2;
            float minY = (gangZone.y1 < gangZone.y2) ? gangZone.y1 : gangZone.y2;
            float maxY = (gangZone.y1 > gangZone.y2) ? gangZone.y1 : gangZone.y2;

            float centerWorldX = (minX + maxX) * 0.5f;
            float centerWorldY = (minY + maxY) * 0.5f;
            float zoneMapX = centerWorldX + 3000.0f;
            float zoneMapY = centerWorldY - 3000.0f;
            float dx = zoneMapX - cameraPosX;
            float dy = zoneMapY - cameraPosY;
            float distance = sqrtf(dx * dx + dy * dy);
            if (distance > MAX_RENDER_DISTANCE)
                continue;

            float width  = maxX - minX;
            float height = maxY - minY;
            if (width < 1.0f || height < 1.0f)
                continue;

            width  += ZONE_OVERLAP;
            height += ZONE_OVERLAP;

            D3DXVECTOR3 zonePos(centerWorldX + 3000.0f, centerWorldY - 3000.0f, 1.0f);
            D3DXVECTOR3 zoneRot(0.0f, 0.0f, 0.0f);
            D3DXVECTOR2 zoneSize(width, height);

            DWORD color = gangZone.color;
            if (gangZone.radarMode == 2)
                color = BlinkZoneColor(color);

            if (drawCallback)
            {
                drawCallback(zonePos, zoneRot, zoneSize, cameraPos, cameraRot,
                             fov, nearPlane, farPlane, fillTexture, color);
            }
        }
    }
    catch (...) {}
}
