// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.
#include "pch.h"
#include "WindowSDL.h"
#include "xgEvent.h"
#include "xgEventHelpers.h"

#include <SDL3/SDL.h>
#include "platform/xgSDLToEvent.h"

namespace xg {
    static SDL_HitTestResult SDLCALL WindowHitTest(SDL_Window*, const SDL_Point* point, void* data)
    {
        return static_cast<SDL_HitTestResult>(static_cast<const WindowSDL*>(data)->HitTestPoint(point->x, point->y));
    }

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
            SDL_Window* source = SDL_GetWindowFromEvent(&sdl);
            const bool foreign = source && source != _sdlWindow;

            // Update internal window state
            if (!foreign)
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

            xgEvent e = foreign ? xgEvent{} : xg::FromSDL(sdl);

            if (_queue && e.Type != xgEventType::None)
                _queue->Push(e);

            if (_dispatcher)
            {
                if (e.Type == xgEventType::None)
                    e.Type = xgEventType::Platform;
                e.Native = &sdl;
                _dispatcher->Dispatch(e);
            }
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

    void WindowSDL::SetTitle(const char* title)
    {
        SDL_SetWindowTitle(_sdlWindow, title ? title : "");
    }

    const char* WindowSDL::GetTitle() const noexcept
    {
        return SDL_GetWindowTitle(_sdlWindow);
    }

    void WindowSDL::Minimize()
    {
        SDL_MinimizeWindow(_sdlWindow);
    }

    void WindowSDL::Maximize()
    {
        SDL_MaximizeWindow(_sdlWindow);
    }

    void WindowSDL::Restore()
    {
        SDL_RestoreWindow(_sdlWindow);
    }

    bool WindowSDL::IsMaximized() const noexcept
    {
        return (SDL_GetWindowFlags(_sdlWindow) & SDL_WINDOW_MAXIMIZED) != 0;
    }

    void WindowSDL::SetCustomFrame(bool enabled)
    {
        if (_customFrame == enabled)
            return;
        _customFrame = enabled;
        _dragCount = 0;

        SDL_SetWindowBordered(_sdlWindow, !enabled);
        SDL_SetWindowHitTest(_sdlWindow, enabled ? &WindowHitTest : nullptr, enabled ? this : nullptr);
    }

    void WindowSDL::SetDragRegions(const WindowRect* rects, int count)
    {
        if (count < 0 || !rects)
            count = 0;
        if (count > MaxDragRegions)
            count = MaxDragRegions;
        for (int i = 0; i < count; ++i)
            _dragRects[i] = rects[i];
        _dragCount = count;
    }

    int WindowSDL::HitTestPoint(int x, int y) const noexcept
    {
        if (!IsMaximized())
        {
            int width = 0, height = 0;
            SDL_GetWindowSize(_sdlWindow, &width, &height);
            const bool left = x < ResizeBorder;
            const bool right = x >= width - ResizeBorder;
            const bool top = y < ResizeBorder;
            const bool bottom = y >= height - ResizeBorder;

            if (top && left)     return SDL_HITTEST_RESIZE_TOPLEFT;
            if (top && right)    return SDL_HITTEST_RESIZE_TOPRIGHT;
            if (bottom && left)  return SDL_HITTEST_RESIZE_BOTTOMLEFT;
            if (bottom && right) return SDL_HITTEST_RESIZE_BOTTOMRIGHT;
            if (top)             return SDL_HITTEST_RESIZE_TOP;
            if (bottom)          return SDL_HITTEST_RESIZE_BOTTOM;
            if (left)            return SDL_HITTEST_RESIZE_LEFT;
            if (right)           return SDL_HITTEST_RESIZE_RIGHT;
        }

        for (int i = 0; i < _dragCount; ++i)
        {
            const WindowRect& r = _dragRects[i];
            if (x >= r.X && x < r.X + r.Width && y >= r.Y && y < r.Y + r.Height)
                return SDL_HITTEST_DRAGGABLE;
        }

        return SDL_HITTEST_NORMAL;
    }

} // namespace xg
