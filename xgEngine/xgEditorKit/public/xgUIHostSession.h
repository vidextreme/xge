// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.
#pragma once
#include <memory>
#include "xgEvent.h"
#include "xgUIDraw.h"

namespace xg
{
    class Engine;
    class EventListener;

    struct UIHostOptions
    {
        bool PrototypeShell = false;
        bool MultiViewport = false;
        const char* FontDirectory = "../editor/fonts";
        const char* IconPath = "../editor/xge-icon.bmp";
        // Window layout file; null = "imgui.ini", "" = no layout file.
        const char* LayoutFile = nullptr;
    };

    // Runs the xgImUIKit host on the engine's main window: picks the render target (the engine renderer, or the
    // xgPlatform UI presenter when no renderer module is loaded), forwards platform events and drives frames.
    class UIHostSession
    {
    public:
        UIHostSession();
        ~UIHostSession();
        UIHostSession(const UIHostSession&) = delete;
        UIHostSession& operator=(const UIHostSession&) = delete;

        bool Start(Engine& engine, const UIHostOptions& options);
        void Frame(float dt);
        void Stop();

        bool IsReady() const { return _ready; }
        UIRenderTarget* GetRenderTarget() const { return _target; }

    private:
        void OnEvent(const xgEvent& e);

        Engine* _engine = nullptr;
        UIRenderTargetUnique _ownedTarget;
        UIRenderTarget* _target = nullptr;
        std::unique_ptr<EventListener> _listener;
        bool _ready = false;
    };
}
