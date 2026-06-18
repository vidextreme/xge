#include "pch.h"
#include "WindowSDL.h"
#include "xgEvent.h"
#include "xgEventHelpers.h"

#include <SDL3/SDL.h>
#include "platform/xgSDLToEvent.h"

namespace xg {


    // ------------------------------------------------------------
    // Constructor
    // ------------------------------------------------------------
    WindowSDL::WindowSDL(const char* title,
        int w,
        int h)
        : _width(w)
        , _height(h)
    {
        SDL_Init(SDL_INIT_VIDEO);

        _sdlWindow = SDL_CreateWindow(title, _width, _height, SDL_WINDOW_RESIZABLE);
    }

    // ------------------------------------------------------------
    // Destructor
    // ------------------------------------------------------------
    WindowSDL::~WindowSDL()
    {
        if (_sdlWindow)
        {
            SDL_DestroyWindow(_sdlWindow);
            _sdlWindow = nullptr;
        }

        SDL_Quit();
    }

    // ------------------------------------------------------------
    // PollEvents
    // ------------------------------------------------------------
    void WindowSDL::PollEvents()
    {
        SDL_Event sdl;
        while (SDL_PollEvent(&sdl))
        {
            // Update internal window state
            switch (sdl.type)
            {
            case SDL_EVENT_QUIT:
                _shouldClose = true;
                break;

            case SDL_EVENT_WINDOW_RESIZED:
                _width = sdl.window.data1;
                _height = sdl.window.data2;
                break;

            case SDL_EVENT_WINDOW_MINIMIZED:
                _minimized = true;
                break;

            case SDL_EVENT_WINDOW_RESTORED:
                _minimized = false;
                break;

            case SDL_EVENT_WINDOW_FOCUS_GAINED:
                _focused = true;
                break;

            case SDL_EVENT_WINDOW_FOCUS_LOST:
                _focused = false;
                break;
            }

            // Convert SDL → xgEvent
            xgEvent e = xg::FromSDL(sdl);
            if (e.Type == xgEventType::None)
                continue;

            // 1. Push into queue
            if (_queue)
                _queue->Push(e);

            // 2. Dispatch immediately
            if (_dispatcher)
                _dispatcher->Dispatch(e);
        }
    }

    // ------------------------------------------------------------
    // Native handle (Win32 HWND)
    // ------------------------------------------------------------
    void* WindowSDL::GetNativeHandle() const noexcept
    {
#if XG_PLATFORM_WINDOWS
        SDL_PropertiesID props = SDL_GetWindowProperties(_sdlWindow);
        return SDL_GetPointerProperty(
            props,
            SDL_PROP_WINDOW_WIN32_HWND_POINTER,
            nullptr
        );
#else
        return nullptr;
#endif
    }

    // ------------------------------------------------------------
    // Platform window pointer (SDL_Window*)
    // ------------------------------------------------------------
    void* WindowSDL::GetPlatformWindow() const noexcept
    {
        return _sdlWindow;
    }

    void WindowSDL::SetEventQueue(EventQueue* q)
    {
        _queue = q;
    }

    void WindowSDL::SetEventDispatcher(EventDispatcher* d)
    {
        _dispatcher = d;
    }

} // namespace xg
