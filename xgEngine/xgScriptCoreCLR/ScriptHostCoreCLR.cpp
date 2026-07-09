// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.
#include "pch.h"
#include "ScriptHostCoreCLR.h"
#include "ScriptModuleCoreCLR.h"
#include "xgScriptEngine.h"
#include "xgCodecRegistry.h"
#include "xgModules.h"

#include <string>
#include <filesystem>


#define XG_CORECLR_HOSTFXR_PATH "\\coreclr\\host\\fxr\\10.0.8\\hostfxr.dll"

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


    bool LoadAndResolveHostfxrExports(const char* engineRoot, 
		ModuleHandle& hostfxrLib,
        hostfxr_initialize_for_runtime_config_fn& initForConfig, 
        hostfxr_get_runtime_delegate_fn& getRuntimeDelegate,
        hostfxr_close_fn& close)
    {
        std::string root(engineRoot ? engineRoot : "");
        std::string hostfxrPath = root + XG_CORECLR_HOSTFXR_PATH;

        hostfxrLib = xg::LoadModule(hostfxrPath.c_str());
        if (!hostfxrLib)
        {
            xg::Log(MessageType::Error, "Failed to load hostfxe.dll [%s]", hostfxrPath.c_str());
            return false;
        }

        initForConfig = (hostfxr_initialize_for_runtime_config_fn)xg::GetSymbol(hostfxrLib, "hostfxr_initialize_for_runtime_config");
        getRuntimeDelegate = (hostfxr_get_runtime_delegate_fn)xg::GetSymbol(hostfxrLib, "hostfxr_get_runtime_delegate");
        close = (hostfxr_close_fn)xg::GetSymbol(hostfxrLib, "hostfxr_close");

        if (!initForConfig ||
            !getRuntimeDelegate ||
            !close)
        {
            xg::Log(MessageType::Error, "Failed to resolve hostfxr exports.");
            return false;
        }
    }

    bool ScriptHostCoreCLR::InitializeRuntime(const char* engineRoot)
    {
		bool result = LoadAndResolveHostfxrExports(engineRoot, _hostfxrLib, _hostfxrInitializeForRuntimeConfig, _hostfxrGetRuntimeDelegate, _hostfxrClose);
		if (!result)
			return false;

        _initialized = true;
        return true;
    }

    ScriptModule* ScriptHostCoreCLR::LoadModule(const char* id,
        uint32_t moduleID,
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

        auto* module = new ScriptModuleCoreCLR(id, moduleID, this, group);
        if (!module->Load(path))
        {
            delete module;
            return nullptr;
        }

        XG_ADDREF(this);
        return module;
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
       

        if (_hostfxrLib)
        {
            xg::UnloadModule(_hostfxrLib);
            _hostfxrLib = nullptr;
        }

        _hostfxrInitializeForRuntimeConfig = nullptr;
        _hostfxrGetRuntimeDelegate = nullptr;
        _hostfxrClose = nullptr;
        _initialized = false;
    }

}
