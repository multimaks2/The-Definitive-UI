/*****************************************************************************
 *
 *  PROJECT:     The-Definitive-UI
 *  FILE:        source/Radar/render/camera/CameraController.cpp
 *  PURPOSE:     Radar camera - follows the player, smoothing and zoom
 *
 *****************************************************************************/

#include "CameraController.h"
#include "MapChunkManager.h"
#include "CCamera.h"
#include "CCam.h"
#include "eCamMode.h"
#include "CPlayerPed.h"
#include "CVehicle.h"
#include "CModelInfo.h"
#include "CMatrix.h"
#include "CTimer.h"
#include "Config.h"
#include "GameState.h"
#include "common.h"
#include <cmath>
#include <windows.h>

const float CameraController::DEFAULT_CAMERA_HEIGHT      = 445.0f;
const float CameraController::DEFAULT_FOV_DEGREES        = 70.0f;
const float CameraController::DEFAULT_CAMERA_OFFSET_Y    = -105.0f;
const float CameraController::DEFAULT_CAMERA_PITCH       = -26.0f;

const float CameraController::MAX_CAMERA_HEIGHT          = 460.0f;
const float CameraController::MAX_FOV_DEGREES            = 70.0f;
const float CameraController::MAX_CAMERA_OFFSET_Y        = -140.0f;
const float CameraController::MAX_CAMERA_PITCH           = -28.0f;

const float CameraController::CAMERA_HEIGHT_PLANE        = 500.0f;
const float CameraController::CAMERA_OFFSET_Y_PLANE      = -60.0f;

const float CameraController::CAMERA_HEIGHT_HELICOPTER   = 480.0f;
const float CameraController::CAMERA_OFFSET_Y_HELICOPTER = -75.0f;


const float CameraController::SPEED_MAX_HEIGHT   = 471.25f;
const float CameraController::SPEED_MAX_OFFSET_Y = -81.25f;
const float CameraController::SPEED_MAX_FOV_RAD  = 71.0f * D3DX_PI / 180.0f;
const float CameraController::DEFAULT_FOV_RAD     = 70.0f * D3DX_PI / 180.0f;
const float CameraController::FOV_DELTA_RAD      = 1.0f * D3DX_PI / 180.0f;

static RadarConfig::RadarCamContext DetectRadarCamContext(CPed* player, bool gModeActive)
{
    if (gModeActive)
        return RadarConfig::RadarCamContext::ZoomMax;

    try
    {
        if (!player || !player->bInVehicle || !player->m_pVehicle)
            return RadarConfig::RadarCamContext::Ped;

        const int modelIndex = player->m_pVehicle->m_nModelIndex;
        if (CModelInfo::IsPlaneModel(modelIndex))
            return RadarConfig::RadarCamContext::Plane;
        if (CModelInfo::IsHeliModel(modelIndex))
            return RadarConfig::RadarCamContext::Heli;

        const float speedKMH = player->m_pVehicle->m_vecMoveSpeed.Magnitude2D() * 50.0f * 3.6f;
        if (speedKMH >= 100.0f)
            return RadarConfig::RadarCamContext::Vehicle;
    }
    catch (...) {}

    return RadarConfig::RadarCamContext::Ped;
}

static void ApplyRadarCamContextOffsets(CameraController::CameraTarget& tgt, RadarConfig::RadarCamContext ctx)
{
    if (!RadarConfig::GetRadar3D())
    {
        tgt.offsetX = 0.0f;
        return;
    }

    const RadarConfig::RadarCamOffsets o = RadarConfig::GetCamOffsets(ctx);
    tgt.offsetX = o.offsetX;
    tgt.offsetY += o.offsetY;
    tgt.height  += o.offsetZ;
    tgt.pitch   += o.pitchDeg * (D3DX_PI / 180.0f);
    tgt.fov     += o.fovDeg * (D3DX_PI / 180.0f);
}

