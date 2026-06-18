#pragma once
#include "xgWindow.h"
#include "xgEventQueue.h"
#include "xgEventDispatcher.h"

// Forward declare SDL types to avoid leaking SDL headers
struct SDL_Window;

namespace xg {

    class WindowSDL : public Window {
    public:
        WindowSDL(const char* title,
            int width,
            int height);

        ~WindowSDL() override;

        void PollEvents() override;

        void* GetNativeHandle() const noexcept override;
        void* GetPlatformWindow() const noexcept override;

        int GetWidth() const noexcept override { return _width; }
        int GetHeight() const noexcept override { return _height; }

        bool IsMinimized() const noexcept override { return _minimized; }
        bool IsFocused() const noexcept override { return _focused; }

        bool ShouldClose() const noexcept override { return _shouldClose; }

        void SetEventQueue(EventQueue* q) override;
        void SetEventDispatcher(EventDispatcher* d) override;

    private:
        SDL_Window* _sdlWindow = nullptr;

        EventQueue* _queue = nullptr;
        EventDispatcher* _dispatcher = nullptr;

        int _width = 0;
        int _height = 0;

        bool _minimized = false;
        bool _focused = true;
        bool _shouldClose = false;
    };

} // namespace xg
