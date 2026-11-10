// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.
#include "UIPlatformBackend.h"
#include "imgui.h"
#include "xgEvent.h"
#include "xgWindow.h"
#include "xgPlatformServices.h"

#include <algorithm>
#include <cfloat>
#include <cstdlib>

namespace xg::imui::detail::platform
{
    namespace
    {
        struct BackendData
        {
            Window* MainWindow = nullptr;
            PlatformServices* Services = nullptr;
            ImGuiMouseCursor LastCursor = ImGuiMouseCursor_COUNT;
            bool TextInputActive = false;
            Window* HoveredWindow = nullptr;
            int ButtonsDown = 0;
            int PendingLeaveFrame = 0;
            float DpiOverride = 0.0f;
        };

        BackendData* GetData()
        {
            return ImGui::GetCurrentContext() ? static_cast<BackendData*>(ImGui::GetIO().BackendPlatformUserData) : nullptr;
        }

        const char* GetClipboardText(ImGuiContext*)
        {
            BackendData* bd = GetData();
            return bd && bd->MainWindow ? bd->MainWindow->GetClipboardText() : nullptr;
        }

        void SetClipboardText(ImGuiContext*, const char* text)
        {
            if (BackendData* bd = GetData(); bd && bd->MainWindow)
                bd->MainWindow->SetClipboardText(text);
        }

        void SetImeData(ImGuiContext*, ImGuiViewport* viewport, ImGuiPlatformImeData* data)
        {
            BackendData* bd = GetData();
            if (!bd || !bd->MainWindow)
                return;

            const bool want = data->WantVisible || data->WantTextInput;
            if (!want)
            {
                if (bd->TextInputActive)
                    bd->MainWindow->StopTextInput();
                bd->TextInputActive = false;
                return;
            }

            if (data->WantVisible)
            {
                const ImVec2 origin = viewport ? viewport->Pos : ImVec2(0.0f, 0.0f);
                WindowRect area = {};
                area.X = static_cast<int>(data->InputPos.x - origin.x);
                area.Y = static_cast<int>(data->InputPos.y - origin.y);
                area.Width = 1;
                area.Height = static_cast<int>(data->InputLineHeight);
                bd->MainWindow->SetTextInputArea(area, 0);
            }
            if (!bd->TextInputActive)
                bd->MainWindow->StartTextInput();
            bd->TextInputActive = true;
        }