CameraController::CameraController()
{
    m_state.height   = DEFAULT_CAMERA_HEIGHT;
    m_state.pitch   = DEFAULT_CAMERA_PITCH * D3DX_PI / 180.0f;
    m_state.yaw     = 0.0f;
    m_state.posX    = 0.0f;
    m_state.posY    = 0.0f;
    m_state.offsetX = 0.0f;
    m_state.offsetY = DEFAULT_CAMERA_OFFSET_Y;
    m_state.fov     = DEFAULT_FOV_DEGREES * D3DX_PI / 180.0f;

    m_target.height   = DEFAULT_CAMERA_HEIGHT;
    m_target.offsetX  = 0.0f;
    m_target.offsetY  = DEFAULT_CAMERA_OFFSET_Y;
    m_target.fov      = DEFAULT_FOV_DEGREES * D3DX_PI / 180.0f;
    m_target.pitch    = DEFAULT_CAMERA_PITCH * D3DX_PI / 180.0f;

    m_cache.cachedInvertedYaw = 0.0f;
    m_cache.cachedSinYaw      = 0.0f;
    m_cache.cachedCosYaw      = 1.0f;
    m_cache.cachedTanHalfFov  = 0.0f;
    m_cache.cachedCameraPos   = D3DXVECTOR3(0.0f, 0.0f, 0.0f);
    m_cache.cachedCameraRot   = D3DXVECTOR3(0.0f, 0.0f, 0.0f);
    m_cache.isValid           = false;

    m_gModeActive         = false;
    m_gModeStartTime      = 0;
    m_returnTarget.height = DEFAULT_CAMERA_HEIGHT;
    m_returnTarget.offsetX = 0.0f;
    m_returnTarget.offsetY = DEFAULT_CAMERA_OFFSET_Y;
    m_returnTarget.fov    = DEFAULT_FOV_DEGREES * D3DX_PI / 180.0f;
    m_returnTarget.pitch  = DEFAULT_CAMERA_PITCH * D3DX_PI / 180.0f;
    m_gKeyPressedLastFrame = false;
}

void CameraController::UpdateFromGame(CPed* player)
{
    try
    {
        if (player)
        {
            const CVector pos = GameState::SafePlayerCentreForMap();
            m_state.posX = pos.x + 3000.0f;
            m_state.posY = pos.y - 3000.0f;
        }

        // Stock CRadar::CalculateCachedSinCos: MODE_1STPERSON (Hydra/Hunter cockpit)
        // uses the *vehicle* forward, not TheCamera.GetForward(). In a steep climb
        // camera forward XY ~ 0 and atan2 becomes garbage.
        CVector forward = TheCamera.GetForward();
        const CCam& cam = TheCamera.m_aCams[TheCamera.m_nActiveCam];
        if ((cam.m_nMode == MODE_1STPERSON || cam.m_nMode == MODE_HELICANNON_1STPERSON)
            && player && player->bInVehicle && player->m_pVehicle)
        {
            forward = player->m_pVehicle->GetForward();
        }
        if (forward.x != 0.0f || forward.y != 0.0f)
            m_state.yaw = atan2f(forward.x, forward.y);
    }
    catch (...) {}
}

