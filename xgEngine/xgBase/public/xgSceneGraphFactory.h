// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.
#pragma once
#include "xgSceneGraph.h"
#include "xgSubsystem.h"

namespace xg
{
    class ActorRegistry;

    //
    // SceneGraphFactory (stable ABI)
    //
    // Users and engine can register SceneGraph types by name.
    // ScriptEngine can request new SceneGraphs by type name.
    //
    class SceneGraphFactory : public Subsystem
    {
        XG_DECLARE_BASE_SUBSYSTEM(SceneGraphFactory)

    public:
        virtual ~SceneGraphFactory() = default;

        // Register a graph type by name
        virtual void RegisterType(const char* typeName,
            SceneGraph* (*creator)(ActorRegistry&)) = 0;

        // Create a graph by name
        virtual SceneGraph* Create(const char* typeName,
            ActorRegistry& registry) = 0;
    };
}