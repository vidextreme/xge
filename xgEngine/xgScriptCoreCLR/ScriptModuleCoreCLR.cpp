#include "pch.h"
#include "ScriptModuleCoreCLR.h"
#include "ScriptHostCoreCLR.h"
#include <filesystem>
#include "xgDynamicObject.h"
#include "xgCodecRegistry.h"
#include "xgCodecJson.h"
#include "xgCodecBinary.h"
namespace xg
{
    ScriptModuleCoreCLR::ScriptModuleCoreCLR(const char* id,
        ScriptHostCoreCLR* host,
        const char* group)
        : ScriptModule(id, host, group)
        , _coreclrHost(host)
    {
    }

    ScriptModuleCoreCLR::~ScriptModuleCoreCLR()
    {
        Shutdown();
    }

    static const xg::FieldSchema TestFields[] =
    {
        { "Health", "Int32", xg::ValueType::Int32, 0, sizeof(int32_t), alignof(int32_t), {nullptr} },
        { "Name",   "String", xg::ValueType::String, 0, sizeof(const char*), alignof(const char*), {nullptr} }
    };

    static const xg::TypeSchema TestSchema =
    {
        "TestType",
        1,
        false,              // IMPORTANT: dynamic layout
        TestFields,
        2,
        0,
        0
    };

    bool ScriptModuleCoreCLR::Load(const char* path)
    {
        if (!_coreclrHost)
            return false;

        // Derive assembly + type name from the DLL path
        std::string assemblyName = std::filesystem::path(path).stem().string();
        std::string typeName = assemblyName + ".ScriptEntry";

        if (!_coreclrHost->GetEntryPoints(
            assemblyName.c_str(),
            typeName.c_str(),
            &_managedInit,
            &_managedUpdate,
            &_managedShutdown))
        {
            return false;
        }

        _valid = true;
        return true;
    }

    bool ScriptModuleCoreCLR::Init(ScriptEngine* engine)
    {
        if (_managedInit)
        {
            using InitFn = int(*)(ScriptEngine*);
            int result = ((InitFn)_managedInit)(engine);
            CodecRegistry* registry =
                _engine->GetCodecRegistry(ScriptBackendType::CoreCLR);
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
            return result != 0;
        }
        return false;
    }

    void ScriptModuleCoreCLR::Update(float dt)
    {
        if (_managedUpdate)
        {
            using UpdateFn = void(*)(float);
            ((UpdateFn)_managedUpdate)(dt);
        }
    }

    void ScriptModuleCoreCLR::Shutdown()
    {
        if (_managedShutdown)
        {
            using ShutdownFn = void(*)();
            ((ShutdownFn)_managedShutdown)();
        }

        _valid = false;
    }

    bool ScriptModuleCoreCLR::IsValid() const
    {
        return _valid;
    }
    void ScriptModuleCoreCLR::OnMessage(const ScriptMessage& msg)
    {
        xg::CodecRegistry* codecs = _host->GetCodecRegistry();
        xg::DynamicObject obj2(&TestSchema);

        bool ok2 = codecs->Decode("TestType", msg, &TestSchema, &obj2);

        if (!ok2)
            printf("Decode failed\n");
        else
        {
            printf("Decoded Health = %d\n", obj2.FindField("Health")->Value.Int32Value);
            printf("Decoded Name   = %s\n", obj2.FindField("Name")->Value.StringValue);
        }
    }
}