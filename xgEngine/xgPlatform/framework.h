// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.
#pragma once

#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers

// SDL3
#include <SDL3/SDL.h>

// Public xgBase headers (via xgBase framework)
#include "xgHandles.h"
#include "xgWindow.h"
#include "xgEvent.h"
#include "xgEventHelpers.h"
#include "xgEventQueue.h"
#include "xgEventDispatcher.h"
#include "xgLog.h"
#include "xgBase.h"
#include "xgSystem.h"

// Platform-specific utility headers
#include "platform/xgEventToSDL.h"
#include "platform/xgSDLToEvent.h"