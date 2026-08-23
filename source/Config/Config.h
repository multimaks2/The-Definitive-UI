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
    bool GetShowGangZones();
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

    void SetShapeCircle(bool useCircle);
    void SetShowGangZones(bool value);
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
}
