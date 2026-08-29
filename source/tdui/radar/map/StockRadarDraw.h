/*****************************************************************************
 *
 *  PROJECT:     The-Definitive-UI
 *  FILE:        source/Radar/mapmanager/StockRadarDraw.h
 *  PURPOSE:     Project stock CRadar blips/gang overlay onto a custom plane
 *
 *****************************************************************************/

#pragma once

#include <d3dx9.h>

struct StockRadarPlane
{
    float cx     = 0.0f;
    float cy     = 0.0f;
    float half   = 0.0f;
    float clipL  = 0.0f;
    float clipT  = 0.0f;
    float clipR  = 0.0f;
    float clipB  = 0.0f;

    bool        use3D = false;
    D3DXVECTOR3 cameraPos{};
    D3DXVECTOR3 cameraRot{};
    float       fov              = 0.0f;
    float       nearPlane        = 0.3f;
    float       farPlane         = 10000.0f;
    float       rtWidth          = 0.0f;
    float       rtHeight         = 0.0f;
    float       sizeX            = 0.0f;
    float       sizeY            = 0.0f;
    float       projectionAspect = 0.0f;
    float       halfX            = 0.0f;
    float       halfY            = 0.0f;
    float       playerRadarX     = 0.0f;
    float       playerRadarY     = 0.0f;
    float       yaw              = 0.0f;
    bool        shapeCircle      = true;
};

namespace StockRadarDraw
{
    void EnsureHooksInstalled();
    void SetPlane(const StockRadarPlane& plane);
    void Begin();
    void End();
    bool IsActive();
    void SyncRwSpritePipeline();
    // After foreign Im2D / GPS Redux / CopNThreat: clear scissor, shaders,
    // color-write and RW texture stages so the rest of the HUD cannot vanish.
    void SanitizeDrawState();
    void InvalidateRwShaderCache();
    void RestoreHudPipeline();

    void Draw(bool gangOverlay, bool gangInMenu);

    // Runs third-party drawRadarOverlayEvent (GPS Redux) with the current
    // plane. Call while the radar RT is bound so Im2D lands in the disc texture.
    // CopNThreat uses drawBlipsEvent and is dispatched from Draw() instead.
    void DrawHudOverlaysOnly();
}
