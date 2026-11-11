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

        // Main window title and initial client size.
        XG_FIELD()
        const char* WindowTitle = nullptr;

        XG_FIELD()
        int WindowWidth = 1280;

        XG_FIELD()
        int WindowHeight = 720;
    };

    // One entry of gameconfig.json "ScriptModules" (each entry may have nested "Children" loaded under it).
    XG_SERIALIZABLE()
    struct ScriptModuleDesc
    {
        XG_DECLARE_TYPE_NAME(ScriptModuleDesc);

        XG_FIELD()
        const char* Id = nullptr;

        XG_FIELD()
        const char* Path = nullptr;

        // Script host group (modules in one group share a host, e.g. one CoreCLR runtime).
        XG_FIELD()
        const char* Group = nullptr;

        // Dispatch group for scheduling; reserved, not used yet.
        XG_FIELD()
        const char* DispatchGroup = nullptr;
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