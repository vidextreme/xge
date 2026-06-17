#include "pch.h"
#include "EditorKernel.h"
#include "xgEngine.h"
#include "xgScriptHost.h"
#include "xgScriptModule.h"

namespace xg
{
    EditorKernelModule::EditorKernelModule(const char* id)
        : ScriptModule(id)
    {
    }

    EditorKernelModule::~EditorKernelModule()
    {
        Shutdown();
    }

    bool EditorKernelModule::Init(ScriptEngine* engine)
    {
        if (!engine)
            return false;

        _engine = engine;

        //
        // Ask the engine to load the managed editor module
        //
        _editorHost =
            _engine->AddScriptModule(
                "editor",
                "Editor.CoreCLR.dll");

        if (!_editorHost)
        {
            _isValid = false;
            return false;
        }

        _editorModule = _engine->GetScriptModule("editor");
        _isValid = (_editorModule != nullptr);
        return _isValid;
    }

    void EditorKernelModule::Update(float dt)
    {
        if (!_isValid || !_editorModule)
            return;

        // The managed editor module is already updated by Engine::Run()
        // via its normal ScriptModule loop. If you want editor-kernel-
        // specific native behavior, do it here.
    }

    void EditorKernelModule::Shutdown()
    {
        if (!_engine)
            return;

        if (_editorModule)
        {
            _engine->RemoveScriptModule("editor");
            _editorModule = nullptr;
        }

        _editorHost = nullptr;
        _engine = nullptr;
        _isValid = false;
    }
}

// Exported factory for xgScriptNative
XG_MODULE_EXPORT xg::ScriptModule* CreateScriptModule(const char* id)
{
    return new xg::EditorKernelModule(id);
}