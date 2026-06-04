#include "pch.h"
#include "WindowSDL.h"
#include <SDL3/SDL.h>

namespace xg {

    WindowSDL::WindowSDL(const char* title, int width, int height) {
        SDL_Init(SDL_INIT_VIDEO);

        window = SDL_CreateWindow(title, width, height, 0);
    }

    WindowSDL::~WindowSDL() {
        if (window) {
            SDL_DestroyWindow(window);
            window = nullptr;
        }
        SDL_QuitSubSystem(SDL_INIT_VIDEO);
    }

    void WindowSDL::PollEvents() {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_EVENT_QUIT) {
                shouldClose = true;
            }
        }
    }


    void* WindowSDL::NativeHandle() noexcept {
        // SDL3: get property list, then get HWND pointer
        SDL_PropertiesID props = SDL_GetWindowProperties(window);
        return SDL_GetPointerProperty(
            props,
            SDL_PROP_WINDOW_WIN32_HWND_POINTER,
            nullptr
        );
    }

    int WindowSDL::Width() const noexcept {
        int w = 0;
        SDL_GetWindowSize(window, &w, nullptr);
        return w;
    }

    int WindowSDL::Height() const noexcept {
        int h = 0;
        SDL_GetWindowSize(window, nullptr, &h);
        return h;
    }

    bool WindowSDL::IsMinimized() const noexcept {
        return (SDL_GetWindowFlags(window) & SDL_WINDOW_MINIMIZED) != 0;
    }

    bool WindowSDL::IsFocused() const noexcept {
        return (SDL_GetWindowFlags(window) & SDL_WINDOW_INPUT_FOCUS) != 0;
    }

} // namespace xg

