#pragma once
#include "Window.h"
#include <SDL3/SDL.h>

namespace xg {

    class WindowSDL : public Window {
    public:
        WindowSDL(const char* title, int width, int height);
        ~WindowSDL() override;

        void PollEvents() override;
        void* NativeHandle() noexcept override;

        int Width() const noexcept override;
        int Height() const noexcept override;

        bool IsMinimized() const noexcept override;
        bool IsFocused() const noexcept override;

        bool ShouldClose() const noexcept override { return shouldClose; }

    private:
        SDL_Window* window = nullptr;
        bool shouldClose = false;
    };

} // namespace xg