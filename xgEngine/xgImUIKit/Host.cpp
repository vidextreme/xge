// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.
#include "xgImUI.h"
#include "ImUIInternal.h"
#include "ImUIHost.h"
#include "backends/UIPlatformBackend.h"
#include "backends/UIRenderBackend.h"
#include "xgEvent.h"
#include "xgLog.h"
#include "xgUIDraw.h"
#include "xgWindow.h"
#include "misc/freetype/imgui_freetype.h"

#include <cstddef>
#include <cstdio>
#include <string>

namespace
{
    struct HostState
    {
        xg::Window* Window = nullptr;
        xg::UIRenderTarget* Target = nullptr;
        xg::imui::HostFlags Flags = xg::imui::HostFlags::None;
        bool Ready = false;
        bool Viewports = false;
        std::string LayoutFile;
    };

    HostState _hostState;

    bool PrototypeShell()
    {
        return HasFlag(_hostState.Flags, xg::imui::HostFlags::PrototypeShell);
    }

    bool FileExists(const std::string& path)
    {
        FILE* file = nullptr;
        if (fopen_s(&file, path.c_str(), "rb") != 0 || !file)
            return false;
        std::fclose(file);
        return true;
    }

    ImFont* LoadFont(const std::string& directory, const char* file, const char* fallback, float size)
    {
        ImFontConfig config;
        config.FontLoaderFlags = ImGuiFreeTypeLoaderFlags_LightHinting;
        ImFontAtlas* atlas = ImGui::GetIO().Fonts;
        const std::string path = directory + "/" + file;
        if (FileExists(path))
            return atlas->AddFontFromFileTTF(path.c_str(), size, &config);

        const std::string fallbackPath = directory + "/" + fallback;
        if (FileExists(fallbackPath))
        {
            xg::Log(xg::MessageType::Warning, "xgImUIKit: font %s not found, using %s", path.c_str(), fallback);
            return atlas->AddFontFromFileTTF(fallbackPath.c_str(), size, &config);
        }

        xg::Log(xg::MessageType::Warning, "xgImUIKit: font %s not found, using the built-in font", path.c_str());
        config.SizePixels = size;
        return atlas->AddFontDefault(&config);
    }

    void ReleaseAppIcon()
    {
        if (!xg::imui::detail::_appIcon)
            return;
        ImGui::UnregisterUserTexture(xg::imui::detail::_appIcon);
        IM_DELETE(xg::imui::detail::_appIcon);
        xg::imui::detail::_appIcon = nullptr;
    }
}

namespace xg::imui::detail
{
    UIRenderTarget* HostRenderTarget()
    {
        return _hostState.Target;
    }

    bool HostPrototypeShell()
    {
        return PrototypeShell();
    }
}

namespace xg
{
    XG_MODULE_EXPORT uint32_t XG_ImUIGetApiVersion()
    {
        return imui::ApiVersion;
    }

