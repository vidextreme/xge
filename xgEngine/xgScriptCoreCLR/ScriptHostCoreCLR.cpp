#include "pch.h"
#include "ScriptHostCoreCLR.h"
#include "ScriptModuleCoreCLR.h"
#include "xgScriptEngine.h"
#include "xgCodecRegistry.h"
#include "xgModules.h"

#include <string>
#include <filesystem>
#include <windows.h>

namespace
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

    LONG CALLBACK CoreClrVectoredHandler(PEXCEPTION_POINTERS ep)
    {
        wchar_t buf[256];
        swprintf_s(buf, L"[VEH] CLR exception code=0x%08X\n", ep->ExceptionRecord->ExceptionCode);
        OutputDebugStringW(buf);
        return EXCEPTION_CONTINUE_SEARCH;
    }

    // coreclr_delegates.h uses char_t; on Windows that’s wchar_t/unsigned short.
    const char_t* ToCharT(const std::wstring& w)
    {
#if defined(_WIN32)
        return reinterpret_cast<const char_t*>(w.c_str());
#else
        return reinterpret_cast<const char_t*>(w.c_str());
#endif
    }
}

namespace xg
{
    ScriptHostCoreCLR::ScriptHostCoreCLR(ScriptEngine* engine)
        : _engine(engine)
    {}

    ScriptHostCoreCLR::~ScriptHostCoreCLR()
    {
        ShutdownRuntime();
    }

    bool ScriptHostCoreCLR::InitializeRuntime(const char* engineRoot)
    {
        std::string root(engineRoot ? engineRoot : "");
        std::string hostfxrPath = root + "\\coreclr\\host\\fxr\\10.0.8\\hostfxr.dll";
        std::string runtimeCfg = root + "\\bin\\xgEditor.CoreCLR.runtimeconfig.json";

        _hostfxrLib = xg::LoadModule(hostfxrPath.c_str());
        if (!_hostfxrLib)
        {
            //OutputDebugStringA("Failed to load hostfxr.dll\n");
            xg::Log(MessageType::Error, "Failed to load hostfxe.dll [%s]", hostfxrPath.c_str());
            return false;
        }

        _hostfxrInitializeForRuntimeConfig =
            (hostfxr_initialize_for_runtime_config_fn)xg::GetSymbol(_hostfxrLib, "hostfxr_initialize_for_runtime_config");
        _hostfxrGetRuntimeDelegate =
            (hostfxr_get_runtime_delegate_fn)xg::GetSymbol(_hostfxrLib, "hostfxr_get_runtime_delegate");
        _hostfxrClose =
            (hostfxr_close_fn)xg::GetSymbol(_hostfxrLib, "hostfxr_close");

        if (!_hostfxrInitializeForRuntimeConfig ||
            !_hostfxrGetRuntimeDelegate ||
            !_hostfxrClose)
        {
            OutputDebugStringA("Failed to resolve hostfxr exports\n");
            return false;
        }

        std::wstring runtimeCfgW = ToWide(runtimeCfg);
        hostfxr_handle ctx = nullptr;

        int rc = _hostfxrInitializeForRuntimeConfig(runtimeCfgW.c_str(), nullptr, &ctx);
        wchar_t buf[256];
        swprintf_s(buf, L"hostfxr_initialize_for_runtime_config rc=0x%08X ctx=%p\n", rc, ctx);
        OutputDebugStringW(buf);

        if (rc != 0 || ctx == nullptr)
            return false;

        _fxrHandle = ctx;

        void* loadFn = nullptr;

        rc = _hostfxrGetRuntimeDelegate(
            _fxrHandle,
            hdt_load_assembly_and_get_function_pointer,
            &loadFn);

        swprintf_s(buf, L"hostfxr_get_runtime_delegate rc=0x%08X ptr=%p\n", rc, loadFn);
        OutputDebugStringW(buf);

        if (rc != 0 || loadFn == nullptr)
            return false;

        _loadAssemblyAndGetFn = (load_assembly_and_get_function_pointer_fn)loadFn;
        _initialized = true;
        return true;
    }

