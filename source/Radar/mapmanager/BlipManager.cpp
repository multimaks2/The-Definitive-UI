/*****************************************************************************
 *
 *  PROJECT:     The-Definitive-UI
 *  FILE:        source/Radar/mapmanager/BlipManager.cpp
 *  PURPOSE:     Radar blips - stock sprite textures and overlay drawing
 *
 *****************************************************************************/

#include "BlipManager.h"
#include "Radar.h"
#include "Config.h"
#include "CFileLoader.h"
#include "CRadar.h"
#include "CSprite2d.h"
#include "RenderWare.h"
#include <d3dx9.h>
#include <cstring>
#include <cstdio>

namespace
{
    LPDIRECT3DTEXTURE9 RwTextureToD3D9(LPDIRECT3DDEVICE9 pDevice, RwTexture* rwTex)
    {
        if (!pDevice || !rwTex)
            return nullptr;

        RwRaster* raster = RwTextureGetRaster(rwTex);
        if (!raster)
            return nullptr;

        const int w = RwRasterGetWidth(raster);
        const int h = RwRasterGetHeight(raster);
        if (w <= 0 || h <= 0)
            return nullptr;

        RwImage* img = RwImageCreate(w, h, 32);
        if (!img)
            return nullptr;
        if (!RwImageAllocatePixels(img))
        {
            RwImageDestroy(img);
            return nullptr;
        }
        if (!RwImageSetFromRaster(img, raster))
        {
            RwImageFreePixels(img);
            RwImageDestroy(img);
            return nullptr;
        }

        LPDIRECT3DTEXTURE9 d3dTex = nullptr;
        if (FAILED(pDevice->CreateTexture((UINT)w, (UINT)h, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &d3dTex, nullptr))
            || !d3dTex)
        {
            RwImageFreePixels(img);
            RwImageDestroy(img);
            return nullptr;
        }

        D3DLOCKED_RECT locked{};
        if (SUCCEEDED(d3dTex->LockRect(0, &locked, nullptr, 0)))
        {
            RwUInt8* src = RwImageGetPixels(img);
            const int srcStride = RwImageGetStride(img);
            const int dstStride = locked.Pitch;
            for (int y = 0; y < h; ++y)
            {
                RwUInt8* rowSrc = src + y * srcStride;
                RwUInt8* rowDst = static_cast<RwUInt8*>(locked.pBits) + y * dstStride;
                for (int x = 0; x < w; ++x)
                {
                    const int off = x * 4;
                    rowDst[off + 0] = rowSrc[off + 2];
                    rowDst[off + 1] = rowSrc[off + 1];
                    rowDst[off + 2] = rowSrc[off + 0];
                    rowDst[off + 3] = rowSrc[off + 3];
                }
            }
            d3dTex->UnlockRect(0);
        }

        RwImageFreePixels(img);
        RwImageDestroy(img);
        return d3dTex;
    }

    static RwTexDictionary* s_deTxd = nullptr;
    static int              s_deSwapDepth = 0;
    static bool             s_deSwapped = false;
    static RwTexture*       s_savedSprites[BlipManager::MAX_BLIP_ID + 1]{};
    static bool             s_replacedSprites[BlipManager::MAX_BLIP_ID + 1]{};

    static RwTexDictionary* EnsureDeTxd()
    {
        if (!s_deTxd)
            s_deTxd = CFileLoader::LoadTexDictionary(PLUGIN_PATH(Radar::Path::BlipTxd));
        return s_deTxd;
    }
}

BlipManager::BlipManager(LPDIRECT3DDEVICE9 pDevice)
    : m_pDevice(pDevice)
    , m_pAuxTxd(nullptr)
    , m_bInitialized(false)
{
    ZeroMemory(m_stockTextures, sizeof(m_stockTextures));
    ZeroMemory(m_auxTextures, sizeof(m_auxTextures));
}

BlipManager::~BlipManager()
{
    Shutdown();
}

bool BlipManager::Initialize()
{
    if (m_bInitialized)
        return true;

    StockRadarDraw::EnsureHooksInstalled();

    if (!m_pAuxTxd)
        m_pAuxTxd = EnsureDeTxd();

    m_bInitialized = true;
    return true;
}

void BlipManager::Shutdown()
{
    for (int i = 0; i <= MAX_BLIP_ID; ++i)
    {
        if (m_stockTextures[i])
        {
            m_stockTextures[i]->Release();
            m_stockTextures[i] = nullptr;
        }
    }

    for (auto*& tex : m_auxTextures)
    {
        if (tex)
        {
            tex->Release();
            tex = nullptr;
        }
    }

    PopDeIcons();

    if (m_pAuxTxd)
    {
        if (s_deTxd == m_pAuxTxd)
            s_deTxd = nullptr;
        RwTexDictionaryDestroy(m_pAuxTxd);
        m_pAuxTxd = nullptr;
    }

    m_bInitialized = false;
}

void BlipManager::EnsureStockTexturesLoaded()
{
}

LPDIRECT3DTEXTURE9 BlipManager::ConvertRwTexture(RwTexture* rwTex)
{
    return RwTextureToD3D9(m_pDevice, rwTex);
}

LPDIRECT3DTEXTURE9 BlipManager::GetStockSpriteTexture(int spriteId)
{
    (void)spriteId;
    return nullptr;
}

