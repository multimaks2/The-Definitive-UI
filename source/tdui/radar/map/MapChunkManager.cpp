/*****************************************************************************
 *
 *  PROJECT:     The-Definitive-UI
 *  FILE:        source/Radar/mapmanager/MapChunkManager.cpp
 *  PURPOSE:     144 radar map tiles - load, cache and cull by camera radius
 *
 *****************************************************************************/

#include "MapChunkManager.h"
#include "plugin.h"
#include "Radar.h"
#include "Config.h"
#include "TxdManager.h"
#include "CFileLoader.h"
#include "RenderWare.h"
#include <d3dx9.h>
#include <cstring>
#include <cmath>

const float MapChunkManager::MAP_WIDTH     = 6000.0f;
const float MapChunkManager::MAP_HEIGHT    = 6000.0f;
const float MapChunkManager::MAP_CENTER_X  = 3000.0f;
const float MapChunkManager::MAP_CENTER_Y  = -3000.0f;

static LPDIRECT3DTEXTURE9 RwTextureToD3D9(LPDIRECT3DDEVICE9 pDevice, RwTexture* rwTex)
{
    if (!pDevice || !rwTex)
        return nullptr;

    RwRaster* raster = RwTextureGetRaster(rwTex);
    if (!raster)
        return nullptr;

    int w = RwRasterGetWidth(raster);
    int h = RwRasterGetHeight(raster);
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
    HRESULT hr = pDevice->CreateTexture((UINT)w, (UINT)h, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &d3dTex, nullptr);
    if (FAILED(hr) || !d3dTex)
    {
        RwImageFreePixels(img);
        RwImageDestroy(img);
        return nullptr;
    }

    D3DLOCKED_RECT locked;
    if (SUCCEEDED(d3dTex->LockRect(0, &locked, nullptr, 0)))
    {
        RwUInt8* src = RwImageGetPixels(img);
        int srcStride = RwImageGetStride(img);
        int dstStride = locked.Pitch;
        for (int y = 0; y < h; y++)
        {
            RwUInt8* rowSrc = src + y * srcStride;
            RwUInt8* rowDst = (RwUInt8*)locked.pBits + y * dstStride;
            for (int x = 0; x < w; x++)
            {
                int off = x * 4;
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

MapChunkManager::MapChunkManager(LPDIRECT3DDEVICE9 pDevice)
    : m_pDevice(pDevice)
    , m_pMapTxd(nullptr)
    , m_initialized(false)
    , m_stockReady(false)
    , m_previewTileFallback(false)
{
    ZeroMemory(m_chunks, sizeof(m_chunks));
    ZeroMemory(m_stockChunks, sizeof(m_stockChunks));
    ZeroMemory(m_loaded, sizeof(m_loaded));
}

MapChunkManager::~MapChunkManager()
{
    Cleanup();
}

bool MapChunkManager::Initialize()
{
    if (m_initialized)
        return true;

    const char* path = PLUGIN_PATH(Radar::Path::MapTxd);
    m_pMapTxd = CFileLoader::LoadTexDictionary(path);
    if (!m_pMapTxd)
        return false;

    m_initialized = true;
    LoadAllChunks();
    return true;
}

void MapChunkManager::EnsureStockChunks()
{
    if (m_stockReady || !m_pDevice)
        return;

    int pluginTiles = 0;
    for (int i = 0; i < MAP_CHUNKS_COUNT; ++i)
    {
        if (m_chunks[i])
            ++pluginTiles;
    }
    if (pluginTiles > 0 && m_previewTileFallback)
        return;

    const int loaded = TxdManager::LoadStockRadarTiles(m_pDevice, m_stockChunks, MAP_CHUNKS_COUNT);
    if (loaded > 0)
        m_stockReady = true;
}

void MapChunkManager::LoadAllChunks()
{
    if (!m_initialized && !Initialize())
        return;

    for (int index = 0; index < MAP_CHUNKS_COUNT; ++index)
    {
        if (m_chunks[index])
            continue;

        char texName[32];
        sprintf_s(texName, "radar%02d", index);

        RwTexture* rwTex = RwTexDictionaryFindNamedTexture(m_pMapTxd, texName);
        if (!rwTex)
            continue;

        m_chunks[index] = RwTextureToD3D9(m_pDevice, rwTex);
        m_loaded[index] = (m_chunks[index] != nullptr);
    }
}

float MapChunkManager::ComputeVisibleRadius(float cameraZ, float fov, float offsetY, float pitch, bool isInAircraft)
{
    // Soft pre-filter around player focus; FOV/NDC cull does the real work.
    // Camera is |offsetY| behind the icon and pitched down — the look ray hits
    // ground far ahead of the player; radius must cover that trapezoid.
    const float absOffset = fabsf(offsetY);
    float visibleRadius = cameraZ * tanf(fov * 0.5f) * 2.0f;

    const float absPitch = fabsf(pitch);
    if (absPitch > 0.05f && absPitch < 1.45f)
    {
        // Distance along view from camera to z=0; conservative radius from focus.
        const float lookGroundDist = cameraZ / tanf(absPitch);
        if (lookGroundDist > visibleRadius)
            visibleRadius = lookGroundDist;
    }

    visibleRadius += absOffset * 1.75f;
    if (isInAircraft)
        visibleRadius *= 1.35f;
    visibleRadius *= 1.15f;
    visibleRadius *= RadarConfig::GetCullRadiusMul();
    visibleRadius += RadarConfig::GetCullRadiusAdd();
    return visibleRadius;
}

void MapChunkManager::Cleanup()
{
    for (int i = 0; i < MAP_CHUNKS_COUNT; ++i)
    {
        if (m_chunks[i])
        {
            m_chunks[i]->Release();
            m_chunks[i] = nullptr;
        }
        m_loaded[i] = false;
    }

    for (int i = 0; i < MAP_CHUNKS_COUNT; ++i)
    {
        if (m_stockChunks[i])
        {
            m_stockChunks[i]->Release();
            m_stockChunks[i] = nullptr;
        }
    }
    m_stockReady = false;

    if (m_pMapTxd)
    {
        RwTexDictionaryDestroy(m_pMapTxd);
        m_pMapTxd = nullptr;
    }
    m_initialized = false;
}

LPDIRECT3DTEXTURE9 MapChunkManager::GetChunk(int index) const
{
    if (index < 0 || index >= MAP_CHUNKS_COUNT)
        return nullptr;

    const bool wantCustom = RadarConfig::GetCustomRadarTxd();
    LPDIRECT3DTEXTURE9 tex = wantCustom ? m_chunks[index] : m_stockChunks[index];
    if (m_previewTileFallback && !tex)
        tex = wantCustom ? m_stockChunks[index] : m_chunks[index];
    return tex;
}

bool MapChunkManager::IsChunkLoaded(int index) const
{
    return GetChunk(index) != nullptr;
}

int MapChunkManager::GetLoadedChunksCount() const
{
    int count = 0;
    for (int i = 0; i < MAP_CHUNKS_COUNT; ++i)
        if (m_loaded[i])
            ++count;
    return count;
}

int MapChunkManager::GetReadyTileCount() const
{
    int count = 0;
    for (int i = 0; i < MAP_CHUNKS_COUNT; ++i)
    {
        if (GetChunk(i))
            ++count;
    }
    return count;
}
