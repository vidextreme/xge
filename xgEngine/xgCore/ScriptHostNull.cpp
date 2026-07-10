// Copyright(c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.

#include "pch.h"
#include "ScriptHostNull.h"
#include "ScriptModuleNull.h"

namespace xg
{
    ScriptHostNull::ScriptHostNull(ScriptEngine* engine)
        : _engine(engine)
    {}

    ScriptHostNull::~ScriptHostNull() = default;

    ScriptModule* ScriptHostNull::LoadModule(const char* id,
        uint32_t moduleID,
        const char* path,
        const char* group)
    {
		return new ScriptModuleNull(id, moduleID, group);
    }

    bool ScriptHostNull::Encode(const void*,
        const TypeSchema*,
        ScriptMessage&)
    {
        return false;
    }

    bool ScriptHostNull::Decode(const ScriptMessage&,
        const TypeSchema*,
        void*)
    {
        return false;
    }

    CodecRegistry* ScriptHostNull::GetCodecRegistry() const
    {
        return nullptr;
    }

    PayloadMode ScriptHostNull::GetPayloadMode() const
    {
        return PayloadMode::BINARY;
    }

    ScriptEngine* ScriptHostNull::GetEngine() const
    {
        return _engine;
    }
}
