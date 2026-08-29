/*****************************************************************************
 *
 *  PROJECT:     The-Definitive-UI
 *  FILE:        source/Shader/ShaderManager.cpp
 *  PURPOSE:     D3DX effect compilation and lifetime
 *
 *****************************************************************************/

#include "ShaderManager.h"
#include <cstring>

// Optimized FX even in Debug — SKIPOPTIMIZATION was crushing GPU FPS.
const DWORD ShaderManager::s_shaderFlags = D3DXSHADER_USE_LEGACY_D3DX9_31_DLL;

ShaderManager::ShaderManager(LPDIRECT3DDEVICE9 pDevice)
    : m_pDevice(pDevice)
{
}

ShaderManager::~ShaderManager()
{
    Shutdown();
}

bool ShaderManager::Initialize()
{
    if (!m_pDevice)
        return false;

    D3DCAPS9 caps;
    if (FAILED(m_pDevice->GetDeviceCaps(&caps)))
        return false;

    if (D3DSHADER_VERSION_MAJOR(caps.PixelShaderVersion) < 2 || D3DSHADER_VERSION_MAJOR(caps.VertexShaderVersion) < 2)
        return false;

    return true;
}

void ShaderManager::Shutdown()
{
    for (auto& [name, pEffect] : m_effects)
    {
        if (pEffect)
            pEffect->Release();
    }
    m_effects.clear();
}

LPD3DXEFFECT ShaderManager::LoadEffectFromString(const char* shaderCode, const char* name)
{
    if (!shaderCode || !name || !m_pDevice)
        return nullptr;

    std::string key(name);
    auto        it = m_effects.find(key);
    if (it != m_effects.end())
        return it->second;

    LPD3DXEFFECT pEffect = nullptr;
    LPD3DXBUFFER pErrors = nullptr;

    HRESULT hr = D3DXCreateEffect(m_pDevice, shaderCode, (UINT)strlen(shaderCode), nullptr, nullptr, s_shaderFlags, nullptr, &pEffect, &pErrors);

    if (pErrors)
        pErrors->Release();

    if (FAILED(hr))
        return nullptr;

    m_effects[key] = pEffect;
    return pEffect;
}

LPD3DXEFFECT ShaderManager::LoadEffectFromFile(const char* path, const char* name)
{
    if (!path || !name || !m_pDevice)
        return nullptr;

    std::string key(name);
    auto        it = m_effects.find(key);
    if (it != m_effects.end())
        return it->second;

    LPD3DXEFFECT pEffect = nullptr;
    LPD3DXBUFFER pErrors = nullptr;

    HRESULT hr = D3DXCreateEffectFromFileA(m_pDevice, path, nullptr, nullptr, s_shaderFlags, nullptr, &pEffect, &pErrors);

    if (pErrors)
        pErrors->Release();

    if (FAILED(hr))
        return nullptr;

    m_effects[key] = pEffect;
    return pEffect;
}

void ShaderManager::ReleaseEffect(LPD3DXEFFECT pEffect)
{
    if (!pEffect)
        return;

    for (auto it = m_effects.begin(); it != m_effects.end(); ++it)
    {
        if (it->second == pEffect)
        {
            it->second->Release();
            m_effects.erase(it);
            return;
        }
    }
}

LPD3DXEFFECT ShaderManager::GetEffect(const std::string& name)
{
    auto it = m_effects.find(name);
    return it != m_effects.end() ? it->second : nullptr;
}
