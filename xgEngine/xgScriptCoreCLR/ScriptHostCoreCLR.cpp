#include "pch.h"
#include "ScriptHostCoreCLR.h"
#include "public/xgScriptModuleCoreCLR.h"

#include <windows.h>
#include <filesystem>
#include <string>

namespace
{
    std::string DirOf(const std::string& p)
    {
        size_t pos = p.find_last_of("/\\");
        return (pos == std::string::npos) ? "." : p.substr(0, pos);
    }

    std::string BuildTpaList(const std::string& runtimePath)
    {
        std::string tpa;
        for (auto& file : std::filesystem::recursive_directory_iterator(runtimePath))
        {
            if (file.path().extension() == ".dll")
            {
                tpa += file.path().string();
                tpa += ';';
            }
        }
        return tpa;
    }

    std::string GetFullPath(const char* path)
    {
        char buffer[MAX_PATH];
        DWORD len = GetFullPathNameA(path, MAX_PATH, buffer, nullptr);
        if (len == 0 || len >= MAX_PATH)
            return std::string(path);
        return std::string(buffer);
    }
}

namespace xg
{
    ScriptHostCoreCLR::ScriptHostCoreCLR()
    {
    }

    ScriptHostCoreCLR::~ScriptHostCoreCLR()
    {
        ShutdownRuntime();
    }

    ScriptModule* ScriptHostCoreCLR::LoadModule(const char* path)
    {
        // Working directory is gameroot/bin, path is likely "Editor.CoreCLR.dll"
        std::string dllPath = GetFullPath(path);   // ...\gameroot\bin\Editor.CoreCLR.dll
        std::string binDir = DirOf(dllPath);      // ...\gameroot\bin
        std::string rootDir = DirOf(binDir);       // ...\gameroot

        if (!_initialized)
        {
            if (!InitializeRuntime(rootDir.c_str()))
                return nullptr;
        }

        auto* module = new xgScriptModuleCoreCLR(this);
        if (!module->Load(path))
        {
            delete module;
            return nullptr;
        }
        return module;
    }

    bool ScriptHostCoreCLR::InitializeRuntime(const char* engineRoot)
    {
        if (_initialized)
            return true;

        // engineRoot = ...\gameroot
        std::string runtimePath = std::string(engineRoot) + "\\coreclr"; // ...\gameroot\coreclr
        std::string scriptsPath = std::string(engineRoot) + "\\bin";    // ...\gameroot\bin
        std::string tpaList = BuildTpaList(runtimePath);

        std::string coreclrPath = runtimePath + "\\coreclr.dll";
        HMODULE coreclr = LoadLibraryA(coreclrPath.c_str());
        if (!coreclr)
            return false;

        _coreclrLib = coreclr;

        _coreclrInitialize = (coreclr_initialize_fn)
            GetProcAddress(coreclr, "coreclr_initialize");
        _coreclrCreateDelegate = (coreclr_create_delegate_fn)
            GetProcAddress(coreclr, "coreclr_create_delegate");
        _coreclrShutdown = (coreclr_shutdown_fn)
            GetProcAddress(coreclr, "coreclr_shutdown");

        if (!_coreclrInitialize || !_coreclrCreateDelegate || !_coreclrShutdown)
            return false;

        const char* propertyKeys[] = {
            "TRUSTED_PLATFORM_ASSEMBLIES",
            "APP_PATHS",
            "APP_NI_PATHS"
        };

        const char* propertyValues[] = {
            tpaList.c_str(),
            scriptsPath.c_str(),
            scriptsPath.c_str()
        };

        void* hostHandle = nullptr;
        unsigned int domainId = 0;

        int hr = _coreclrInitialize(
            engineRoot,          // base path
            "xgEngineDomain",
            3,
            propertyKeys,
            propertyValues,
            &hostHandle,
            &domainId);

        if (hr < 0)
            return false;

        _hostHandle = hostHandle;
        _domainId = domainId;

        if (!GetEntryPoints(&_init, &_update, &_shutdown))
            return false;

        _initialized = true;
        return true;
    }

    bool ScriptHostCoreCLR::GetEntryPoints(void** initFn, void** updateFn, void** shutdownFn)
    {
        if (!_coreclrCreateDelegate || !_hostHandle)
            return false;

        int hr = 0;

        const char* assemblyName = "Editor.CoreCLR";
        const char* typeName = "xgEditor.ScriptEntry";

        if (initFn)
        {
            hr = _coreclrCreateDelegate(
                _hostHandle, _domainId,
                assemblyName, typeName, "Init",
                initFn);
            if (hr < 0) return false;
        }

        if (updateFn)
        {
            hr = _coreclrCreateDelegate(
                _hostHandle, _domainId,
                assemblyName, typeName, "Update",
                updateFn);
            if (hr < 0) return false;
        }

        if (shutdownFn)
        {
            hr = _coreclrCreateDelegate(
                _hostHandle, _domainId,
                assemblyName, typeName, "Shutdown",
                shutdownFn);
            if (hr < 0) return false;
        }

        return true;
    }

    void ScriptHostCoreCLR::ShutdownRuntime()
    {
        if (_coreclrShutdown && _hostHandle)
        {
            _coreclrShutdown(_hostHandle, _domainId);
            _hostHandle = nullptr;
            _domainId = 0;
        }

        if (_coreclrLib)
        {
            FreeLibrary((HMODULE)_coreclrLib);
            _coreclrLib = nullptr;
        }

        _coreclrInitialize = nullptr;
        _coreclrCreateDelegate = nullptr;
        _coreclrShutdown = nullptr;
        _initialized = false;
    }
}