        ImGuiKey ToImGuiKey(xgKey key)
        {
            switch (key)
            {
            case xgKey::Tab: return ImGuiKey_Tab;
            case xgKey::Left: return ImGuiKey_LeftArrow;
            case xgKey::Right: return ImGuiKey_RightArrow;
            case xgKey::Up: return ImGuiKey_UpArrow;
            case xgKey::Down: return ImGuiKey_DownArrow;
            case xgKey::PageUp: return ImGuiKey_PageUp;
            case xgKey::PageDown: return ImGuiKey_PageDown;
            case xgKey::Home: return ImGuiKey_Home;
            case xgKey::End: return ImGuiKey_End;
            case xgKey::Insert: return ImGuiKey_Insert;
            case xgKey::Delete: return ImGuiKey_Delete;
            case xgKey::Backspace: return ImGuiKey_Backspace;
            case xgKey::Space: return ImGuiKey_Space;
            case xgKey::Enter: return ImGuiKey_Enter;
            case xgKey::Escape: return ImGuiKey_Escape;
            case xgKey::LeftCtrl: return ImGuiKey_LeftCtrl;
            case xgKey::LeftShift: return ImGuiKey_LeftShift;
            case xgKey::LeftAlt: return ImGuiKey_LeftAlt;
            case xgKey::LeftSuper: return ImGuiKey_LeftSuper;
            case xgKey::RightCtrl: return ImGuiKey_RightCtrl;
            case xgKey::RightShift: return ImGuiKey_RightShift;
            case xgKey::RightAlt: return ImGuiKey_RightAlt;
            case xgKey::RightSuper: return ImGuiKey_RightSuper;
            case xgKey::Menu: return ImGuiKey_Menu;
            case xgKey::Apostrophe: return ImGuiKey_Apostrophe;
            case xgKey::Comma: return ImGuiKey_Comma;
            case xgKey::Minus: return ImGuiKey_Minus;
            case xgKey::Period: return ImGuiKey_Period;
            case xgKey::Slash: return ImGuiKey_Slash;
            case xgKey::Semicolon: return ImGuiKey_Semicolon;
            case xgKey::Equal: return ImGuiKey_Equal;
            case xgKey::LeftBracket: return ImGuiKey_LeftBracket;
            case xgKey::Backslash: return ImGuiKey_Backslash;
            case xgKey::RightBracket: return ImGuiKey_RightBracket;
            case xgKey::GraveAccent: return ImGuiKey_GraveAccent;
            case xgKey::CapsLock: return ImGuiKey_CapsLock;
            case xgKey::ScrollLock: return ImGuiKey_ScrollLock;
            case xgKey::NumLock: return ImGuiKey_NumLock;
            case xgKey::PrintScreen: return ImGuiKey_PrintScreen;
            case xgKey::Pause: return ImGuiKey_Pause;
            case xgKey::KeypadDecimal: return ImGuiKey_KeypadDecimal;
            case xgKey::KeypadDivide: return ImGuiKey_KeypadDivide;
            case xgKey::KeypadMultiply: return ImGuiKey_KeypadMultiply;
            case xgKey::KeypadSubtract: return ImGuiKey_KeypadSubtract;
            case xgKey::KeypadAdd: return ImGuiKey_KeypadAdd;
            case xgKey::KeypadEnter: return ImGuiKey_KeypadEnter;
            case xgKey::KeypadEqual: return ImGuiKey_KeypadEqual;
            default: break;
            }
            if (key >= xgKey::Num0 && key <= xgKey::Num9)
                return static_cast<ImGuiKey>(ImGuiKey_0 + (static_cast<int>(key) - static_cast<int>(xgKey::Num0)));
            if (key >= xgKey::A && key <= xgKey::Z)
                return static_cast<ImGuiKey>(ImGuiKey_A + (static_cast<int>(key) - static_cast<int>(xgKey::A)));
            if (key >= xgKey::F1 && key <= xgKey::F12)
                return static_cast<ImGuiKey>(ImGuiKey_F1 + (static_cast<int>(key) - static_cast<int>(xgKey::F1)));
            if (key >= xgKey::Keypad0 && key <= xgKey::Keypad9)
                return static_cast<ImGuiKey>(ImGuiKey_Keypad0 + (static_cast<int>(key) - static_cast<int>(xgKey::Keypad0)));
            return ImGuiKey_None;
        }

        CursorShape ToCursorShape(ImGuiMouseCursor cursor)
        {
            switch (cursor)
            {
            case ImGuiMouseCursor_TextInput: return CursorShape::TextInput;
            case ImGuiMouseCursor_ResizeAll: return CursorShape::ResizeAll;
            case ImGuiMouseCursor_ResizeNS: return CursorShape::ResizeNS;
            case ImGuiMouseCursor_ResizeEW: return CursorShape::ResizeEW;
            case ImGuiMouseCursor_ResizeNESW: return CursorShape::ResizeNESW;
            case ImGuiMouseCursor_ResizeNWSE: return CursorShape::ResizeNWSE;
            case ImGuiMouseCursor_Hand: return CursorShape::Hand;
            case ImGuiMouseCursor_Wait: return CursorShape::Wait;
            case ImGuiMouseCursor_Progress: return CursorShape::Progress;
            case ImGuiMouseCursor_NotAllowed: return CursorShape::NotAllowed;
            case ImGuiMouseCursor_None: return CursorShape::Hidden;
            default: return CursorShape::Arrow;
            }
        }

        struct ViewportData
        {
            Window* PlatformWindow = nullptr;
            bool Owned = false;
            bool PendingShow = false;
            bool ActivateOnShow = true;
            int IgnorePosEventFrame = -1;
            int IgnoreSizeEventFrame = -1;
        };

        Window* WindowOf(ImGuiViewport* viewport)
        {
            ViewportData* data = viewport ? static_cast<ViewportData*>(viewport->PlatformUserData) : nullptr;
            return data ? data->PlatformWindow : nullptr;
        }

        float ScaleOf(const BackendData* bd, const Window* window)
        {
            if (bd && bd->DpiOverride > 0.0f)
                return bd->DpiOverride;
            const float scale = window ? window->GetDisplayScale() : 1.0f;
            return (std::clamp)(scale, 0.5f, 4.0f);
        }

        float GetViewportDpiScale(ImGuiViewport* viewport)
        {
            BackendData* bd = GetData();
            Window* window = WindowOf(viewport);
            if (!window && bd && viewport == ImGui::GetMainViewport())
                window = bd->MainWindow;
            return ScaleOf(bd, window);
        }

