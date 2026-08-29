/*****************************************************************************
 *
 *  PROJECT:     The-Definitive-UI
 *  FILE:        source/Config/Config.h
 *  PURPOSE:     Global plugin configuration (radar ini + future settings)
 *
 *****************************************************************************/

#pragma once

class Config
{
public:
    Config();
    ~Config();
};

namespace RadarConfig
{
    void Load();
    void Save();
    const char* GetConfigPath();

    bool GetShapeCircle();
    bool GetRadar3D();
    bool GetShowGangZones();
    bool GetBlipEdgeFade();
    int  GetBlipIconScalePercent();
    bool GetRadarRender();
    bool GetMenuRender();
    bool GetDeIcons();
    bool GetCustomRadarTxd();
    bool GetUpdatedHelp();
    bool GetRadioText();
    bool GetGps();
    bool GetHeatHaze();
    bool GetSpeedBlur();
    int  GetWindowMode();
    bool HasWindowModeOverride();
    int  GetWindowWidth();
    int  GetWindowHeight();
    int  GetColorDepth();
    bool HasWindowResolution();
    bool HasColorDepth();
    const char* GetUiLanguage();
    int  GetZoomKeyVk();
    int  GetCircleSize();
    int  GetSquareSizeX();
    int  GetSquareSizeY();
    int  GetBorderThickness();
    int  GetOffsetX();
    int  GetOffsetY();
    void GetBackgroundColor(int& outR, int& outG, int& outB);
    void GetBackgroundColor(int& outR, int& outG, int& outB, int& outA);
    void GetCircleColor(int& outR, int& outG, int& outB, int& outA);
    void GetBorderColor(int& outR, int& outG, int& outB, int& outA);

    enum class RadarCamContext : int
    {
        Ped = 0,
        Vehicle,
        Plane,
        Heli,
        ZoomMax,
        Count
    };

    struct RadarCamOffsets
    {
        float offsetX = 0.0f;
        float offsetY = 0.0f;
        float offsetZ = 0.0f;
        float pitchDeg = 0.0f;
        float fovDeg = 0.0f;
    };

    RadarCamOffsets GetCamOffsets(RadarCamContext ctx);
    void SetCamOffsetX(RadarCamContext ctx, float value, bool persist = true);
    void SetCamOffsetY(RadarCamContext ctx, float value, bool persist = true);
    void SetCamOffsetZ(RadarCamContext ctx, float value, bool persist = true);
    void SetCamPitchDeg(RadarCamContext ctx, float value, bool persist = true);
    void SetCamFovDeg(RadarCamContext ctx, float value, bool persist = true);
    float GetCullRadiusMul();
    float GetCullRadiusAdd();
    float GetCullNdcMargin(); // internal tuning only (not exposed in UI)

    void SetShapeCircle(bool useCircle);
    void SetRadar3D(bool use3D);
    void SetShowGangZones(bool value);
    void SetBlipEdgeFade(bool value);
    void SetBlipIconScalePercent(int percent);
    void SetDeIcons(bool value);
    void SetCustomRadarTxd(bool value);
    void SetUpdatedHelp(bool value);
    void SetRadioText(bool value);
    void SetGps(bool value);
    void SetHeatHaze(bool value);
    void SetSpeedBlur(bool value);
    void SetWindowMode(int value);
    void ClearWindowMode();
    void SetWindowResolution(int width, int height, int depth = 0);
    void SetColorDepth(int depth);
    void SetUiLanguage(const char* name);
    void SetCircleSize(int value);
    void SetSquareSizeX(int value);
    void SetSquareSizeY(int value);
    void SetBorderThickness(int value);
    void SetOffsetX(int value);
    void SetOffsetY(int value);
    void SetBackgroundColor(int r, int g, int b, int a = 255, bool persist = true);
    void SetCircleColor(int r, int g, int b, int a = 255, bool persist = true);
    void SetBorderColor(int r, int g, int b, int a = 255, bool persist = true);
    void SetCullRadiusMul(float value, bool persist = true);
    void SetCullRadiusAdd(float value, bool persist = true);
    void ResetRadarAppearance();
    void ResetRadarCamera();
}
