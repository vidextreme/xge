// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.
#include "pch.h"
#include "xgEngine.h"
#include "xgUIHostSession.h"
#include "xgImage.h"
#include "xgDialogs.h"

namespace xg
{
    namespace
    {
        constexpr uint32_t UIHostPrototypeShell = 1u << 0;
        constexpr uint32_t UIHostMultiViewport = 1u << 1;
        constexpr uint32_t UIHostNoLayoutFile = 1u << 2;

        std::unique_ptr<UIHostSession> _uiHostSession;
        ScriptEngine* _uiHostEngine = nullptr;

        Window* MainWindowOf(ScriptEngine* engine)
        {
            return engine ? static_cast<Engine*>(engine)->MainWindow.get() : nullptr;
        }
    }
}

// Starts the xgImUIKit host on the engine's main window for a module that runs without the editor kernel.
// flags: bit 0 = prototype editor shell, bit 1 = multi-viewport, bit 2 = don't load or save imgui.ini. Returns false when a host is already running.
XG_MODULE_EXPORT bool XG_UIHostStart(xg::ScriptEngine* engine, uint32_t callerModuleId, uint32_t flags)
{
    if (!engine || xg::_uiHostSession)
        return false;
    xg::SetLogger(engine->GetLogger());
    xg::UIHostOptions options;
    options.PrototypeShell = (flags & xg::UIHostPrototypeShell) != 0;
    options.MultiViewport = (flags & xg::UIHostMultiViewport) != 0;
    if (flags & xg::UIHostNoLayoutFile)
        options.LayoutFile = "";
    auto session = std::make_unique<xg::UIHostSession>();
    if (!session->Start(*static_cast<xg::Engine*>(engine), options))
        return false;
    xg::_uiHostSession = std::move(session);
    xg::_uiHostEngine = engine;
    xg::Log(xg::MessageType::Info, "UI host started by module %u", callerModuleId);
    return true;
}

XG_MODULE_EXPORT void XG_UIHostFrame(xg::ScriptEngine* engine, float dt)
{
    if (xg::_uiHostSession && engine == xg::_uiHostEngine)
        xg::_uiHostSession->Frame(dt);
}

XG_MODULE_EXPORT void XG_UIHostStop(xg::ScriptEngine* engine)
{
    if (!xg::_uiHostSession || engine != xg::_uiHostEngine)
        return;
    xg::_uiHostSession->Stop();
    xg::_uiHostSession.reset();
    xg::_uiHostEngine = nullptr;
}

// Asks the main window to close; Engine::Run then leaves its loop and shuts down normally.
XG_MODULE_EXPORT void XG_RequestQuit(xg::ScriptEngine* engine)
{
    if (xg::Window* window = xg::MainWindowOf(engine))
        window->RequestClose();
}

// Loads a BMP or PNG as tightly packed RGBA8. Free the result with XG_FreeImageRGBA.
XG_MODULE_EXPORT uint8_t* XG_LoadImageRGBA(const char* path, int* width, int* height)
{
    xg::ImageRGBA image = {};
    if (!xg::LoadImageRGBA(path, &image))
        return nullptr;
    if (width)
        *width = image.Width;
    if (height)
        *height = image.Height;
    return image.Pixels;
}

XG_MODULE_EXPORT void XG_FreeImageRGBA(uint8_t* pixels)
{
    xg::ImageRGBA image = { 0, 0, pixels };
    xg::FreeImageRGBA(&image);
}

XG_MODULE_EXPORT void XG_ShowOpenFolderDialog(xg::ScriptEngine* engine, const char* defaultLocation,
    xg::DialogCallback callback, void* user)
{
    xg::ShowOpenFolderDialog(xg::MainWindowOf(engine), defaultLocation, callback, user);
}

XG_MODULE_EXPORT void XG_ShowOpenFileDialog(xg::ScriptEngine* engine, const char* filterName, const char* filterPattern,
    const char* defaultLocation, xg::DialogCallback callback, void* user)
{
    xg::ShowOpenFileDialog(xg::MainWindowOf(engine), filterName, filterPattern, defaultLocation, callback, user);
}