LPDIRECT3DTEXTURE9 BlipManager::GetOwnedSpriteTexture(int spriteId)
{
    if (spriteId == RADAR_SPRITE_LIGHT || spriteId == RADAR_SPRITE_RUNWAY)
        return nullptr;
    if (spriteId < 0 || spriteId > MAX_BLIP_ID || !m_pDevice || !m_pAuxTxd)
        return nullptr;
    if (m_stockTextures[spriteId])
        return m_stockTextures[spriteId];

    char texName[16];
    sprintf_s(texName, "%d", spriteId);
    RwTexture* rwTex = RwTexDictionaryFindNamedTexture(m_pAuxTxd, texName);
    if (!rwTex)
        return nullptr;
    m_stockTextures[spriteId] = ConvertRwTexture(rwTex);
    return m_stockTextures[spriteId];
}

LPDIRECT3DTEXTURE9 BlipManager::LoadAuxTextureFromTxd(const char* texName)
{
    if (!m_pDevice || !m_pAuxTxd || !texName)
        return nullptr;

    RwTexture* rwTex = RwTexDictionaryFindNamedTexture(m_pAuxTxd, texName);
    if (!rwTex)
        return nullptr;

    if (strcmp(texName, "line") == 0 || strcmp(texName, "radarLine") == 0)
    {
        if (!m_auxTextures[0])
            m_auxTextures[0] = ConvertRwTexture(rwTex);
        return m_auxTextures[0];
    }

    if (strcmp(texName, "radarRingPlane") == 0 || strcmp(texName, "RingPlane") == 0)
    {
        if (!m_auxTextures[2])
            m_auxTextures[2] = ConvertRwTexture(rwTex);
        return m_auxTextures[2];
    }

    return ConvertRwTexture(rwTex);
}

void BlipManager::DrawStockOverlay(const StockRadarPlane& plane, bool gangZones)
{
    StockRadarDraw::SetPlane(plane);
    StockRadarDraw::Begin();
    StockRadarDraw::Draw(gangZones, false);
    StockRadarDraw::End();
}

void BlipManager::PushDeIcons()
{
    if (++s_deSwapDepth != 1)
        return;
    s_deSwapped = false;
    if (!RadarConfig::GetDeIcons() || !CRadar::RadarBlipSprites)
        return;
    RwTexDictionary* txd = EnsureDeTxd();
    if (!txd)
        return;

    ZeroMemory(s_replacedSprites, sizeof(s_replacedSprites));
    for (int i = 0; i <= MAX_BLIP_ID; ++i)
    {
        if (i == RADAR_SPRITE_LIGHT || i == RADAR_SPRITE_RUNWAY)
            continue;
        char texName[16];
        sprintf_s(texName, "%d", i);
        RwTexture* deTex = RwTexDictionaryFindNamedTexture(txd, texName);
        if (!deTex)
            continue;
        s_savedSprites[i] = CRadar::RadarBlipSprites[i].m_pTexture;
        s_replacedSprites[i] = true;
        CRadar::RadarBlipSprites[i].m_pTexture = deTex;
        s_deSwapped = true;
    }
}

void BlipManager::PopDeIcons()
{
    if (s_deSwapDepth <= 0)
        return;
    if (--s_deSwapDepth != 0)
        return;
    if (!s_deSwapped || !CRadar::RadarBlipSprites)
    {
        s_deSwapped = false;
        return;
    }
    for (int i = 0; i <= MAX_BLIP_ID; ++i)
    {
        if (!s_replacedSprites[i])
            continue;
        CRadar::RadarBlipSprites[i].m_pTexture = s_savedSprites[i];
        s_replacedSprites[i] = false;
        s_savedSprites[i] = nullptr;
    }
    s_deSwapped = false;
}

bool BlipManager::IsLegendSprite(unsigned char spriteId)
{
    switch (spriteId)
    {
    case RADAR_SPRITE_BIGSMOKE:
    case RADAR_SPRITE_CATALINAPINK:
    case RADAR_SPRITE_CESARVIAPANDO:
    case RADAR_SPRITE_CJ:
    case RADAR_SPRITE_CRASH1:
    case RADAR_SPRITE_MCSTRAP:
    case RADAR_SPRITE_OGLOC:
    case RADAR_SPRITE_RYDER:
    case RADAR_SPRITE_SWEET:
    case RADAR_SPRITE_THETRUTH:
    case RADAR_SPRITE_TORENORANCH:
    case RADAR_SPRITE_WOOZIE:
    case RADAR_SPRITE_ZERO:
        return true;
    default:
        return false;
    }
}

bool BlipManager::IsMissionCheckpointSprite(unsigned char spriteId)
{
    switch (spriteId)
    {
    case RADAR_SPRITE_NONE:
    case RADAR_SPRITE_QMARK:
        return true;
    default:
        return false;
    }
}

eHeightIndicatorType BlipManager::GetHeightIndicatorType(float blipZ, float playerZ, float threshold)
{
    const float diff = blipZ - playerZ;
    if (diff > threshold)
        return HEIGHT_INDICATOR_ABOVE;
    if (diff < -threshold)
        return HEIGHT_INDICATOR_BELOW;
    return HEIGHT_INDICATOR_SAME;
}

DWORD BlipManager::TraceColorToD3D(unsigned int blipColour, bool bright, bool friendly)
{
    CRGBA color = CRadar::GetRadarTraceColour(blipColour, bright ? 1 : 0, friendly ? 1 : 0);
    return D3DCOLOR_ARGB(color.a, color.r, color.g, color.b);
}