        float ReadDpiOverride()
        {
            char* value = nullptr;
            size_t length = 0;
            float scale = 0.0f;
            if (_dupenv_s(&value, &length, "XG_UI_DPI_SCALE") == 0 && value)
            {
                scale = std::strtof(value, nullptr);
                free(value);
            }
            return scale > 0.0f ? (std::clamp)(scale, 0.5f, 4.0f) : 0.0f;
        }

        void CreateViewportWindow(ImGuiViewport* viewport)
        {
            BackendData* bd = GetData();
            uint32_t flags = WindowCreateFlagsHidden;
            if (viewport->Flags & ImGuiViewportFlags_NoDecoration)
                flags |= WindowCreateFlagsBorderless;
            if (viewport->Flags & ImGuiViewportFlags_NoTaskBarIcon)
                flags |= WindowCreateFlagsNoTaskbar;
            if (viewport->Flags & ImGuiViewportFlags_TopMost)
                flags |= WindowCreateFlagsTopMost;

            WindowDesc desc = {};
            desc.Title = "xgImUI";
            desc.X = static_cast<int>(viewport->Pos.x);
            desc.Y = static_cast<int>(viewport->Pos.y);
            desc.Width = static_cast<int>(viewport->Size.x);
            desc.Height = static_cast<int>(viewport->Size.y);
            desc.Flags = flags;

            ViewportData* data = IM_NEW(ViewportData)();
            data->PlatformWindow = bd && bd->Services ? bd->Services->CreateSecondaryWindow(desc) : nullptr;
            data->Owned = true;
            viewport->PlatformUserData = data;
            viewport->PlatformHandle = data->PlatformWindow;
            viewport->PlatformHandleRaw = data->PlatformWindow ? data->PlatformWindow->GetNativeHandle() : nullptr;
        }

        void DestroyViewportWindow(ImGuiViewport* viewport)
        {
            ViewportData* data = static_cast<ViewportData*>(viewport->PlatformUserData);
            if (data)
            {
                BackendData* bd = GetData();
                if (data->Owned && data->PlatformWindow && bd && bd->Services)
                    bd->Services->DestroySecondaryWindow(data->PlatformWindow);
                IM_DELETE(data);
            }
            viewport->PlatformUserData = nullptr;
            viewport->PlatformHandle = nullptr;
            viewport->PlatformHandleRaw = nullptr;
        }

        void ShowViewportWindow(ImGuiViewport* viewport)
        {
            ViewportData* data = static_cast<ViewportData*>(viewport->PlatformUserData);
            if (!data || !data->PlatformWindow)
                return;
            data->PendingShow = true;
            data->ActivateOnShow = (viewport->Flags & ImGuiViewportFlags_NoFocusOnAppearing) == 0;
        }

        void SetViewportPos(ImGuiViewport* viewport, ImVec2 pos)
        {
            ViewportData* data = static_cast<ViewportData*>(viewport->PlatformUserData);
            if (!data || !data->PlatformWindow)
                return;
            data->IgnorePosEventFrame = ImGui::GetFrameCount();
            data->PlatformWindow->SetPosition(static_cast<int>(pos.x), static_cast<int>(pos.y));
        }

        ImVec2 GetViewportPos(ImGuiViewport* viewport)
        {
            int x = 0, y = 0;
            if (Window* window = WindowOf(viewport))
                window->GetPosition(&x, &y);
            return ImVec2(static_cast<float>(x), static_cast<float>(y));
        }

        void SetViewportSize(ImGuiViewport* viewport, ImVec2 size)
        {
            ViewportData* data = static_cast<ViewportData*>(viewport->PlatformUserData);
            if (!data || !data->PlatformWindow)
                return;
            data->IgnoreSizeEventFrame = ImGui::GetFrameCount();
            data->PlatformWindow->SetSize(static_cast<int>(size.x), static_cast<int>(size.y));
        }

        ImVec2 GetViewportSize(ImGuiViewport* viewport)
        {
            Window* window = WindowOf(viewport);
            return window ? ImVec2(static_cast<float>(window->GetWidth()), static_cast<float>(window->GetHeight())) : ImVec2(0.0f, 0.0f);
        }

        void SetViewportFocus(ImGuiViewport* viewport)
        {
            if (Window* window = WindowOf(viewport))
                window->Focus();
        }

