// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.
#include "pch.h"
#include "ScriptModuleNull.h"
#include "xgSceneGraph.h"
namespace xg
{
    ScriptModuleNull::ScriptModuleNull(const char* id,
        uint32_t moduleID,
        const char* group)
        : ScriptModule(id, moduleID, nullptr /* no host */, group)
    {}

    ScriptModuleNull::~ScriptModuleNull()
    {
        Shutdown();
    }

    bool ScriptModuleNull::Init(ScriptEngine* engine)
    {
        _engine = engine;
        _isValid = (engine != nullptr);
        return _isValid;
    }

    void ScriptModuleNull::Update(float /*dt*/)
    {
        // Pure logic module — no host, no runtime.
        // Add logic here if needed.
    }

    void ScriptModuleNull::Shutdown()
    {
        _engine = nullptr;
        _isValid = false;
    }

    bool ScriptModuleNull::IsValid() const
    {
        return _isValid;
    }

    void ScriptModuleNull::OnMessage(const ScriptMessage& /*msg*/)
    {
        // No-op: logic module does not decode or process payloads by default.
        // Add routing or handling logic here if desired.
    }
    SceneGraph* ScriptModuleNull::GetSceneGraph()
    {
        return _engine->GetSystem<SceneGraph>(this);
    }
}
