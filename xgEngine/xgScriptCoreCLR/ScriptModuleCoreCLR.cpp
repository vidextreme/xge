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
        std::string runtimeCfg = assemblyName + ".runtimeconfig.json";
        hostfxr_handle ctx = nullptr;
		std::wstring runtimeCfgW = ToWide(runtimeCfg);
        int rc = _coreclrHost->GetInitializeRuntimeConfigFunc()(runtimeCfgW.c_str(), nullptr, &ctx);
        xg::Log(MessageType::Info, "hostfxr Initialize for runtime config [%s]", runtimeCfg.c_str());


        if (rc != 0 || ctx == nullptr)
            return false;

        _fxrHandle = ctx;

        void* loadFn = nullptr;

        rc = _coreclrHost->GetRuntimeDelegateFunc()(
            _fxrHandle,
            hdt_load_assembly_and_get_function_pointer,
            &loadFn);
        

        if (rc != 0 || loadFn == nullptr)
            return false;

        _loadAssemblyAndGetFn = (load_assembly_and_get_function_pointer_fn)loadFn;

        std::string typeName = assemblyName + ".ScriptEntry";

        if(!LoadEntryPoints(assemblyName.c_str(), typeName.c_str(), L"Init", &_managedInit)
            || !LoadEntryPoints(assemblyName.c_str(), typeName.c_str(), L"Update", &_managedUpdate)
            || !LoadEntryPoints(assemblyName.c_str(), typeName.c_str(), L"Shutdown", &_managedShutdown))
        {
            return false;
        }
        /*if (!_coreclrHost->GetEntryPoints(
            assemblyName.c_str(),
            typeName.c_str(),
            &_managedInit,
            &_managedUpdate,
            &_managedShutdown))
        {
            return false;
        }*/

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
    bool ScriptModuleCoreCLR::LoadEntryPoints(const char* assemblyName, const char* typeName, const wchar_t* entryPointName, void** func)
    {
        if (!_loadAssemblyAndGetFn)
            return false;

        std::string asmPath = GetFullPath(assemblyName) + ".dll";
        std::wstring asmPathW = ToWide(asmPath);

        //OutputDebugStringA(("Resolved ASM path: " + asmPath + "\n").c_str());
        //OutputDebugStringA(("Resolved TYPE name: " + std::string(typeName ? typeName : "") + "\n").c_str());

        const char_t* asmPathT = ToCharT(asmPathW);

        std::wstring typeNameW = ToWide("xgEditor.CoreCLR.ScriptEntry, xgEditor.CoreCLR");
        const char_t* typeNameT = ToCharT(typeNameW);


        auto resolve = [&](const wchar_t* methodW, void** outPtr)
            {
                if (!outPtr)
                    return true;

                std::wstring methodWide(methodW);
                const char_t* methodT = ToCharT(methodWide);

                void* raw = nullptr;
                int rc = -1;

                rc = _loadAssemblyAndGetFn(
                    asmPathT,
                    typeNameT,
                    methodT,
                    UNMANAGEDCALLERSONLY_METHOD, // matches [UnmanagedCallersOnly]
                    nullptr,
                    &raw);

                //wchar_t buf[256];
                //swprintf_s(buf, L"load_assembly_and_get_function_pointer %s rc=0x%08X ptr=%p\n", methodW, rc, raw);
                //OutputDebugStringW(buf);

                if (rc != 0 || raw == nullptr)
                    return false;

                *outPtr = raw;
                return true;
            };
        return resolve(entryPointName, func);
    }
}