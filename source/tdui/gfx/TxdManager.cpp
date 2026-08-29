/*****************************************************************************
 *
 *  PROJECT:     The-Definitive-UI
 *  FILE:        source/TxdManager/TxdManager.cpp
 *  PURPOSE:     Load textures from .txd (same approach as Radar Trilogy reference)
 *
 *****************************************************************************/

#include "TxdManager.h"

#include "plugin.h"
#include "CFileLoader.h"
#include "CTxdStore.h"
#include "CStreaming.h"
#include "CStreamingInfo.h"
#include "common.h"
#include "RenderWare.h"
#include <d3d9.h>
#include <d3dx9.h>
#include <cstdio>

TxdManager::TxdManager()
    : m_pDevice(nullptr)
    , m_pTxd(nullptr)
    , m_bInitialized(false)
{
}

TxdManager::~TxdManager()
{
    Shutdown();
}

bool TxdManager::Initialize(LPDIRECT3DDEVICE9 pDevice)
{
    if (m_bInitialized)
        return true;

    if (!pDevice)
        return false;

    m_pDevice = pDevice;
    m_bInitialized = true;
    return true;
}

void TxdManager::Shutdown()
{
    UnloadTxd();
    m_pDevice = nullptr;
    m_bInitialized = false;
}

bool TxdManager::LoadTxd(const char* szPath)
{
    if (!m_bInitialized || !szPath || !szPath[0])
        return false;

    UnloadTxd();

    // Same as reference: CFileLoader::LoadTexDictionary(PLUGIN_PATH(...))
    // Accept both absolute paths and plugin-relative via PLUGIN_PATH when needed by caller
    m_pTxd = CFileLoader::LoadTexDictionary(szPath);
    return m_pTxd != nullptr;
}

void TxdManager::UnloadTxd()
{
    ReleaseCachedTextures();

    if (m_pTxd)
    {
        RwTexDictionaryDestroy(m_pTxd);
        m_pTxd = nullptr;
    }
}

void TxdManager::ReleaseCachedTextures()
{
    for (auto& pair : m_cache)
    {
        if (pair.second)
        {
            pair.second->Release();
            pair.second = nullptr;
        }
    }
    m_cache.clear();
}

LPDIRECT3DTEXTURE9 TxdManager::GetTexture(const char* szName)
{
    if (!m_bInitialized || !m_pTxd || !m_pDevice || !szName || !szName[0])
        return nullptr;

    const std::string key(szName);
    auto it = m_cache.find(key);
    if (it != m_cache.end())
        return it->second;

    RwTexture* pRwTex = RwTexDictionaryFindNamedTexture(m_pTxd, szName);
    if (!pRwTex)
        return nullptr;

    LPDIRECT3DTEXTURE9 pD3dTex = RwTextureToD3D9(m_pDevice, pRwTex);
    if (pD3dTex)
        m_cache[key] = pD3dTex;

    return pD3dTex;
}

LPDIRECT3DTEXTURE9 TxdManager::LoadTexture(const char* szName)
{
    return GetTexture(szName);
}

LPDIRECT3DTEXTURE9 TxdManager::RwTextureToD3D9(LPDIRECT3DDEVICE9 pDevice, RwTexture* pRwTex)
{
    if (!pDevice || !pRwTex)
        return nullptr;

    RwRaster* pRaster = RwTextureGetRaster(pRwTex);
    if (!pRaster)
        return nullptr;

    const int w = RwRasterGetWidth(pRaster);
    const int h = RwRasterGetHeight(pRaster);
    if (w <= 0 || h <= 0)
        return nullptr;

    RwImage* pImage = RwImageCreate(w, h, 32);
    if (!pImage)
        return nullptr;

    if (!RwImageAllocatePixels(pImage))
    {
        RwImageDestroy(pImage);
        return nullptr;
    }

    if (!RwImageSetFromRaster(pImage, pRaster))
    {
        RwImageFreePixels(pImage);
        RwImageDestroy(pImage);
        return nullptr;
    }

    LPDIRECT3DTEXTURE9 pD3dTex = nullptr;
    HRESULT hr = pDevice->CreateTexture(static_cast<UINT>(w), static_cast<UINT>(h), 1, 0,
                                        D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &pD3dTex, nullptr);
    if (FAILED(hr) || !pD3dTex)
    {
        RwImageFreePixels(pImage);
        RwImageDestroy(pImage);
        return nullptr;
    }

    D3DLOCKED_RECT locked{};
    if (SUCCEEDED(pD3dTex->LockRect(0, &locked, nullptr, 0)))
    {
        RwUInt8* pSrc = RwImageGetPixels(pImage);
        const int srcStride = RwImageGetStride(pImage);
        const int dstStride = locked.Pitch;

        for (int y = 0; y < h; ++y)
        {
            RwUInt8* pRowSrc = pSrc + y * srcStride;
            RwUInt8* pRowDst = static_cast<RwUInt8*>(locked.pBits) + y * dstStride;

            for (int x = 0; x < w; ++x)
            {
                const int off = x * 4;
                // RW RGBA -> D3D A8R8G8B8 (swap R/B)
                pRowDst[off + 0] = pRowSrc[off + 2];
                pRowDst[off + 1] = pRowSrc[off + 1];
                pRowDst[off + 2] = pRowSrc[off + 0];
                pRowDst[off + 3] = pRowSrc[off + 3];
            }
        }

        pD3dTex->UnlockRect(0);
    }

    RwImageFreePixels(pImage);
    RwImageDestroy(pImage);
    return pD3dTex;
}

