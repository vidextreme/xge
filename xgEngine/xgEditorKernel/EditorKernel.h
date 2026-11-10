// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.
#pragma once
#include "xgScriptModule.h"
#include "xgEvent.h"
#include "xgUIDraw.h"

#include <memory>   // allowed (non‑public header)

namespace xg
{
    class ScriptEngine;
    class EventListener;
    class NativeTestWindow;
    class NativeFloatingWindow;
    class SceneViewWindow;

    XG_SERIALIZABLE()
        struct PlayerState
    {
        XG_FIELD()
            int health;
        XG_FIELD()
            float x;
        XG_FIELD()
            float y;
    };

    class EditorKernelModule : public ScriptModule
    {
    public:
        EditorKernelModule(const char* id, int moduleID, ScriptHost* host, const char* group);
        ~EditorKernelModule() override;

        bool Init(ScriptEngine* engine) override;
        void Update(float dt) override;
        void Shutdown() override;

        bool IsValid() const override { return _isValid; }

        SceneGraph* GetSceneGraph() override;
    private:
        // Editor UI lifecycle (xgImUIKit host)
        void InitUI();
        void ShutdownUI();

        // Platform event handler (called by EventDispatcher)
        void OnEvent(const xgEvent& e);
        void OnMessage(const ScriptMessage& msg) override;
    private:
        ScriptEngine* _engine = nullptr;
        ScriptModule* _editorModule = nullptr;
        ScriptModule* _gameKernel = nullptr;
        UIRenderTargetUnique _ownedUITarget;
        UIRenderTarget* _uiTarget = nullptr;

        bool _isValid = true;
        bool _prototypeUI = true;
        bool _multiViewport = true;
        bool _uiReady = false;

        // Modernized: EditorKernel owns its listener
        std::unique_ptr<EventListener> _eventListener;
        std::unique_ptr<NativeTestWindow> _nativeTestWindow;
        std::unique_ptr<NativeFloatingWindow> _nativeFloatingWindow;
        std::unique_ptr<SceneViewWindow> _sceneViewWindows[2];
    };
}