        bool GetViewportFocus(ImGuiViewport* viewport)
        {
            Window* window = WindowOf(viewport);
            return window && window->IsFocused();
        }

        bool GetViewportMinimized(ImGuiViewport* viewport)
        {
            Window* window = WindowOf(viewport);
            return window && window->IsMinimized();
        }

        void SetViewportTitle(ImGuiViewport* viewport, const char* title)
        {
            if (Window* window = WindowOf(viewport))
                window->SetTitle(title);
        }

        void SetViewportAlpha(ImGuiViewport* viewport, float alpha)
        {
            if (Window* window = WindowOf(viewport))
                window->SetOpacity(alpha);
        }

        bool AnyViewportFocused()
        {
            for (ImGuiViewport* viewport : ImGui::GetPlatformIO().Viewports)
                if (GetViewportFocus(viewport))
                    return true;
            return false;
        }

        void UpdateMonitors(BackendData* bd)
        {
            MonitorInfo monitors[16];
            const int count = bd->Services->GetMonitors(monitors, 16);
            ImGuiPlatformIO& pio = ImGui::GetPlatformIO();
            pio.Monitors.resize(0);
            for (int i = 0; i < count; ++i)
            {
                ImGuiPlatformMonitor monitor;
                monitor.MainPos = ImVec2(static_cast<float>(monitors[i].Bounds.X), static_cast<float>(monitors[i].Bounds.Y));
                monitor.MainSize = ImVec2(static_cast<float>(monitors[i].Bounds.Width), static_cast<float>(monitors[i].Bounds.Height));
                monitor.WorkPos = ImVec2(static_cast<float>(monitors[i].WorkBounds.X), static_cast<float>(monitors[i].WorkBounds.Y));
                monitor.WorkSize = ImVec2(static_cast<float>(monitors[i].WorkBounds.Width), static_cast<float>(monitors[i].WorkBounds.Height));
                monitor.DpiScale = monitors[i].DpiScale;
                pio.Monitors.push_back(monitor);
            }
        }

        ImVec2 ToBackendPos(BackendData* bd, const xgEvent& e, int x, int y)
        {
            if (!bd->Services)
                return ImVec2(static_cast<float>(x), static_cast<float>(y));
            Window* source = e.Source ? e.Source : bd->MainWindow;
            int ox = 0, oy = 0;
            source->GetPosition(&ox, &oy);
            return ImVec2(static_cast<float>(x + ox), static_cast<float>(y + oy));
        }

        void UpdateMouseCursor(BackendData* bd)
        {
            ImGuiIO& io = ImGui::GetIO();
            if (io.ConfigFlags & ImGuiConfigFlags_NoMouseCursorChange)
                return;
            const ImGuiMouseCursor cursor = io.MouseDrawCursor ? ImGuiMouseCursor_None : ImGui::GetMouseCursor();
            if (cursor == bd->LastCursor)
                return;
            bd->LastCursor = cursor;
            bd->MainWindow->SetCursorShape(ToCursorShape(cursor));
        }
    }

    bool Init(Window* window, PlatformServices* services)
    {
        if (!window)
            return false;
        ImGuiIO& io = ImGui::GetIO();
        IM_ASSERT(io.BackendPlatformUserData == nullptr);

        BackendData* bd = IM_NEW(BackendData)();
        bd->MainWindow = window;
        bd->Services = services;
        bd->DpiOverride = ReadDpiOverride();
        io.BackendPlatformUserData = bd;
        io.ConfigDpiScaleFonts = true;
        io.ConfigDpiScaleViewports = services != nullptr;
        io.BackendPlatformName = "xg::Window";
        io.BackendFlags |= ImGuiBackendFlags_HasMouseCursors;

        ImGuiPlatformIO& pio = ImGui::GetPlatformIO();
        pio.Platform_GetClipboardTextFn = GetClipboardText;
        pio.Platform_SetClipboardTextFn = SetClipboardText;
        pio.Platform_SetImeDataFn = SetImeData;
        pio.Platform_GetWindowDpiScale = GetViewportDpiScale;

        ImGuiViewport* mainViewport = ImGui::GetMainViewport();
        mainViewport->PlatformHandleRaw = window->GetNativeHandle();

        if (services)
        {
            io.BackendFlags |= ImGuiBackendFlags_PlatformHasViewports;
            ViewportData* data = IM_NEW(ViewportData)();
            data->PlatformWindow = window;
            data->Owned = false;
            mainViewport->PlatformUserData = data;
            mainViewport->PlatformHandle = window;

            pio.Platform_CreateWindow = CreateViewportWindow;
            pio.Platform_DestroyWindow = DestroyViewportWindow;
            pio.Platform_ShowWindow = ShowViewportWindow;
            pio.Platform_SetWindowPos = SetViewportPos;
            pio.Platform_GetWindowPos = GetViewportPos;
            pio.Platform_SetWindowSize = SetViewportSize;
            pio.Platform_GetWindowSize = GetViewportSize;
            pio.Platform_SetWindowFocus = SetViewportFocus;
            pio.Platform_GetWindowFocus = GetViewportFocus;
            pio.Platform_GetWindowMinimized = GetViewportMinimized;
            pio.Platform_SetWindowTitle = SetViewportTitle;
            pio.Platform_SetWindowAlpha = SetViewportAlpha;
            UpdateMonitors(bd);
        }
        return true;
    }