void CameraController::UpdateFromSpeed(CPed* player)
{
    try
    {
        const bool radar3D = RadarConfig::GetRadar3D();
        static bool s_wasRadar3D = true;
        const bool modeChanged = (radar3D != s_wasRadar3D);
        s_wasRadar3D = radar3D;

        bool gKeyPressed = (GetAsyncKeyState(RadarConfig::GetZoomKeyVk()) & 0x8000) != 0;

        if (m_gModeActive)
        {
            unsigned int elapsed = CTimer::m_snTimeInMilliseconds - m_gModeStartTime;
            if (elapsed >= G_MODE_DURATION_MS)
            {
                m_target = m_returnTarget;
                m_gModeActive = false;
            }
            if (!radar3D)
            {
                m_target.offsetY = 0.0f;
                m_target.pitch   = -D3DX_PI * 0.5f;
            }
            else
                ApplyRadarCamContextOffsets(m_target, DetectRadarCamContext(player, true));
            UpdateInterpolation();
            if (modeChanged)
            {
                m_state.offsetY = m_target.offsetY;
                m_state.pitch   = m_target.pitch;
            }
            m_gKeyPressedLastFrame = gKeyPressed;
            return;
        }

        if (gKeyPressed && !m_gKeyPressedLastFrame)
        {
            m_returnTarget   = m_target;
            m_target.height  = DEFAULT_CAMERA_HEIGHT + 100.0f;
            if (radar3D)
            {
                m_target.pitch   = MAX_CAMERA_PITCH * D3DX_PI / 180.0f;
                m_target.offsetY = -15.0f;
            }
            else
            {
                m_target.pitch   = -D3DX_PI * 0.5f;
                m_target.offsetY = 0.0f;
            }
            m_gModeActive    = true;
            m_gModeStartTime = CTimer::m_snTimeInMilliseconds;
            m_gKeyPressedLastFrame = gKeyPressed;
            if (radar3D)
                ApplyRadarCamContextOffsets(m_target, RadarConfig::RadarCamContext::ZoomMax);
            UpdateInterpolation();
            return;
        }
        m_gKeyPressedLastFrame = gKeyPressed;

        if (!player || !player->bInVehicle || !player->m_pVehicle)
        {
            m_target.height  = DEFAULT_CAMERA_HEIGHT;
            m_target.fov     = DEFAULT_FOV_DEGREES * D3DX_PI / 180.0f;
            m_target.offsetY = DEFAULT_CAMERA_OFFSET_Y;
            m_target.pitch   = DEFAULT_CAMERA_PITCH * D3DX_PI / 180.0f;
        }
        else
        {
            CVehicle* vehicle    = player->m_pVehicle;
            int       modelIndex = vehicle->m_nModelIndex;

            if (CModelInfo::IsPlaneModel(modelIndex))
            {
                m_target.height  = CAMERA_HEIGHT_PLANE;
                m_target.fov     = DEFAULT_FOV_DEGREES * D3DX_PI / 180.0f;
                m_target.offsetY = CAMERA_OFFSET_Y_PLANE;
                m_target.pitch   = DEFAULT_CAMERA_PITCH * D3DX_PI / 180.0f;
            }
            else if (CModelInfo::IsHeliModel(modelIndex))
            {
                m_target.height  = CAMERA_HEIGHT_HELICOPTER;
                m_target.fov     = DEFAULT_FOV_DEGREES * D3DX_PI / 180.0f;
                m_target.offsetY = CAMERA_OFFSET_Y_HELICOPTER;
                m_target.pitch   = DEFAULT_CAMERA_PITCH * D3DX_PI / 180.0f;
            }
            else
            {
                CVector velocity  = vehicle->m_vecMoveSpeed;
                float   speedXY   = velocity.Magnitude2D();
                float   speedMS   = speedXY * 50.0f;
                float   speedKMH  = speedMS * 3.6f;

                if (speedKMH < 60.0f)
                {
                    m_target.height  = DEFAULT_CAMERA_HEIGHT;
                    m_target.fov    = DEFAULT_FOV_DEGREES * D3DX_PI / 180.0f;
                    m_target.offsetY = DEFAULT_CAMERA_OFFSET_Y;
                    m_target.pitch  = DEFAULT_CAMERA_PITCH * D3DX_PI / 180.0f;
                }
                else if (speedKMH >= 100.0f)
                {
                    m_target.height   = SPEED_MAX_HEIGHT;
                    m_target.fov      = SPEED_MAX_FOV_RAD;
                    m_target.offsetY  = SPEED_MAX_OFFSET_Y;
                    m_target.pitch    = DEFAULT_CAMERA_PITCH * D3DX_PI / 180.0f;
                }
                else
                {
                    float t = (speedKMH - 60.0f) / 40.0f;
                    m_target.height   = DEFAULT_CAMERA_HEIGHT + (SPEED_MAX_HEIGHT - DEFAULT_CAMERA_HEIGHT) * t;
                    m_target.fov      = DEFAULT_FOV_RAD + FOV_DELTA_RAD * t;
                    m_target.offsetY  = DEFAULT_CAMERA_OFFSET_Y + (SPEED_MAX_OFFSET_Y - DEFAULT_CAMERA_OFFSET_Y) * t;
                    m_target.pitch    = DEFAULT_CAMERA_PITCH * D3DX_PI / 180.0f;
                }
            }
        }

        // 2D: true top-down, no look-ahead camera shift
        if (!radar3D)
        {
            m_target.offsetY = 0.0f;
            m_target.pitch   = -D3DX_PI * 0.5f;
        }

        ApplyRadarCamContextOffsets(m_target, DetectRadarCamContext(player, false));

        UpdateInterpolation();
        if (modeChanged)
        {
            m_state.offsetY = m_target.offsetY;
            m_state.pitch   = m_target.pitch;
        }
    }
    catch (...) {}
}

