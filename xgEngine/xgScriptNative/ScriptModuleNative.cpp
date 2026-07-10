// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.
#include "pch.h"
#include "ScriptModuleNative.h"
#include "xgScriptEngine.h"
#include "xgScriptHost.h"   // for ScriptHost base
#include "xgModules.h"      // for UnloadModule

namespace xg
{
    ScriptModuleNative::ScriptModuleNative(
        const char* id,
        uint32_t moduleID,
        ScriptHost* host,
        const char* group,
        void* lib,
        InitFunc init,
        UpdateFunc update,
        ShutdownFunc shutdown,
        OnMessageFunc onMessage)
        : ScriptModule(id, moduleID, host, group)
        , _nativeHost(host)   // typed reference
        , _lib(lib)
        , _init(init)
        , _update(update)
        , _shutdown(shutdown)
        , _onMessage(onMessage)
    {
        _isValid =
            (_lib != nullptr) &&
            (_init != nullptr) &&
            (_update != nullptr) &&
            (_shutdown != nullptr);
		    //_onMessage is optional, so we don't check it for validity
    }

    ScriptModuleNative::~ScriptModuleNative()
    {
        if (_initialized)
            Shutdown();

        if (_lib)
        {
            xg::UnloadModule(_lib);
            _lib = nullptr;
        }
    }

    bool ScriptModuleNative::Init(ScriptEngine* engine)
    {
        if (!_isValid || !_init || _initialized)
            return false;

        if (!_init(engine, GetModuleId()))
        {
            _isValid = false;
            return false;
        }

        _initialized = true;
        return true;
    }

    void ScriptModuleNative::Update(float dt)
    {
        if (_isValid && _initialized && _update)
            _update(dt);
    }

    void ScriptModuleNative::Shutdown()
    {
        if (_isValid && _initialized && _shutdown)
            _shutdown();

        _initialized = false;
    }

    bool ScriptModuleNative::IsValid() const
    {
        return _isValid;
    }
    void ScriptModuleNative::OnMessage(const ScriptMessage& msg)
    {
        if (!_onMessage)
            return;

        void* payloadCopy = nullptr;
        if (msg.Payload && msg.PayloadSize > 0)
        {
            payloadCopy = XG_MANAGED_ALLOC(msg.PayloadSize);
            memcpy(payloadCopy, msg.Payload, msg.PayloadSize);
        }

        _onMessage(msg.TypeName, payloadCopy, msg.PayloadSize);

        if (payloadCopy)
            XG_MANAGED_FREE(payloadCopy);
    }
}
