#pragma once
#include "xgHandles.h"

namespace xg {

    class EventQueue;
	class EventDispatcher;
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
    };

    // Factory implemented in xgPlatform
    XG_DECLARE_UNIQUE(Window)
    XG_API WindowUnique NewWindow(const char* title, int width, int height);

} // namespace xg
