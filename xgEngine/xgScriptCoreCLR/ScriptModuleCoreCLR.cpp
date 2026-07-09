#include "pch.h"
#include "ScriptModuleCoreCLR.h"
#include "ScriptHostCoreCLR.h"
#include <filesystem>
#include "xgDynamicObject.h"
#include "xgCodecRegistry.h"
#include "xgCodecJson.h"
#include "xgCodecBinary.h"


#define XG_SCRIPTENTRY_TYPE "ScriptEntry"

namespace xg
{
    std::string GetFullPath(const char* path)
    {
        char buffer[MAX_PATH];
        DWORD len = GetFullPathNameA(path, MAX_PATH, buffer, nullptr);
        return (len == 0 || len >= MAX_PATH) ? std::string(path) : std::string(buffer);
    }

    std::wstring ToWide(const std::string& s)
    {
        if (s.empty())
            return std::wstring();

        int len = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
        if (len <= 0)
            return std::wstring();

        std::wstring w(len - 1, L'\0');
        MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, &w[0], len);
        return w;
    }

    const char_t* ToCharT(const std::wstring& w)
    {
#if defined(_WIN32)
        // Windows: wchar_t == char_t (UTF‑16)
        return reinterpret_cast<const char_t*>(w.c_str());
#else
        // Linux/macOS: wchar_t = UTF‑32 → convert to UTF‑8
        thread_local std::string utf8;

        utf8.clear();
        utf8.reserve(w.size());

        for (wchar_t c : w)
        {
            if (c < 0x80)
            {
                utf8.push_back(static_cast<char>(c));
            }
            else
            {
                // Full UTF‑32 → UTF‑8 encoding needed here
                throw std::runtime_error("Non‑ASCII wchar_t conversion not implemented");
            }
        }

        return reinterpret_cast<const char_t*>(utf8.c_str());
#endif
    }


    ScriptModuleCoreCLR::ScriptModuleCoreCLR(const char* id,
        uint32_t moduleID,
        ScriptHostCoreCLR* host,
        const char* group)
        : ScriptModule(id, moduleID, host, group)
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
        std::string runtimeCfg = assemblyName + ".runtimeconfig.json";
        hostfxr_handle ctx = nullptr;
		std::wstring runtimeCfgW = ToWide(runtimeCfg);
        xg::Log(MessageType::Info, "hostfxr Initialize for runtime config [%s]", runtimeCfg.c_str());

        int rc = _coreclrHost->GetInitializeRuntimeConfigFunc()(runtimeCfgW.c_str(), nullptr, &ctx);

        if (rc != 0 || ctx == nullptr)
        {
			xg::Log(MessageType::Error, "hostfxr Initialize failed with error code: %d", rc);
            return false;
        }

        _fxrHandle = ctx;

        void* loadFn = nullptr;

        rc = _coreclrHost->GetRuntimeDelegateFunc()(
            _fxrHandle,
            hdt_load_assembly_and_get_function_pointer,
            &loadFn);
        

        if (rc != 0 || loadFn == nullptr)
        {
			xg::Log(MessageType::Error, "hostfxr GetRuntimeDelegate failed with error code: %d", rc);
            return false;
        }

        _loadAssemblyAndGetFn = (load_assembly_and_get_function_pointer_fn)loadFn;

        std::string typeName = assemblyName + "." + XG_SCRIPTENTRY_TYPE;

        if(!LoadEntryPoints(assemblyName.c_str(), typeName.c_str(), L"Init", (void**)& _managedInit)
            || !LoadEntryPoints(assemblyName.c_str(), typeName.c_str(), L"Update", (void**)& _managedUpdate)
            || !LoadEntryPoints(assemblyName.c_str(), typeName.c_str(), L"Shutdown", (void**)& _managedShutdown)
            || !LoadEntryPoints(assemblyName.c_str(), typeName.c_str(), L"OnMessage", (void**)&_managedOnMessageMethod))
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
            int result = _managedInit(engine, GetModuleId());
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
            _managedUpdate(dt);
        }
    }

    void ScriptModuleCoreCLR::Shutdown()
    {
        if (_managedShutdown)
        {
            _managedShutdown();
        }

        if (_fxrHandle )
        {
            _coreclrHost->GetCloseFunc()(_fxrHandle);
            _fxrHandle = nullptr;
        }
        _loadAssemblyAndGetFn = nullptr;
        _valid = false;
    }

    bool ScriptModuleCoreCLR::IsValid() const
    {
        return _valid;
    }
    /*void ScriptModuleCoreCLR::OnMessage(const ScriptMessage& msg)
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
    }*/
    void ScriptModuleCoreCLR::OnMessage(const ScriptMessage& msg)
    {
        if (!_managedOnMessageMethod)
            return;

        auto func = reinterpret_cast<OnMessageFunc>(_managedOnMessageMethod);

        void* payloadCopy = nullptr;
        if (msg.Payload && msg.PayloadSize > 0)
        {
            payloadCopy = XG_MANAGED_ALLOC(msg.PayloadSize);
            memcpy(payloadCopy, msg.Payload, msg.PayloadSize);
        }

        func(msg.TypeName, payloadCopy, msg.PayloadSize);

        if (payloadCopy)
            XG_MANAGED_FREE(payloadCopy);
    }
    bool ScriptModuleCoreCLR::LoadEntryPoints(const char* assemblyName, const char* typeName, const wchar_t* entryPointName, void** func)
    {
        if (!func)return true;

        if (!_loadAssemblyAndGetFn)
            return false;

		std::string assemblyNameStr(assemblyName);
        std::string asmPath = GetFullPath(assemblyName) + ".dll";
        std::wstring asmPathW = ToWide(asmPath);

        const char_t* asmPathT = ToCharT(asmPathW);               
        std::wstring typeNameW = ToWide(std::format("{}.{}, {}", assemblyNameStr, XG_SCRIPTENTRY_TYPE, assemblyNameStr));

        const char_t* typeNameT = ToCharT(typeNameW);

        std::wstring methodWide(entryPointName);
        const char_t* methodT = ToCharT(methodWide);

        int rc = -1;

        rc = _loadAssemblyAndGetFn(
            asmPathT,
            typeNameT,
            methodT,
            UNMANAGEDCALLERSONLY_METHOD,
            nullptr,
            func);

        if (rc != 0 || func == nullptr)
        {
			xg::Log(MessageType::Error, "Failed to load entry point '%ls' from assembly '%s'. Error code: %d", entryPointName, asmPath.c_str(), rc);
            return false;
        }
        return true;
    }
}