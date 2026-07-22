// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.
#pragma once

#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers

// CoreCLR hosting headers
#include "hostfxr.h"
#include "coreclr_delegates.h"

// Public xgBase headers
#include "xgHandles.h"
#include "xgWindow.h"
#include "xgRenderer.h"
#include "xgScriptEngine.h"
#include "xgScriptHost.h"
#include "xgEventQueue.h"
#include "xgEventDispatcher.h"
#include "xgLog.h"
#include "xgEngineDef.h"
#include "xgModules.h"
#include "xgScriptModule.h"
#include "xgCallback.h"
#include "xgMemberCallback.h"
#include "xgListenerBinding.h"
#include "xgBase.h"
#include "xgSystem.h"