void CameraController::ApplySettingsPreview(RadarConfig::RadarCamContext ctx)
{
    CameraTarget tgt = {};
    tgt.offsetX = 0.0f;

    switch (ctx)
    {
    case RadarConfig::RadarCamContext::Ped:
        tgt.height  = DEFAULT_CAMERA_HEIGHT;
        tgt.fov     = DEFAULT_FOV_RAD;
        tgt.offsetY = DEFAULT_CAMERA_OFFSET_Y;
        tgt.pitch   = DEFAULT_CAMERA_PITCH * D3DX_PI / 180.0f;
        break;
    case RadarConfig::RadarCamContext::Vehicle:
        tgt.height  = SPEED_MAX_HEIGHT;
        tgt.fov     = SPEED_MAX_FOV_RAD;
        tgt.offsetY = SPEED_MAX_OFFSET_Y;
        tgt.pitch   = DEFAULT_CAMERA_PITCH * D3DX_PI / 180.0f;
        break;
    case RadarConfig::RadarCamContext::Plane:
        tgt.height  = CAMERA_HEIGHT_PLANE;
        tgt.fov     = DEFAULT_FOV_RAD;
        tgt.offsetY = CAMERA_OFFSET_Y_PLANE;
        tgt.pitch   = DEFAULT_CAMERA_PITCH * D3DX_PI / 180.0f;
        break;
    case RadarConfig::RadarCamContext::Heli:
        tgt.height  = CAMERA_HEIGHT_HELICOPTER;
        tgt.fov     = DEFAULT_FOV_RAD;
        tgt.offsetY = CAMERA_OFFSET_Y_HELICOPTER;
        tgt.pitch   = DEFAULT_CAMERA_PITCH * D3DX_PI / 180.0f;
        break;
    case RadarConfig::RadarCamContext::ZoomMax:
        tgt.height  = DEFAULT_CAMERA_HEIGHT + 100.0f;
        tgt.fov     = DEFAULT_FOV_RAD;
        tgt.offsetY = -15.0f;
        tgt.pitch   = MAX_CAMERA_PITCH * D3DX_PI / 180.0f;
        break;
    default:
        tgt.height  = DEFAULT_CAMERA_HEIGHT;
        tgt.fov     = DEFAULT_FOV_RAD;
        tgt.offsetY = DEFAULT_CAMERA_OFFSET_Y;
        tgt.pitch   = DEFAULT_CAMERA_PITCH * D3DX_PI / 180.0f;
        break;
    }

    if (!RadarConfig::GetRadar3D())
    {
        tgt.offsetX = 0.0f;
        tgt.offsetY = 0.0f;
        tgt.pitch   = -D3DX_PI * 0.5f;
    }
    else
        ApplyRadarCamContextOffsets(tgt, ctx);

    m_state.posX    = MapChunkManager::MAP_CENTER_X;
    m_state.posY    = MapChunkManager::MAP_CENTER_Y;
    m_state.yaw     = 0.0f;
    m_state.height  = tgt.height;
    m_state.fov     = tgt.fov;
    m_state.offsetX = tgt.offsetX;
    m_state.offsetY = tgt.offsetY;
    m_state.pitch   = tgt.pitch;
    m_target        = tgt;
    m_cache.isValid = false;
}

void CameraController::UpdateInterpolation()
{
    const float interpolationSpeed       = 0.15f;
    const float gKeyInterpolationSpeed   = interpolationSpeed / 8.0f;

    float currentInterpolationSpeed = m_gModeActive ? gKeyInterpolationSpeed : interpolationSpeed;

    m_state.height  += (m_target.height - m_state.height) * currentInterpolationSpeed;
    m_state.fov     += (m_target.fov - m_state.fov) * currentInterpolationSpeed;
    m_state.offsetX += (m_target.offsetX - m_state.offsetX) * currentInterpolationSpeed;
    m_state.offsetY += (m_target.offsetY - m_state.offsetY) * currentInterpolationSpeed;
    m_state.pitch   += (m_target.pitch - m_state.pitch) * currentInterpolationSpeed;
}

