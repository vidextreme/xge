#include "pch.h"
#include "WindowSDL.h"
#include <SDL3/SDL.h>

namespace xg {

    WindowSDL::WindowSDL(const char* title, int w, int h)
        : width(w), height(h)
    {
        SDL_Init(SDL_INIT_VIDEO);

        sdlWindow = SDL_CreateWindow(title, width, height, SDL_WINDOW_RESIZABLE);
    }

    WindowSDL::~WindowSDL() {
        if (sdlWindow) {
            SDL_DestroyWindow(sdlWindow);
            sdlWindow = nullptr;
        }

        SDL_Quit();
    }

    void WindowSDL::PollEvents() {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {

            switch (e.type) {

            case SDL_EVENT_QUIT:
                shouldClose = true;
                break;

            case SDL_EVENT_WINDOW_RESIZED:
                width = e.window.data1;
                height = e.window.data2;
                break;

            case SDL_EVENT_WINDOW_MINIMIZED:
                minimized = true;
                break;

            case SDL_EVENT_WINDOW_RESTORED:
                minimized = false;
                break;

            case SDL_EVENT_WINDOW_FOCUS_GAINED:
                focused = true;
                break;

            case SDL_EVENT_WINDOW_FOCUS_LOST:
                focused = false;
                break;
            }
        }
    }

    void* WindowSDL::GetNativeHandle() const noexcept {
#if XG_PLATFORM_WINDOWS
        SDL_PropertiesID props = SDL_GetWindowProperties(sdlWindow);
        return SDL_GetPointerProperty(
            props,
            SDL_PROP_WINDOW_WIN32_HWND_POINTER,
            nullptr
        );
#else
        return nullptr;
#endif
    }

} // namespace xg
