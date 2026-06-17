#pragma once

#include "xgScriptModule.h"

namespace xg
{
    class ScriptEngine;

    class EditorKernelModule : public ScriptModule
    {
    public:
        EditorKernelModule(const char* id);
        ~EditorKernelModule() override;

        bool Init(ScriptEngine* engine) override;
        void Update(float dt) override;
        void Shutdown() override;
        bool IsValid() const override { return _isValid; }

    private:
        ScriptEngine* _engine = nullptr;
        bool         _isValid = true;
        ScriptHost* _editorHost = nullptr;
        ScriptModule* _editorModule = nullptr;
    };
}
