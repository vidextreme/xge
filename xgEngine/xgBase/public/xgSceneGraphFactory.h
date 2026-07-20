#pragma once
#include "xgSceneGraph.h"

namespace xg
{
    class ActorRegistry;

    //
    // SceneGraphFactory (stable ABI)
    //
    // Users and engine can register SceneGraph types by name.
    // ScriptEngine can request new SceneGraphs by type name.
    //
    class SceneGraphFactory : public System
    {
        XG_DECLARE_BASE_SYSTEM(SceneGraphFactory)

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