    float DisplayScale()
    {
        BackendData* bd = GetData();
        return bd ? ScaleOf(bd, bd->MainWindow) : 1.0f;
    }

    void Shutdown()
    {
        BackendData* bd = GetData();
        if (!bd)
            return;
        if (bd->TextInputActive && bd->MainWindow)
            bd->MainWindow->StopTextInput();

        ImGuiIO& io = ImGui::GetIO();
        ImGuiPlatformIO& pio = ImGui::GetPlatformIO();
        if (ViewportData* data = static_cast<ViewportData*>(ImGui::GetMainViewport()->PlatformUserData))
            IM_DELETE(data);
        ImGui::GetMainViewport()->PlatformUserData = nullptr;
        pio.Platform_GetClipboardTextFn = nullptr;
        pio.Platform_SetClipboardTextFn = nullptr;
        pio.Platform_SetImeDataFn = nullptr;
        io.BackendPlatformName = nullptr;
        io.BackendPlatformUserData = nullptr;
        io.BackendFlags &= ~(ImGuiBackendFlags_HasMouseCursors | ImGuiBackendFlags_PlatformHasViewports);
        IM_DELETE(bd);
    }

    void NewFrame(float deltaTime)
    {
        BackendData* bd = GetData();
        IM_ASSERT(bd != nullptr && "UI platform backend not initialized");

        ImGuiIO& io = ImGui::GetIO();
        Window* window = bd->MainWindow;
        const bool minimized = window->IsMinimized();
        io.DisplaySize = minimized ? ImVec2(0.0f, 0.0f) : ImVec2(static_cast<float>(window->GetWidth()), static_cast<float>(window->GetHeight()));
        io.DisplayFramebufferScale = ImVec2(1.0f, 1.0f);
        io.DeltaTime = deltaTime > 0.0f ? deltaTime : 1.0f / 60.0f;

        if (bd->Services)
        {
            UpdateMonitors(bd);
            float mx = 0.0f, my = 0.0f;
            const bool useGlobal = AnyViewportFocused() && (bd->ButtonsDown != 0 || !bd->HoveredWindow);
            if (useGlobal && bd->Services->GetGlobalMousePosition(&mx, &my))
                io.AddMousePosEvent(mx, my);
            else if (!bd->HoveredWindow && bd->PendingLeaveFrame != 0 && bd->PendingLeaveFrame <= ImGui::GetFrameCount() + 1 && bd->ButtonsDown == 0)
                io.AddMousePosEvent(-FLT_MAX, -FLT_MAX);
            bd->PendingLeaveFrame = 0;
        }

        UpdateMouseCursor(bd);
    }

