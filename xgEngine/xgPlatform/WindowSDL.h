#pragma once
#include "xgWindow.h"

// Forward declare SDL types to avoid leaking SDL headers
struct SDL_Window;

namespace xg {

    class WindowSDL : public Window {
    public:
        WindowSDL(const char* title, int width, int height);
        ~WindowSDL() override;

        void PollEvents() override;

        void* GetNativeHandle() const noexcept override;
        void* GetPlatformWindow() const noexcept override;

        int GetWidth() const noexcept override { return width; }
        int GetHeight() const noexcept override { return height; }

        bool IsMinimized() const noexcept override { return minimized; }
        bool IsFocused() const noexcept override { return focused; }

        bool ShouldClose() const noexcept override { return shouldClose; }

    private:
        SDL_Window* sdlWindow = nullptr;

        int width = 0;
        int height = 0;

        bool minimized = false;
        bool focused = true;
        bool shouldClose = false;
    };

} // namespace xg
