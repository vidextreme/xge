#pragma once

#include "xgScriptHost.h"
#include "xgModules.h"

namespace xg
{
    //
    // ScriptHostNative
    //
    class ScriptHostNative : public ScriptHost
    {
    public:
        XG_IMPL_REFCOUNTED()
        ScriptHostNative(ScriptEngine* engine);
        ~ScriptHostNative() override = default;

        // Load a native script module from the given path.
        ScriptModule* LoadModule(const char* id, uint32_t moduleID, const char* path, const char* group) override;
        ScriptEngine* GetEngine() const override;

        CodecRegistry* GetCodecRegistry() const override;
        PayloadMode GetPayloadMode() const override;

        bool Encode(const void* object,
            const TypeSchema* schema,
            ScriptMessage& outMessage) override;

        virtual bool Decode(const ScriptMessage& message,
            const TypeSchema* schema,
            void* outObject) override;
    private:
		ScriptEngine* _engine = nullptr;
    };
}
