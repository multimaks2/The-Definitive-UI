/*****************************************************************************
 *
 *  PROJECT:     The-Definitive-UI
 *  FILE:        source/pch.h
 *  PURPOSE:     Precompiled header — heavy stable includes only
 *
 *****************************************************************************/

#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <windows.h>
#include <d3d9.h>
#include <d3dx9.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include "plugin.h"
#include "RenderWare.h"
