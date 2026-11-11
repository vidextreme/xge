// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.
#include "pch.h"
#include <memory>
#include <unordered_map>
#include "xgUIHostSession.h"
#include "xgEngine.h"
#include "xgListenerBinding.h"
#include "xgImage.h"
#include "xgImUI.h"
#include "xgPlatformServices.h"

namespace xg
{
    UIHostSession::UIHostSession() = default;

    UIHostSession::~UIHostSession()
    {
        Stop();
    }

    bool UIHostSession::Start(Engine& engine, const UIHostOptions& options)
    {
        if (_ready)
            return false;
        Window* window = engine.MainWindow.get();
        if (!window)
        {
            xg::Log(xg::MessageType::Error, "UIHost: no main window, UI disabled");
            return false;
        }

        if (engine.Renderer.valid())
        {
            _target = engine.Renderer.get();
        }
        else
        {
            _ownedTarget = xg::NewUIRenderTarget(*window);
            _target = _ownedTarget.get();
        }
        if (!_target)
        {
            xg::Log(xg::MessageType::Error, "UIHost: no UI render target available, UI disabled");
            return false;
        }

        imui::HostDesc desc = {};
        desc.Size = sizeof(desc);
        desc.MainWindow = window;
        desc.RenderTarget = _target;
        desc.Flags = (options.PrototypeShell ? imui::HostFlags::PrototypeShell : imui::HostFlags::None)
            | (options.MultiViewport ? imui::HostFlags::MultiViewport : imui::HostFlags::None);
        desc.Platform = GetPlatformServices();
        desc.FontDirectory = options.FontDirectory;
        desc.LayoutFile = options.LayoutFile;
        if (XG_ImUIGetApiVersion() < imui::ApiVersion || !XG_ImUIHostInit(&desc))
        {
            xg::Log(xg::MessageType::Error, "UIHost: xgImUIKit initialization failed, UI disabled");
            _target = nullptr;
            _ownedTarget.reset();
            return false;
        }

        if (options.IconPath)
        {
            ImageRGBA icon = {};
            if (LoadImageRGBA(options.IconPath, &icon))
            {
                XG_ImUIHostSetAppIcon(icon.Pixels, icon.Width, icon.Height);
                FreeImageRGBA(&icon);
            }
        }

        _engine = &engine;
        _listener = std::make_unique<ListenerBinding<UIHostSession>>(this, &UIHostSession::OnEvent);
        if (auto* dispatcher = engine.GetSubsystem<EventDispatcher>())
            dispatcher->AddListener(_listener.get());
        _ready = true;
        return true;
    }

    void UIHostSession::Frame(float dt)
    {
        if (_ready)
            XG_ImUIHostFrame(dt);
    }

    void UIHostSession::Stop()
    {
        if (!_ready)
            return;
        _ready = false;
        if (auto* dispatcher = _engine->GetSubsystem<EventDispatcher>())
            dispatcher->RemoveListener(_listener.get());
        _listener.reset();
        XG_ImUIHostShutdown();
        _target = nullptr;
        _ownedTarget.reset();
        _engine = nullptr;
    }

    void UIHostSession::OnEvent(const xgEvent& e)
    {
        if (_ready)
            XG_ImUIHostEvent(&e);
    }
}
