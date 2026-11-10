// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.
#include "xgImUI.h"
#include "ImUIInternal.h"
#include "ImUIHost.h"
#include "backends/UIPlatformBackend.h"
#include "backends/UIRenderBackend.h"
#include "xgEvent.h"
#include "xgUIDraw.h"
#include "xgWindow.h"

#include <cstddef>
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
    };

    HostState s_hostState;

    bool PrototypeShell()
    {
        return HasFlag(s_hostState.Flags, xg::imui::HostFlags::PrototypeShell);
    }

    void ReleaseAppIcon()
    {
        if (!xg::imui::detail::s_appIcon)
            return;
        ImGui::UnregisterUserTexture(xg::imui::detail::s_appIcon);
        IM_DELETE(xg::imui::detail::s_appIcon);
        xg::imui::detail::s_appIcon = nullptr;
    }
}

namespace xg::imui::detail
{
    UIRenderTarget* HostRenderTarget()
    {
        return s_hostState.Target;
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
        if (s_hostState.Ready || !desc || desc->Size < offsetof(imui::HostDesc, Platform) || !desc->MainWindow || !desc->RenderTarget
            || !desc->FontDirectory)
            return false;

        s_hostState.Window = desc->MainWindow;
        s_hostState.Target = desc->RenderTarget;
        s_hostState.Flags = desc->Flags;
        PlatformServices* services = desc->Size >= sizeof(imui::HostDesc) ? desc->Platform : nullptr;
        s_hostState.Viewports = HasFlag(desc->Flags, imui::HostFlags::MultiViewport) && services && desc->RenderTarget->SupportsSurfaces();

        IMGUI_CHECKVERSION();
        ImGui::CreateContext();

        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
        io.ConfigErrorRecoveryEnableAssert = false;
        if (s_hostState.Viewports)
        {
            io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
            io.ConfigViewportsNoTaskBarIcon = true;
        }

        const std::string fonts = desc->FontDirectory;
        io.Fonts->AddFontFromFileTTF((fonts + "/DejaVuSans.ttf").c_str(), 14.0f);
        io.Fonts->AddFontFromFileTTF((fonts + "/DejaVuSans.ttf").c_str(), 13.0f);
        io.Fonts->AddFontFromFileTTF((fonts + "/DejaVuSans-Bold.ttf").c_str(), 13.0f);

        if (PrototypeShell())
        {
            imui::detail::ApplyTheme();
            s_hostState.Window->SetCustomFrame(true);
        }
        else
        {
            ImGui::StyleColorsDark();
        }

        if (!imui::detail::platform::Init(s_hostState.Window, s_hostState.Viewports ? services : nullptr))
        {
            ImGui::DestroyContext();
            s_hostState = {};
            return false;
        }

        io.BackendRendererName = "xg::UIRenderTarget";
        io.BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset | ImGuiBackendFlags_RendererHasTextures;
        if (s_hostState.Viewports)
            imui::detail::render::InitViewports(*s_hostState.Target);
        s_hostState.Target->SetClearColor(Color(13.0f / 255.0f, 13.0f / 255.0f, 13.0f / 255.0f, 1.0f));
        s_hostState.Ready = true;
        return true;
    }

    XG_MODULE_EXPORT void XG_ImUIHostEvent(const xgEvent* e)
    {
        if (!s_hostState.Ready || !e)
            return;
        imui::detail::platform::ProcessEvent(*e);
    }

    XG_MODULE_EXPORT void XG_ImUIHostFrame(float deltaTime)
    {
        if (!s_hostState.Ready)
            return;

        imui::detail::platform::NewFrame(deltaTime);
        ImGui::NewFrame();

        if (PrototypeShell())
        {
            imui::detail::DrawMainDockspace(s_hostState.Window);
            imui::detail::DrawPrototypePanels();
        }
        else
        {
            imui::detail::SetDefaultDockSpace(ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport(), ImGuiDockNodeFlags_PassthruCentralNode));
        }
        imui::detail::DrawRegisteredPanels();

        ImGui::Render();
        ImDrawData* drawData = ImGui::GetDrawData();
        imui::detail::render::ProcessTextures(*s_hostState.Target, drawData);
        imui::detail::render::SubmitDrawData(*s_hostState.Target, drawData);

        if (s_hostState.Viewports)
        {
            ImGui::UpdatePlatformWindows();
            ImGui::RenderPlatformWindowsDefault();
            imui::detail::platform::ShowPendingWindows();
        }
    }

    XG_MODULE_EXPORT void XG_ImUIHostSetAppIcon(const uint8_t* rgba, int width, int height)
    {
        if (!s_hostState.Ready || !rgba || width <= 0 || height <= 0)
            return;
        s_hostState.Window->SetIconRGBA(rgba, width, height);
        if (PrototypeShell())
        {
            ReleaseAppIcon();
            imui::detail::s_appIcon = imui::detail::CreateIconTexture(rgba, width, height, imui::detail::AppIconSize);
        }
    }

    XG_MODULE_EXPORT void XG_ImUIHostShutdown()
    {
        if (!s_hostState.Ready)
            return;
        s_hostState.Ready = false;

        if (s_hostState.Viewports)
        {
            ImGui::DestroyPlatformWindows();
            imui::detail::render::ShutdownViewports();
        }
        ReleaseAppIcon();
        imui::detail::ReleaseUserTextures();
        imui::detail::render::DestroyAllTextures(*s_hostState.Target);
        s_hostState.Target->SubmitUI(nullptr);
        imui::detail::platform::Shutdown();
        ImGui::DestroyContext();
        s_hostState = {};
    }
}