    void ProcessEvent(const xgEvent& e)
    {
        BackendData* bd = GetData();
        if (!bd)
            return;
        ImGuiIO& io = ImGui::GetIO();

        switch (e.Type)
        {
        case xgEventType::MouseMove:
        {
            bd->HoveredWindow = e.Source ? e.Source : bd->MainWindow;
            bd->PendingLeaveFrame = 0;
            io.AddMouseSourceEvent(ImGuiMouseSource_Mouse);
            if (bd->Services && bd->ButtonsDown != 0)
                break;
            const ImVec2 pos = ToBackendPos(bd, e, e.MouseMove.X, e.MouseMove.Y);
            io.AddMousePosEvent(pos.x, pos.y);
            break;
        }

        case xgEventType::MouseButtonDown:
        case xgEventType::MouseButtonUp:
        {
            int button = -1;
            switch (e.MouseButton.Button)
            {
            case xgMouseButton::Left: button = 0; break;
            case xgMouseButton::Right: button = 1; break;
            case xgMouseButton::Middle: button = 2; break;
            case xgMouseButton::X1: button = 3; break;
            case xgMouseButton::X2: button = 4; break;
            }
            if (button < 0)
                break;
            io.AddMouseSourceEvent(ImGuiMouseSource_Mouse);
            if (!bd->Services)
            {
                const ImVec2 pos = ToBackendPos(bd, e, e.MouseButton.X, e.MouseButton.Y);
                io.AddMousePosEvent(pos.x, pos.y);
            }
            io.AddMouseButtonEvent(button, e.Type == xgEventType::MouseButtonDown);
            if (e.Type == xgEventType::MouseButtonDown)
                bd->ButtonsDown |= 1 << button;
            else
                bd->ButtonsDown &= ~(1 << button);
            break;
        }

        case xgEventType::MouseWheel:
            io.AddMouseSourceEvent(ImGuiMouseSource_Mouse);
            io.AddMouseWheelEvent(-e.MouseWheel.X, e.MouseWheel.Y);
            break;

        case xgEventType::MouseLeave:
            if (!bd->Services)
            {
                io.AddMousePosEvent(-FLT_MAX, -FLT_MAX);
                break;
            }
            if (!e.Source || e.Source == bd->HoveredWindow)
                bd->HoveredWindow = nullptr;
            bd->PendingLeaveFrame = ImGui::GetFrameCount() + 1;
            break;

        case xgEventType::WindowMoved:
        case xgEventType::WindowResize:
        case xgEventType::WindowCloseRequested:
            if (bd->Services && e.Source && e.Source != bd->MainWindow)
            {
                if (ImGuiViewport* viewport = ImGui::FindViewportByPlatformHandle(e.Source))
                {
                    const ViewportData* data = static_cast<const ViewportData*>(viewport->PlatformUserData);
                    const int frame = ImGui::GetFrameCount();
                    if (e.Type == xgEventType::WindowMoved)
                    {
                        if (!data || frame > data->IgnorePosEventFrame + 1)
                            viewport->PlatformRequestMove = true;
                    }
                    else if (e.Type == xgEventType::WindowResize)
                    {
                        if (!data || frame > data->IgnoreSizeEventFrame + 1)
                            viewport->PlatformRequestResize = true;
                    }
                    else
                    {
                        viewport->PlatformRequestClose = true;
                    }
                }
            }
            break;

        case xgEventType::TextInput:
            io.AddInputCharactersUTF8(e.TextInput.Text);
            break;

        case xgEventType::KeyDown:
        case xgEventType::KeyUp:
        {
            const bool down = e.Type == xgEventType::KeyDown;
            io.AddKeyEvent(ImGuiMod_Ctrl, (e.Key.Mods & xgKeyModCtrl) != 0);
            io.AddKeyEvent(ImGuiMod_Shift, (e.Key.Mods & xgKeyModShift) != 0);
            io.AddKeyEvent(ImGuiMod_Alt, (e.Key.Mods & xgKeyModAlt) != 0);
            io.AddKeyEvent(ImGuiMod_Super, (e.Key.Mods & xgKeyModSuper) != 0);
            const ImGuiKey key = ToImGuiKey(e.Key.Code);
            if (key != ImGuiKey_None)
            {
                io.AddKeyEvent(key, down);
                io.SetKeyEventNativeData(key, e.Key.Key, e.Key.Scancode, e.Key.Scancode);
            }
            break;
        }

        case xgEventType::WindowFocusGained:
            io.AddFocusEvent(true);
            break;

        case xgEventType::WindowFocusLost:
            if (!bd->Services || !AnyViewportFocused())
            {
                io.AddFocusEvent(false);
                bd->ButtonsDown = 0;
            }
            break;

        default:
            break;
        }
    }

    void ShowPendingWindows()
    {
        if (!GetData())
            return;
        for (ImGuiViewport* viewport : ImGui::GetPlatformIO().Viewports)
        {
            ViewportData* data = static_cast<ViewportData*>(viewport->PlatformUserData);
            if (!data || !data->PendingShow || !data->PlatformWindow)
                continue;
            data->PendingShow = false;
            if (data->ActivateOnShow)
                data->PlatformWindow->Show();
            else
                data->PlatformWindow->ShowWithoutActivation();
        }
    }
}
