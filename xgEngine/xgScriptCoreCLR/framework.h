// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.
#pragma once

#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers

// CoreCLR hosting headers
#include "hostfxr.h"
#include "coreclr_delegates.h"

// Public xgBase headers
#include "public/xgHandles.h"
#include "public/xgWindow.h"
#include "public/xgRenderer.h"
#include "public/xgScriptEngine.h"
#include "public/xgScriptHost.h"
#include "public/xgEventQueue.h"
#include "public/xgEventDispatcher.h"
#include "public/xgLog.h"
#include "public/xgEngineDef.h"
#include "public/xgModules.h"
#include "public/xgScriptModule.h"
#include "public/xgCallback.h"
#include "public/xgMemberCallback.h"
#include "public/xgListenerBinding.h"
#include "public/xgBase.h"
#include "public/xgSystem.h"