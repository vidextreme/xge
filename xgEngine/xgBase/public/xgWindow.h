// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.
#pragma once
#include "xgHandles.h"

namespace xg {

    class EventQueue;
	class EventDispatcher;

    struct WindowRect
    {
        int X;
        int Y;
        int Width;
        int Height;
    };

    enum class CursorShape : int
    {
        Arrow,
        TextInput,
        ResizeAll,
        ResizeNS,
        ResizeEW,
        ResizeNESW,
        ResizeNWSE,
        Hand,
        Wait,
        Progress,
        NotAllowed,
        Hidden,
    };

    class Window {
    public:
        virtual ~Window() = default;

        // Pump OS/platform events
        virtual void PollEvents() = 0;

        // Native OS window handle (HWND on Windows, NSWindow* on macOS, etc.)
        virtual void* GetNativeHandle() const noexcept = 0;
        virtual void* GetPlatformWindow() const noexcept = 0;

        // Dimensions
        virtual int GetWidth() const noexcept = 0;
        virtual int GetHeight() const noexcept = 0;

        // State
        virtual bool IsMinimized() const noexcept = 0;
        virtual bool IsFocused() const noexcept = 0;

        // Close request (e.g., user clicked X)
        virtual bool ShouldClose() const noexcept = 0;

		virtual void SetEventQueue(EventQueue* q) = 0;
		virtual void SetEventDispatcher(EventDispatcher* d) = 0;

        virtual void SetTitle(const char* title) = 0;
        virtual const char* GetTitle() const noexcept = 0;

        virtual void Minimize() = 0;
        virtual void Maximize() = 0;
        virtual void Restore() = 0;
        virtual bool IsMaximized() const noexcept = 0;

        virtual void RequestClose() = 0;

        virtual void SetCustomFrame(bool enabled) = 0;
        virtual bool HasCustomFrame() const noexcept = 0;

        static constexpr int MaxDragRegions = 8;
        virtual void SetDragRegions(const WindowRect* rects, int count) = 0;

        virtual void SetCursorShape(CursorShape shape) = 0;

        virtual void SetClipboardText(const char* text) = 0;
        // Valid until the next call on this window.
        virtual const char* GetClipboardText() = 0;

        virtual void StartTextInput() = 0;
        virtual void StopTextInput() = 0;
        virtual void SetTextInputArea(const WindowRect& rect, int cursorX) = 0;

        virtual float GetDisplayScale() const noexcept = 0;

        virtual void SetIconRGBA(const void* rgba, int width, int height) = 0;

        // Desktop position of the client area, in logical pixels.
        virtual void SetPosition(int x, int y) = 0;
        virtual void GetPosition(int* x, int* y) const noexcept = 0;
        virtual void SetSize(int width, int height) = 0;
        virtual void Show() = 0;
        virtual void Focus() = 0;
        virtual void SetOpacity(float opacity) = 0;
        // True once the OS asked to close this window. Secondary windows never quit the app.
        virtual bool RequestedClose() const noexcept = 0;

        // Shows the window without taking keyboard focus (e.g. a window spawned mid-drag).
        virtual void ShowWithoutActivation() = 0;
    };

    // Factory implemented in xgPlatform
    XG_DECLARE_UNIQUE(Window)
    XG_API WindowUnique NewWindow(const char* title, int width, int height);

} // namespace xg
