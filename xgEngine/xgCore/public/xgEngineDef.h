// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.
#pragma once
#include "xgBase.h"
#include "xgTypeRegistry.h"
#include "xgEngineDef.generated.h"

namespace xg
{
    XG_SERIALIZABLE()
    struct EngineConfig
    {
        XG_DECLARE_TYPE_NAME(EngineConfig);

        XG_FIELD()
        const char* RendererModule = nullptr;

        XG_FIELD()
        bool EditorUI = true;

        XG_FIELD()
        bool EditorMultiViewport = true;

        // Game project folder to open (xge.exe --project <path> or gameconfig "ProjectPath"); empty for none.
        XG_FIELD()
        const char* ProjectPath = nullptr;

        // Set by xge.exe --new-project <path>: the editor creates the project there, then opens it.
        XG_FIELD()
        const char* NewProjectPath = nullptr;

        // Build the project's scripts when it opens and they are stale (xge.exe --no-script-build or gameconfig false to skip).
        XG_FIELD()
        bool ProjectAutoBuild = true;
    };

    XG_ENUM(inherit = byte)
    enum class ThreadDomain : uint8_t
    {
        MainThread = 0,
        WorkerThread,
        DedicatedThread,
        PinnedThread
    };
}