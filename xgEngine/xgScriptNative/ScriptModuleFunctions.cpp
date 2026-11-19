// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.
#include "pch.h"
#include "ScriptModuleFunctions.h"
#include "xgSceneGraph.h"
#include "xgScriptEngine.h"

#include <cstring>

namespace xg
{
    ScriptModuleFunctions::ScriptModuleFunctions(std::string id, uint32_t moduleId, std::string group, const XG_ModuleFunctions& functions)
        : ScriptModule(nullptr, moduleId, nullptr, nullptr)
        , _idText(std::move(id))
        , _groupText(std::move(group))
        , _functions(functions)
    {
        SetNames(_idText.c_str(), GroupOrNull(_groupText));
    }

    bool ScriptModuleFunctions::Init(ScriptEngine* engine)
    {
        if (!IsValid() || _initialized)
            return false;
        if (!_functions.Init(engine, GetModuleId()))
            return false;
        _initialized = true;
        return true;
    }

    void ScriptModuleFunctions::Update(float dt)
    {
        if (_initialized)
            _functions.Update(dt);
    }

    void ScriptModuleFunctions::Shutdown()
    {
        if (_initialized)
            _functions.Shutdown();
        _initialized = false;
    }

    bool ScriptModuleFunctions::IsValid() const
    {
        return _functions.Init && _functions.Update && _functions.Shutdown;
    }

    void ScriptModuleFunctions::OnMessage(const ScriptMessage& msg)
    {
        if (!_initialized || !_functions.OnMessage)
            return;
        void* payloadCopy = nullptr;
        if (msg.Payload && msg.PayloadSize > 0)
        {
            payloadCopy = XG_MANAGED_ALLOC(msg.PayloadSize);
            std::memcpy(payloadCopy, msg.Payload, msg.PayloadSize);
        }
        _functions.OnMessage(msg.TypeName, payloadCopy, msg.PayloadSize);
        if (payloadCopy)
            XG_MANAGED_FREE(payloadCopy);
    }

    SceneGraph* ScriptModuleFunctions::GetSceneGraph()
    {
        return _engine ? _engine->GetSubsystem<SceneGraph>(this) : nullptr;
    }
}
