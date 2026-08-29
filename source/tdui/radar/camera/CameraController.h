/*****************************************************************************
 *
 *  PROJECT:     The-Definitive-UI
 *  FILE:        source/Radar/render/camera/CameraController.h
 *  PURPOSE:     Radar camera - follows the player, smoothing and zoom
 *
 *****************************************************************************/

#pragma once

#include <d3d9.h>
#include <d3dx9.h>
#include "CVector.h"
#include "Config.h"

class CPed;

class CameraController
{
public:
    struct CameraState
    {
        float height;
        float pitch;
        float yaw;
        float posX;
        float posY;
        float offsetX;
        float offsetY;
        float fov;
    };

    struct CameraTarget
    {
        float height;
        float offsetX;
        float offsetY;
        float fov;
        float pitch;
    };

    CameraController();

    void               UpdateFromGame(CPed* player);
    void               UpdateFromSpeed(CPed* player);
    void               GetCachedCalculations(float& offsetWorldX, float& offsetWorldY, D3DXVECTOR3& cameraPos, D3DXVECTOR3& cameraRot, float cameraHeight = 0.0f);

    const CameraState& GetState() const { return m_state; }
    void               SetState(const CameraState& state) { m_state = state; }
    void               ApplySettingsPreview(RadarConfig::RadarCamContext ctx);
    void               InvalidateCache() { m_cache.isValid = false; }

    bool  IsInAircraft(CPed* player) const;
    bool  IsInPlane(CPed* player) const;  // plane only (not helicopter) - for avionics display
    float GetVehicleRollAngle(CPed* player) const;
    float GetVehiclePitchAngle(CPed* player) const;

private:
    void UpdateInterpolation();

    static const unsigned int G_MODE_DURATION_MS = 3000;

    CameraState  m_state;
    CameraTarget  m_target;
    bool          m_gModeActive;
    unsigned int  m_gModeStartTime;
    CameraTarget  m_returnTarget;
    bool          m_gKeyPressedLastFrame;

    struct CameraCache
    {
        float       cachedInvertedYaw;
        float       cachedSinYaw;
        float       cachedCosYaw;
        float       cachedTanHalfFov;
        D3DXVECTOR3 cachedCameraPos;
        D3DXVECTOR3 cachedCameraRot;
        bool        isValid;
    };
    CameraCache m_cache;

    static const float DEFAULT_CAMERA_HEIGHT;
    static const float DEFAULT_FOV_DEGREES;
    static const float DEFAULT_CAMERA_OFFSET_Y;
    static const float DEFAULT_CAMERA_PITCH;
    static const float MAX_CAMERA_HEIGHT;
    static const float MAX_FOV_DEGREES;
    static const float MAX_CAMERA_OFFSET_Y;
    static const float MAX_CAMERA_PITCH;
    static const float CAMERA_HEIGHT_PLANE;
    static const float CAMERA_HEIGHT_HELICOPTER;
    static const float CAMERA_OFFSET_Y_PLANE;
    static const float CAMERA_OFFSET_Y_HELICOPTER;

    // Precomputed: 75% height, 25% offset toward helicopter (vehicle at max speed)
    static const float SPEED_MAX_HEIGHT;
    static const float SPEED_MAX_OFFSET_Y;
    static const float SPEED_MAX_FOV_RAD;
    static const float DEFAULT_FOV_RAD;
    static const float FOV_DELTA_RAD;  // MAX - DEFAULT в радианах
};