    ScriptModule* ScriptHostCoreCLR::LoadModule(const char* id,
        const char* path,
        const char* group)
    {
        std::string dllPath = GetFullPath(path);
        std::filesystem::path dll(dllPath);
        std::string binDir = dll.parent_path().string();
        std::string rootDir = std::filesystem::path(binDir).parent_path().string();

        if (!_initialized)
        {
            if (!InitializeRuntime(rootDir.c_str()))
                return nullptr;
        }

        auto* module = new ScriptModuleCoreCLR(id, this, group);
        if (!module->Load(path))
        {
            delete module;
            return nullptr;
        }

        XG_ADDREF(this);
        return module;
    }

    bool ScriptHostCoreCLR::GetEntryPoints(const char* assemblyName,
        const char* typeName,
        void** initFn,
        void** updateFn,
        void** shutdownFn)
    {
        if (!_initialized || !_loadAssemblyAndGetFn)
            return false;

        std::string asmPath = GetFullPath(assemblyName) + ".dll";
        std::wstring asmPathW = ToWide(asmPath);
        //std::wstring typeNameW = ToWide(typeName ? typeName : "");

        OutputDebugStringA(("Resolved ASM path: " + asmPath + "\n").c_str());
        OutputDebugStringA(("Resolved TYPE name: " + std::string(typeName ? typeName : "") + "\n").c_str());

        const char_t* asmPathT = ToCharT(asmPathW);
        //const char_t* typeNameT = ToCharT(typeNameW);

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

                PVOID veh = AddVectoredExceptionHandler(1, CoreClrVectoredHandler);

                rc = _loadAssemblyAndGetFn(
                    asmPathT,
                    typeNameT,
                    methodT,
                    UNMANAGEDCALLERSONLY_METHOD, // matches [UnmanagedCallersOnly]
                    nullptr,
                    &raw);

                if (veh)
                    RemoveVectoredExceptionHandler(veh);

                wchar_t buf[256];
                swprintf_s(buf, L"load_assembly_and_get_function_pointer %s rc=0x%08X ptr=%p\n", methodW, rc, raw);
                OutputDebugStringW(buf);

                if (rc != 0 || raw == nullptr)
                    return false;

                *outPtr = raw;
                return true;
            };

        if (!resolve(L"Init", initFn)) return false;
        if (!resolve(L"Update", updateFn)) return false;
        if (!resolve(L"Shutdown", shutdownFn)) return false;

        return true;
    }

    ScriptEngine* ScriptHostCoreCLR::GetEngine() const
    {
        return _engine;
    }

    CodecRegistry* ScriptHostCoreCLR::GetCodecRegistry() const
    {
        return _engine->GetCodecRegistry(ScriptBackendType::CoreCLR);
    }

    PayloadMode ScriptHostCoreCLR::GetPayloadMode() const
    {
        return _engine->GetPayloadMode();
    }

    bool ScriptHostCoreCLR::Encode(const void*,
        const TypeSchema*,
        ScriptMessage&)
    {
        return false;
    }

    bool ScriptHostCoreCLR::Decode(const ScriptMessage&,
        const TypeSchema*,
        void*)
    {
        return false;
    }

    void ScriptHostCoreCLR::ShutdownRuntime()
    {
        if (_fxrHandle && _hostfxrClose)
        {
            _hostfxrClose(_fxrHandle);
            _fxrHandle = nullptr;
        }

        if (_hostfxrLib)
        {
            xg::UnloadModule(_hostfxrLib);
            _hostfxrLib = nullptr;
        }

        _hostfxrInitializeForRuntimeConfig = nullptr;
        _hostfxrGetRuntimeDelegate = nullptr;
        _hostfxrClose = nullptr;
        _loadAssemblyAndGetFn = nullptr;
        _initialized = false;
    }
}