void CameraController::GetCachedCalculations(float& offsetWorldX, float& offsetWorldY,
                                             D3DXVECTOR3& cameraPos, D3DXVECTOR3& cameraRot,
                                             float cameraHeight)
{
    // gta-sa-radar-trilogy: invertedYaw = -yaw, offset from sin/cos(invertedYaw), cameraRot.z = invertedYaw
    float invertedYaw = -m_state.yaw;
    if (!m_cache.isValid || fabsf(m_cache.cachedInvertedYaw - invertedYaw) > 0.001f)
    {
        m_cache.cachedInvertedYaw = invertedYaw;
        m_cache.cachedSinYaw      = sinf(invertedYaw);
        m_cache.cachedCosYaw      = cosf(invertedYaw);
    }

    const float lookAhead = RadarConfig::GetRadar3D() ? m_state.offsetY : 0.0f;
    const float lateral = RadarConfig::GetRadar3D() ? m_state.offsetX : 0.0f;
    offsetWorldX = lookAhead * m_cache.cachedSinYaw + lateral * m_cache.cachedCosYaw;
    offsetWorldY = lookAhead * m_cache.cachedCosYaw - lateral * m_cache.cachedSinYaw;

    if (cameraHeight <= 0.0f)
    {
        cameraHeight = m_state.height;
        if (cameraHeight <= 0.0f)
        {
            const float mapWidth = 6000.0f;
            if (!m_cache.isValid || fabsf(m_cache.cachedTanHalfFov - tanf(m_state.fov * 0.5f)) > 0.001f)
                m_cache.cachedTanHalfFov = tanf(m_state.fov * 0.5f);
            cameraHeight = mapWidth * 0.5f / m_cache.cachedTanHalfFov;
        }
    }

    const float pitch = RadarConfig::GetRadar3D() ? m_state.pitch : (-D3DX_PI * 0.5f);
    cameraPos = D3DXVECTOR3(m_state.posX + offsetWorldX, m_state.posY - offsetWorldY, cameraHeight);
    cameraRot = D3DXVECTOR3(pitch, 0.0f, invertedYaw);

    m_cache.cachedCameraPos = cameraPos;
    m_cache.cachedCameraRot = cameraRot;
    m_cache.isValid         = true;
}

bool CameraController::IsInAircraft(CPed* player) const
{
    try
    {
        if (!player || !player->bInVehicle || !player->m_pVehicle)
            return false;

        CVehicle* vehicle    = player->m_pVehicle;
        int       modelIndex = vehicle->m_nModelIndex;

        return CModelInfo::IsPlaneModel(modelIndex) || CModelInfo::IsHeliModel(modelIndex);
    }
    catch (...) { return false; }
}

bool CameraController::IsInPlane(CPed* player) const
{
    try
    {
        if (!player || !player->bInVehicle || !player->m_pVehicle)
            return false;

        CVehicle* vehicle    = player->m_pVehicle;
        int       modelIndex = vehicle->m_nModelIndex;

        return CModelInfo::IsPlaneModel(modelIndex);
    }
    catch (...) { return false; }
}

float CameraController::GetVehicleRollAngle(CPed* player) const
{
    try
    {
        if (!player || !player->bInVehicle || !player->m_pVehicle)
            return 0.0f;

        CVehicle*   vehicle    = player->m_pVehicle;
        CMatrixLink& matrixLink = vehicle->GetMatrix();

        return atan2f(-matrixLink.right.z, matrixLink.at.z);
    }
    catch (...) { return 0.0f; }
}

float CameraController::GetVehiclePitchAngle(CPed* player) const
{
    try
    {
        if (!player || !player->bInVehicle || !player->m_pVehicle)
            return 0.0f;

        CVehicle*   vehicle    = player->m_pVehicle;
        CMatrixLink& matrixLink = vehicle->GetMatrix();

        return asinf(matrixLink.up.z);
    }
    catch (...) { return 0.0f; }
}
