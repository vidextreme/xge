#pragma once
#include "xgScriptModule.h"
#include "ScriptHostCoreCLR.h"
namespace xg
{
    class ScriptModuleCoreCLR : public ScriptModule
    {
    public:
        ScriptModuleCoreCLR(const char* id,
            uint32_t moduleID,
            ScriptHostCoreCLR* host,
            const char* group);

        ~ScriptModuleCoreCLR() override;

        bool Load(const char* path);
        bool Init(ScriptEngine* engine) override;
        void Update(float dt) override;
        void Shutdown() override;
        bool IsValid() const override;

        void OnMessage(const ScriptMessage& msg) override;
    private:
        bool LoadEntryPoints(const char* assemblyName,
            const char* typeName,
            const wchar_t* entryPointName,
            void** func);

        ScriptHostCoreCLR* _coreclrHost = nullptr;

        // hostfxr runtime context
        hostfxr_handle _fxrHandle = nullptr;

        // load_assembly_and_get_function_pointer delegate (stdcall, char_t*)
        load_assembly_and_get_function_pointer_fn _loadAssemblyAndGetFn = nullptr;

        InitFunc _managedInit = nullptr;
        UpdateFunc _managedUpdate = nullptr;
        ShutdownFunc _managedShutdown = nullptr;
        OnMessageFunc _managedOnMessageMethod = nullptr;


        bool _valid = false;
    };
}
