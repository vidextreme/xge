#pragma once
#include "xgBase.h"
#include <memory>

namespace xg {

    class Window {
    public:
        virtual ~Window() = default;

        // Pump OS/platform events
        virtual void PollEvents() = 0;

        // Native OS window handle (HWND, NSWindow*, etc.)
        virtual void* NativeHandle() noexcept = 0;

        // Dimensions
        virtual int Width() const noexcept = 0;
        virtual int Height() const noexcept = 0;

        // State
        virtual bool IsMinimized() const noexcept = 0;
        virtual bool IsFocused() const noexcept = 0;

        virtual bool ShouldClose() const noexcept = 0;
    };

    // Factory implemented in xgPlatform
    XG_API std::unique_ptr<Window> CreateWindow(const char* title, int width, int height);

} // namespace xg
