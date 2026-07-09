#include "pch.h"
#include "ScriptHostNative.h"
#include "ScriptModuleNative.h"
#include "xgModules.h"
#include "xgCodecRegistry.h"
#include "xgCodecJson.h"
#include "xgCodecBinary.h"
namespace xg
{
    using ScriptModuleFunc = ScriptModuleNative::ScriptModuleFunc;

    bool DummyEncoder(const void* src,
        const xg::TypeSchema* schema,
        xg::ScriptMessage& msg)
    {
        // Should never be called for dynamic types
        return false;
    }

    bool DummyDecoder(const xg::ScriptMessage& msg,
        const xg::TypeSchema* schema,
        void* dst)
    {
        // Should never be called for dynamic types
        return false;
    }

    ScriptHostNative::ScriptHostNative(ScriptEngine* engine) 
        : _engine(engine)
    {
        // Get the shared registry for the Native backend
        CodecRegistry* registry =
            _engine->GetCodecRegistry(ScriptBackendType::Native);

        PayloadMode mode = _engine->GetPayloadMode();
        if (mode == PayloadMode::JSON)
        {
            // Register generic JSON codec
            registry->RegisterEncoder("*", Encode_JSON_Generic);
            registry->RegisterDecoder("*", Decode_JSON_Generic);
		}
        else if (mode == PayloadMode::BINARY)
        {
            // Register generic binary codec
            registry->RegisterEncoder("*", Encode_Binary_Generic);
            registry->RegisterDecoder("*", Decode_Binary_Generic);
        }
    }

    ScriptModule* ScriptHostNative::LoadModule(const char* id, uint32_t moduleID, const char* path, const char* group)
    {
        if (!path)
            return nullptr;

        // Load the native DLL
        ModuleHandle lib = xg::LoadModule(path);
        if (!lib)
            return nullptr;

        // First try the modern NativeAOT-style factory
        auto moduleFn = reinterpret_cast<ScriptModuleFunc>(
            xg::GetSymbol(lib, "CreateScriptModule"));

        if (moduleFn)
        {
            // Module is responsible for unloading lib
            ScriptModule* module = moduleFn(id, moduleID, this, group);

            if (!module || !module->IsValid())
            {
                delete module;
                xg::UnloadModule(lib);
                return nullptr;
            }

            // ScriptModuleNative must store lib internally
            return module;
        }

        // Fallback: legacy C ABI triple-function module
        InitFunc initFn = reinterpret_cast<InitFunc>(
            xg::GetSymbol(lib, "ScriptModule_Init"));
        UpdateFunc updateFn = reinterpret_cast<UpdateFunc>(
            xg::GetSymbol(lib, "ScriptModule_Update"));
        ShutdownFunc shutdownFn = reinterpret_cast<ShutdownFunc>(
            xg::GetSymbol(lib, "ScriptModule_Shutdown"));

        if (!initFn || !updateFn || !shutdownFn)
        {
            xg::UnloadModule(lib);
            return nullptr;
        }

        // Create ScriptModuleNative wrapper
        ScriptModule* module =
            new ScriptModuleNative(id, moduleID, this, group, lib, initFn, updateFn, shutdownFn);

        if (!module->IsValid())
        {
            delete module;
            xg::UnloadModule(lib);
            return nullptr;
        }

        XG_ADDREF(this);

        return module;
    }

    ScriptEngine* ScriptHostNative::GetEngine() const
    {
        return _engine;
    }
    CodecRegistry* ScriptHostNative::GetCodecRegistry() const
    {
        return _engine->GetCodecRegistry(ScriptBackendType::Native);
    }
    PayloadMode ScriptHostNative::GetPayloadMode() const
    {
		return _engine->GetPayloadMode();
    }
    bool ScriptHostNative::Encode(const void* object, const TypeSchema* schema, ScriptMessage& outMessage)
    {
        // 2. Get the Native backend codec registry
        CodecRegistry* registry =
            _engine->GetCodecRegistry(ScriptBackendType::Native);

        // 3. Find encoder for this type + mode
        EncoderFn encoder = registry->GetEncoder(schema->Name);
        if (!encoder)
            return false;

        // 4. Perform encoding
        outMessage.TypeName = schema->Name;

        return encoder(object, schema, outMessage);
    }
    bool ScriptHostNative::Decode(const ScriptMessage& message, const TypeSchema* schema, void* outObject)
    {
        return false;
    }
}
