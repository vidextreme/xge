// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.
#pragma once

#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers

// SDL3
#include <SDL3/SDL.h>

// Public xgBase headers (via xgBase framework)
#include "public/xgHandles.h"
#include "public/xgWindow.h"
#include "public/xgEvent.h"
#include "public/xgEventHelpers.h"
#include "public/xgEventQueue.h"
#include "public/xgEventDispatcher.h"
#include "public/xgLog.h"
#include "public/xgBase.h"
#include "public/xgSystem.h"

// Platform-specific utility headers
#include "platform/xgEventToSDL.h"
#include "platform/xgSDLToEvent.h"