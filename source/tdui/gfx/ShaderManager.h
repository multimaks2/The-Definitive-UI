/*****************************************************************************
 *
 *  PROJECT:     The-Definitive-UI
 *  FILE:        source/Shader/ShaderManager.h
 *  PURPOSE:     D3DX effect compilation and lifetime
 *
 *****************************************************************************/

#pragma once

#include <d3d9.h>
#include <d3dx9.h>
#include <map>
#include <string>

class ShaderManager
{
public:
    ShaderManager(LPDIRECT3DDEVICE9 pDevice);
    ~ShaderManager();

    bool         Initialize();
    void         Shutdown();
    LPD3DXEFFECT LoadEffectFromString(const char* shaderCode, const char* name);
    LPD3DXEFFECT LoadEffectFromFile(const char* path, const char* name);
    void         ReleaseEffect(LPD3DXEFFECT pEffect);
    LPD3DXEFFECT GetEffect(const std::string& name);

private:
    LPDIRECT3DDEVICE9                  m_pDevice;
    std::map<std::string, LPD3DXEFFECT> m_effects;

    static const DWORD s_shaderFlags;
};