namespace
{
    constexpr int kTxdModelBase = 20000;
    constexpr uintptr_t kRwD3D9RasterExtOffset = 0xB4E9E0;
    constexpr uintptr_t kCTxdStoreGetTxd = 0x408340;

    LPDIRECT3DTEXTURE9 CopyRwTextureFromD3D(LPDIRECT3DDEVICE9 device, RwTexture* rwTex)
    {
        if (!device || !rwTex)
            return nullptr;

        RwRaster* raster = RwTextureGetRaster(rwTex);
        if (!raster)
            return nullptr;

        const int extOff = *reinterpret_cast<int*>(kRwD3D9RasterExtOffset);
        if (extOff <= 0)
            return nullptr;

        struct RasterExt
        {
            IDirect3DTexture9* texture;
        };
        auto* ext = reinterpret_cast<RasterExt*>(reinterpret_cast<unsigned char*>(raster) + extOff);
        IDirect3DTexture9* src = ext ? ext->texture : nullptr;
        if (!src)
            return nullptr;

        D3DSURFACE_DESC desc{};
        if (FAILED(src->GetLevelDesc(0, &desc)) || desc.Width == 0 || desc.Height == 0)
            return nullptr;

        LPDIRECT3DTEXTURE9 dst = nullptr;
        if (FAILED(D3DXCreateTexture(device, desc.Width, desc.Height, 1, 0,
                                     D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &dst))
            || !dst)
            return nullptr;

        IDirect3DSurface9* srcSurf = nullptr;
        IDirect3DSurface9* dstSurf = nullptr;
        if (FAILED(src->GetSurfaceLevel(0, &srcSurf)) || FAILED(dst->GetSurfaceLevel(0, &dstSurf)))
        {
            if (srcSurf)
                srcSurf->Release();
            if (dstSurf)
                dstSurf->Release();
            dst->Release();
            return nullptr;
        }

        const HRESULT hr = D3DXLoadSurfaceFromSurface(
            dstSurf, nullptr, nullptr, srcSurf, nullptr, nullptr, D3DX_FILTER_NONE, 0);
        srcSurf->Release();
        dstSurf->Release();
        if (FAILED(hr))
        {
            dst->Release();
            return nullptr;
        }
        return dst;
    }
}

int TxdManager::LoadStockRadarTiles(LPDIRECT3DDEVICE9 pDevice, LPDIRECT3DTEXTURE9* outTiles, int count)
{
    if (!pDevice || !outTiles || count <= 0)
        return 0;

    if (count > 144)
        count = 144;

    int slots[144];
    int requestCount = 0;
    for (int i = 0; i < count; ++i)
    {
        slots[i] = -1;
        if (outTiles[i])
            continue;
        char name[16];
        sprintf_s(name, "radar%02d", i);
        const int slot = CTxdStore::FindTxdSlot(name);
        if (slot < 0)
            continue;
        slots[i] = slot;
        CStreaming::RequestTxdModel(slot, GAME_REQUIRED | KEEP_IN_MEMORY);
        ++requestCount;
    }

    if (requestCount == 0)
        return 0;

    __try
    {
        CStreaming::LoadAllRequestedModels(false);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return 0;
    }

    int loaded = 0;
    for (int i = 0; i < count; ++i)
    {
        if (outTiles[i])
        {
            ++loaded;
            continue;
        }

        const int slot = slots[i];
        if (slot < 0)
            continue;

        const int modelId = kTxdModelBase + slot;
        if (!CStreaming::HasModelLoaded(modelId))
            continue;

        CTxdStore::AddRef(slot);
        RwTexDictionary* dict = plugin::CallAndReturn<RwTexDictionary*, kCTxdStoreGetTxd>(slot);
        RwTexture* rwTex = nullptr;
        if (dict)
        {
            char texName[16];
            sprintf_s(texName, "radar%02d", i);
            rwTex = RwTexDictionaryFindNamedTexture(dict, texName);
            if (!rwTex)
                rwTex = GetFirstTexture(dict);
        }

        if (rwTex)
        {
            outTiles[i] = CopyRwTextureFromD3D(pDevice, rwTex);
            if (!outTiles[i])
                outTiles[i] = RwTextureToD3D9(pDevice, rwTex);
            if (outTiles[i])
                ++loaded;
        }

        CTxdStore::RemoveRef(slot);
        CStreaming::RemoveModel(modelId);
    }
    return loaded;
}
