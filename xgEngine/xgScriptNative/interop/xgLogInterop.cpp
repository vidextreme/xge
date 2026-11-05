// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.
#include "pch.h"
#include "xgLog.h"
#include "xgModules.h"
#include "xgScriptEngine.h"

XG_MODULE_EXPORT void XGAddLogCallbackForEngine(
    xg::ScriptEngine* engine,
    xg::LogCallback callback)
{
    if (!engine || !callback)
        return;

    engine->AddLogCallback(callback);
}

XG_MODULE_EXPORT void XGLogForEngine(
    xg::ScriptEngine* engine,
    int32_t type,
    const char* message)
{
    if (!engine || !message)
        return;

    if (xg::Logger* logger = engine->GetLogger())
        logger->Write(type, message);
}
