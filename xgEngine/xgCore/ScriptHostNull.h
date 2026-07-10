// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.
#pragma once

#include "xgScriptHost.h"

namespace xg
{
    //
    // ScriptHostNull
    //
    // A backend that intentionally does nothing.
    // Used for logic-only ScriptModules that do not require
    // CoreCLR, NativeAOT, Lua, or any runtime backend.
    //
    class ScriptHostNull : public ScriptHost
    {
    public:
        XG_IMPL_REFCOUNTED()
        explicit ScriptHostNull(ScriptEngine* engine);
        ~ScriptHostNull() override;

        ScriptModule* LoadModule(const char* id,
            uint32_t moduleID,
            const char* path,
            const char* group) override;

        bool Encode(const void* object,
            const TypeSchema* schema,
            ScriptMessage& outMessage) override;

        bool Decode(const ScriptMessage& message,
            const TypeSchema* schema,
            void* outObject) override;

        CodecRegistry* GetCodecRegistry() const override;
        PayloadMode GetPayloadMode() const override;
        ScriptEngine* GetEngine() const override;

    private:
        ScriptEngine* _engine = nullptr;
    };
}
