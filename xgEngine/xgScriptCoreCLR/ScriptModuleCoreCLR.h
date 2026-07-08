#pragma once
#include "xgScriptModule.h"
#include "ScriptHostCoreCLR.h"
namespace xg
{
    class ScriptModuleCoreCLR : public ScriptModule
    {
    public:
        ScriptModuleCoreCLR(const char* id,
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
        ScriptHostCoreCLR* _coreclrHost = nullptr;   // typed host

        // hostfxr runtime context
        hostfxr_handle _fxrHandle = nullptr;

        // load_assembly_and_get_function_pointer delegate (stdcall, char_t*)
        load_assembly_and_get_function_pointer_fn _loadAssemblyAndGetFn = nullptr;

        void* _managedInit = nullptr;
        void* _managedUpdate = nullptr;
        void* _managedShutdown = nullptr;

        bool _valid = false;
    };
}
