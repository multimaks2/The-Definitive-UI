/*****************************************************************************
 *
 *  PROJECT:     The-Definitive-UI
 *  FILE:        source/Radar/mapmanager/MapChunkManager.h
 *  PURPOSE:     144 radar map tiles - load, cache and cull by camera radius
 *
 *****************************************************************************/

#pragma once

#include <cmath>
#include <d3d9.h>
#include <d3dx9.h>
#include "RenderWare.h"
#include "MathUtils.h"

class MapChunkManager
{
public:
    static const int   MAP_CHUNKS_COUNT = 144;
    static const int   MAP_CHUNKS_PER_ROW = 12;
    static const float MAP_WIDTH;
    static const float MAP_HEIGHT;
    static const float MAP_CENTER_X;
    static const float MAP_CENTER_Y;

    struct FrustumParams
    {
        const D3DXVECTOR3* cameraPos;
        const D3DXVECTOR3* cameraRot;
        float fov;
        float nearPlane;
        float farPlane;
        float screenWidth;
        float screenHeight;
        float projectionAspect;
        // Extra radius around offset camera; 0 = focus-only distance cull.
        float cameraCullRadius = 0.0f;
    };

    MapChunkManager(LPDIRECT3DDEVICE9 pDevice);
    ~MapChunkManager();

    bool Initialize();
    void LoadAllChunks();
    void EnsureStockChunks();
    void Cleanup();

    // Settings preview: use alternate tile set when preferred source is unavailable (pre-game menu).
    void SetPreviewTileFallback(bool enabled) { m_previewTileFallback = enabled; }

    // Distance cull around focusPos (usually player/icon — NOT the offset camera),
    // then FOV/NDC cull with frustumParams->cameraPos/Rot (actual radar camera).
    // Pass frustumParams=nullptr to skip frustum culling.
    template<typename F>
    void ForEachChunkInRadius(const D3DXVECTOR3& focusPos, float visibleRadius,
                             const FrustumParams* frustumParams, F&& callback) const;

    LPDIRECT3DTEXTURE9 GetChunk(int index) const;
    bool               IsChunkLoaded(int index) const;
    int                GetLoadedChunksCount() const;
    // Custom or stock — how many tiles are usable for pause map / HUD.
    int                GetReadyTileCount() const;

    // focus = player; camera sits |offsetY| behind. pitch (rad) = look-ahead on ground.
    static float ComputeVisibleRadius(float cameraZ, float fov, float offsetY, float pitch, bool isInAircraft);

private:
    LPDIRECT3DDEVICE9   m_pDevice;
    RwTexDictionary*    m_pMapTxd;
    LPDIRECT3DTEXTURE9  m_chunks[MAP_CHUNKS_COUNT];
    LPDIRECT3DTEXTURE9  m_stockChunks[MAP_CHUNKS_COUNT];
    bool                m_loaded[MAP_CHUNKS_COUNT];
    bool                m_initialized;
    bool                m_stockReady;
    bool                m_previewTileFallback;
};

template<typename F>
void MapChunkManager::ForEachChunkInRadius(const D3DXVECTOR3& focusPos, float visibleRadius,
                                          const FrustumParams* frustumParams, F&& callback) const
{
    const float chunkWorldWidth  = MAP_WIDTH / MAP_CHUNKS_PER_ROW;
    const float chunkWorldHeight = MAP_HEIGHT / MAP_CHUNKS_PER_ROW;
    const float halfW = chunkWorldWidth * 0.5f;
    const float halfH = chunkWorldHeight * 0.5f;
    const float mapLeft = MAP_CENTER_X - MAP_WIDTH * 0.5f;
    const float mapTop  = MAP_CENTER_Y + MAP_HEIGHT * 0.5f;

    // Chunk visible if ANY part is in view: expand radius so chunk rect can intersect
    const float chunkHalfDiag = sqrtf(halfW * halfW + halfH * halfH);
    const float effectiveRadius = visibleRadius + chunkHalfDiag;
    const float radiusSq = effectiveRadius * effectiveRadius;

    for (int index = 0; index < MAP_CHUNKS_COUNT; ++index)
    {
        if (!GetChunk(index))
            continue;

        LPDIRECT3DTEXTURE9 chunkTex = GetChunk(index);
        if (!chunkTex)
            continue;

        int row = index / MAP_CHUNKS_PER_ROW;
        int col = index % MAP_CHUNKS_PER_ROW;
        float chunkCenterX = mapLeft + (col + 0.5f) * chunkWorldWidth;
        float chunkCenterY = mapTop - (row + 0.5f) * chunkWorldHeight;

        // Distance around focus (player icon). Also accept tiles near the offset
        // camera — when turning, forward edge chunks can be in-frustum but outside
        // a circle centered on the icon.
        const float distFocusSq = MathUtils::DistanceSq2D(chunkCenterX, chunkCenterY, focusPos.x, focusPos.y);
        if (distFocusSq > radiusSq)
        {
            bool nearCamera = false;
            if (frustumParams && frustumParams->cameraPos && frustumParams->cameraCullRadius > 0.0f)
            {
                const float camR = frustumParams->cameraCullRadius + chunkHalfDiag;
                const float camRSq = camR * camR;
                nearCamera = MathUtils::DistanceSq2D(chunkCenterX, chunkCenterY,
                                                     frustumParams->cameraPos->x,
                                                     frustumParams->cameraPos->y) <= camRSq;
            }
            if (!nearCamera)
                continue;
        }

        // Frustum: NDC AABB of the tile must overlap the camera view cone.
        if (frustumParams && frustumParams->cameraPos && frustumParams->cameraRot)
        {
            if (!MathUtils::IsMapQuadInView(
                    chunkCenterX - halfW, chunkCenterY - halfH,
                    chunkCenterX + halfW, chunkCenterY + halfH,
                    *frustumParams->cameraPos, *frustumParams->cameraRot,
                    frustumParams->fov, frustumParams->nearPlane, frustumParams->farPlane,
                    frustumParams->screenWidth, frustumParams->screenHeight,
                    frustumParams->projectionAspect))
                continue;
        }

        D3DXVECTOR3 elementPos(chunkCenterX, chunkCenterY, 0.0f);
        D3DXVECTOR3 elementRot(0.0f, 0.0f, 0.0f);
        D3DXVECTOR2 elementSize(chunkWorldWidth, chunkWorldHeight);

        callback(index, elementPos, elementRot, elementSize, chunkTex);
    }
}