    XG_MODULE_EXPORT bool XG_ImUIHostInit(const imui::HostDesc* desc)
    {
        if (_hostState.Ready || !desc || desc->Size < offsetof(imui::HostDesc, Platform) || !desc->MainWindow || !desc->RenderTarget
            || !desc->FontDirectory)
            return false;

        _hostState.Window = desc->MainWindow;
        _hostState.Target = desc->RenderTarget;
        _hostState.Flags = desc->Flags;
        PlatformServices* services = desc->Size >= offsetof(imui::HostDesc, Platform) + sizeof(desc->Platform) ? desc->Platform : nullptr;
        const bool hasLayoutFile = desc->Size >= offsetof(imui::HostDesc, LayoutFile) + sizeof(desc->LayoutFile);
        _hostState.LayoutFile = hasLayoutFile && desc->LayoutFile ? desc->LayoutFile : "imgui.ini";
        _hostState.Viewports = HasFlag(desc->Flags, imui::HostFlags::MultiViewport) && services && desc->RenderTarget->SupportsSurfaces();

        IMGUI_CHECKVERSION();
        ImGui::CreateContext();

        ImGuiIO& io = ImGui::GetIO();
        io.IniFilename = _hostState.LayoutFile.empty() ? nullptr : _hostState.LayoutFile.c_str();
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
        io.ConfigErrorRecoveryEnableAssert = false;
        if (_hostState.Viewports)
        {
            io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
            io.ConfigViewportsNoTaskBarIcon = true;
        }

        const std::string fonts = desc->FontDirectory;
        LoadFont(fonts, "NotoSans-Regular.ttf", "DejaVuSans.ttf", 15.0f);
        LoadFont(fonts, "NotoSans-Regular.ttf", "DejaVuSans.ttf", 14.0f);
        LoadFont(fonts, "NotoSans-Bold.ttf", "DejaVuSans-Bold.ttf", 14.0f);
        LoadFont(fonts, "NotoSansMono-Regular.ttf", "DejaVuSans.ttf", 14.0f);

        if (PrototypeShell())
        {
            imui::detail::ApplyTheme();
            _hostState.Window->SetCustomFrame(true);
        }
        else
        {
            ImGui::StyleColorsDark();
        }

        if (!imui::detail::platform::Init(_hostState.Window, _hostState.Viewports ? services : nullptr))
        {
            ImGui::DestroyContext();
            _hostState = {};
            return false;
        }
        ImGui::GetStyle().ScaleAllSizes(imui::detail::platform::DisplayScale());

        io.BackendRendererName = "xg::UIRenderTarget";
        io.BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset | ImGuiBackendFlags_RendererHasTextures;
        if (_hostState.Viewports)
            imui::detail::render::InitViewports(*_hostState.Target);
        _hostState.Target->SetClearColor(Color(13.0f / 255.0f, 13.0f / 255.0f, 13.0f / 255.0f, 1.0f));
        _hostState.Ready = true;
        return true;
    }

    XG_MODULE_EXPORT void XG_ImUIHostEvent(const xgEvent* e)
    {
        if (!_hostState.Ready || !e)
            return;
        imui::detail::platform::ProcessEvent(*e);
    }

    XG_MODULE_EXPORT void XG_ImUIHostFrame(float deltaTime)
    {
        if (!_hostState.Ready)
            return;

        imui::detail::platform::NewFrame(deltaTime);
        ImGui::NewFrame();

        if (PrototypeShell())
        {
            imui::detail::DrawMainDockspace(_hostState.Window);
            imui::detail::DrawPrototypePanels();
        }
        else
        {
            imui::detail::SetDefaultDockSpace(ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport(), ImGuiDockNodeFlags_PassthruCentralNode));
        }
        imui::detail::DrawRegisteredPanels();

        ImGui::Render();
        ImDrawData* drawData = ImGui::GetDrawData();
        imui::detail::render::ProcessTextures(*_hostState.Target, drawData);
        imui::detail::render::SubmitDrawData(*_hostState.Target, drawData);

        if (_hostState.Viewports)
        {
            ImGui::UpdatePlatformWindows();
            ImGui::RenderPlatformWindowsDefault();
            imui::detail::platform::ShowPendingWindows();
        }
    }

    XG_MODULE_EXPORT void XG_ImUIHostSetAppIcon(const uint8_t* rgba, int width, int height)
    {
        if (!_hostState.Ready || !rgba || width <= 0 || height <= 0)
            return;
        _hostState.Window->SetIconRGBA(rgba, width, height);
        if (PrototypeShell())
        {
            ReleaseAppIcon();
            imui::detail::_appIcon = imui::detail::CreateIconTexture(rgba, width, height, imui::detail::AppIconSize);
        }
    }

    XG_MODULE_EXPORT void XG_ImUIHostShutdown()
    {
        if (!_hostState.Ready)
            return;
        _hostState.Ready = false;

        if (_hostState.Viewports)
        {
            ImGui::DestroyPlatformWindows();
            imui::detail::render::ShutdownViewports();
        }
        ReleaseAppIcon();
        imui::detail::ReleaseUserTextures();
        imui::detail::render::DestroyAllTextures(*_hostState.Target);
        _hostState.Target->SubmitUI(nullptr);
        imui::detail::platform::Shutdown();
        ImGui::DestroyContext();
        _hostState = {};
    }
}
