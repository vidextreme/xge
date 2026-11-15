// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.
#include "pch.h"
#include "xgSceneGraph.h"
#include "ScriptModuleCoreCLR.h"
#include "ScriptHostCoreCLR.h"
#include "HostString.h"
#include <filesystem>
#include "xgDynamicObject.h"
#include "xgCodecRegistry.h"


#define XG_SCRIPTENTRY_TYPE "ScriptEntry"

namespace xg
{
    using coreclr::GetFullPath;
    using coreclr::ToHostString;

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
        const std::filesystem::path assemblyFile = coreclr::PathFromUtf8(GetFullPath(path));
        std::string assemblyName = coreclr::PathToUtf8(assemblyFile.stem());
        std::string assemblyPath = coreclr::PathToUtf8(assemblyFile);
        std::string runtimeCfg = coreclr::PathToUtf8(assemblyFile.parent_path() / coreclr::PathFromUtf8(assemblyName + ".runtimeconfig.json"));
        hostfxr_handle ctx = nullptr;
        const coreclr::HostString runtimeCfgHost = ToHostString(runtimeCfg);
        xg::Log(MessageType::Info, "hostfxr Initialize for runtime config [%s]", runtimeCfg.c_str());

        int rc = _coreclrHost->GetInitializeRuntimeConfigFunc()(runtimeCfgHost.c_str(), nullptr, &ctx);

        if (rc < 0 || ctx == nullptr)
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

        if(!LoadEntryPoints(assemblyName.c_str(), assemblyPath.c_str(), typeName.c_str(), "Init", (void**)& _managedInit)
            || !LoadEntryPoints(assemblyName.c_str(), assemblyPath.c_str(), typeName.c_str(), "Update", (void**)& _managedUpdate)
            || !LoadEntryPoints(assemblyName.c_str(), assemblyPath.c_str(), typeName.c_str(), "Shutdown", (void**)& _managedShutdown)
            || !LoadEntryPoints(assemblyName.c_str(), assemblyPath.c_str(), typeName.c_str(), "OnMessage", (void**)&_managedOnMessageMethod))
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
            auto shutdown = _managedShutdown;
            _managedShutdown = nullptr;
            shutdown();
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
        if (!_managedOnMessageMethod)
            return;

        void* payloadCopy = nullptr;
        if (msg.Payload && msg.PayloadSize > 0)
        {
            payloadCopy = XG_MANAGED_ALLOC(msg.PayloadSize);
            memcpy(payloadCopy, msg.Payload, msg.PayloadSize);
        }

        _managedOnMessageMethod(msg.TypeName, payloadCopy, msg.PayloadSize);

        if (payloadCopy)
            XG_MANAGED_FREE(payloadCopy);
    }
    
    SceneGraph* ScriptModuleCoreCLR::GetSceneGraph()
    {
        return _engine->GetSubsystem<SceneGraph>(this);
    }

    bool ScriptModuleCoreCLR::LoadEntryPoints(const char* assemblyName, const char* assemblyPath, const char* typeName, const char* entryPointName, void** func)
    {
        if (!func)return true;

        if (!_loadAssemblyAndGetFn)
            return false;

		std::string assemblyNameStr(assemblyName);
        std::string asmPath = assemblyPath;
        const coreclr::HostString asmPathHost = ToHostString(asmPath);
        const coreclr::HostString typeNameHost = ToHostString(std::format("{}.{}, {}", assemblyNameStr, XG_SCRIPTENTRY_TYPE, assemblyNameStr));
        const coreclr::HostString methodHost = ToHostString(entryPointName);

        int rc = -1;

        rc = _loadAssemblyAndGetFn(
            asmPathHost.c_str(),
            typeNameHost.c_str(),
            methodHost.c_str(),
            UNMANAGEDCALLERSONLY_METHOD,
            nullptr,
            func);

        if (rc != 0 || func == nullptr)
        {
			xg::Log(MessageType::Error, "Failed to load entry point '%s' from assembly '%s'. Error code: %d", entryPointName, asmPath.c_str(), rc);
            return false;
        }
        return true;
    }
}