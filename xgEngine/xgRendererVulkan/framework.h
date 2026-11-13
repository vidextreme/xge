// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.
#pragma once

#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers
#include <windows.h>

// Vulkan through volk: vulkan-1.dll is loaded at runtime, no import library
#include "volk.h"

// Public xgBase headers
#include "xgHandles.h"
#include "xgWindow.h"
#include "xgRenderer.h"
#include "xgBase.h"
#include "xgSubsystem.h"
