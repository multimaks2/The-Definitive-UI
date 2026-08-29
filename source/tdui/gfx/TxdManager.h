/*****************************************************************************
 *
 *  PROJECT:     The-Definitive-UI
 *  FILE:        source/TxdManager/TxdManager.h
 *  PURPOSE:     Load textures from .txd (same approach as Radar Trilogy reference)
 *
 *****************************************************************************/

#pragma once

#include <d3d9.h>
#include <string>
#include <unordered_map>

struct RwTexDictionary;
struct RwTexture;

class TxdManager
{
public:
    TxdManager();
    ~TxdManager();

    bool Initialize(LPDIRECT3DDEVICE9 pDevice);
    void Shutdown();

    bool IsInitialized() const { return m_bInitialized; }

    // Load / unload dictionary (path relative to plugin or absolute)
    bool LoadTxd(const char* szPath);
    void UnloadTxd();
    bool IsTxdLoaded() const { return m_pTxd != nullptr; }

    // Find by name inside loaded TXD and convert to D3D9 (cached)
    LPDIRECT3DTEXTURE9 GetTexture(const char* szName);
    LPDIRECT3DTEXTURE9 LoadTexture(const char* szName); // alias of GetTexture

    void ReleaseCachedTextures();

    // Convert single RwTexture -> IDirect3DTexture9 (caller owns ref)
    static LPDIRECT3DTEXTURE9 RwTextureToD3D9(LPDIRECT3DDEVICE9 pDevice, RwTexture* pRwTex);
    // Copies stock radar00.. tiles from the game TXD pool into outTiles. Caller owns refs.
    static int LoadStockRadarTiles(LPDIRECT3DDEVICE9 pDevice, LPDIRECT3DTEXTURE9* outTiles, int count);

private:
    LPDIRECT3DDEVICE9  m_pDevice;
    RwTexDictionary*   m_pTxd;
    bool               m_bInitialized;

    std::unordered_map<std::string, LPDIRECT3DTEXTURE9> m_cache;
};
