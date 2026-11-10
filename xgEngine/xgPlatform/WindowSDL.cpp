// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.
#include "pch.h"
#include "WindowSDL.h"
#include "xgEvent.h"
#include "xgEventHelpers.h"

#include <SDL3/SDL.h>
#include "platform/xgSDLToEvent.h"

#include <unordered_map>

namespace xg {
    namespace
    {
        std::unordered_map<SDL_WindowID, WindowSDL*>& Registry()
        {
            static std::unordered_map<SDL_WindowID, WindowSDL*> windows;
            return windows;
        }
    }

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
        SDL_SetHint(SDL_HINT_MOUSE_FOCUS_CLICKTHROUGH, "1");
        SDL_Init(SDL_INIT_VIDEO);

        _sdlWindow = SDL_CreateWindow(title, _width, _height, SDL_WINDOW_RESIZABLE);
        Register();
    }

    WindowSDL::WindowSDL(const WindowDesc& desc)
        : _secondary(true)
        , _width(desc.Width)
        , _height(desc.Height)
    {
        SDL_WindowFlags flags = 0;
        if (desc.Flags & WindowCreateFlagsHidden)
            flags |= SDL_WINDOW_HIDDEN;
        if (desc.Flags & WindowCreateFlagsBorderless)
            flags |= SDL_WINDOW_BORDERLESS;
        if (desc.Flags & WindowCreateFlagsNoTaskbar)
            flags |= SDL_WINDOW_UTILITY;
        if (desc.Flags & WindowCreateFlagsTopMost)
            flags |= SDL_WINDOW_ALWAYS_ON_TOP;

        _sdlWindow = SDL_CreateWindow(desc.Title ? desc.Title : "", (std::max)(desc.Width, 1), (std::max)(desc.Height, 1), flags);
        if (_sdlWindow)
            SDL_SetWindowPosition(_sdlWindow, desc.X, desc.Y);
        _focused = false;
        Register();
    }

    void WindowSDL::Register()
    {
        if (_sdlWindow)
            Registry()[SDL_GetWindowID(_sdlWindow)] = this;
    }

    WindowSDL* WindowSDL::Find(SDL_Window* window)
    {
        if (!window)
            return nullptr;
        auto it = Registry().find(SDL_GetWindowID(window));
        return it == Registry().end() ? nullptr : it->second;
    }

    // ------------------------------------------------------------
    // Destructor
    // ------------------------------------------------------------
    WindowSDL::~WindowSDL()
    {
        for (SDL_Cursor*& cursor : _cursors)
        {
            if (cursor)
                SDL_DestroyCursor(cursor);
            cursor = nullptr;
        }

        if (_sdlWindow)
        {
            Registry().erase(SDL_GetWindowID(_sdlWindow));
            SDL_DestroyWindow(_sdlWindow);
            _sdlWindow = nullptr;
        }

        if (!_secondary)
            SDL_Quit();
    }

    // ------------------------------------------------------------
    // PollEvents
    // ------------------------------------------------------------
    void WindowSDL::ApplyEvent(const SDL_Event& sdl)
    {
        switch (sdl.type)
        {
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

        case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
            _closeRequested = true;
            if (!_secondary)
                _shouldClose = true;
            break;
        }
    }

    void WindowSDL::PollEvents()
    {
        if (_secondary)
            return;

        SDL_Event sdl;
        while (SDL_PollEvent(&sdl))
        {
            SDL_Window* source = SDL_GetWindowFromEvent(&sdl);
            WindowSDL* owner = Find(source);
            if (source && !owner)
                continue;

            if (sdl.type == SDL_EVENT_QUIT)
                _shouldClose = true;
            if (owner)
                owner->ApplyEvent(sdl);

            xgEvent e = xg::FromSDL(sdl);
            e.Source = owner;

            if (_queue && e.Type != xgEventType::None && (owner == this || !owner))
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

    void WindowSDL::SetCursorShape(CursorShape shape)
    {
        if (shape == _cursorShape)
            return;
        _cursorShape = shape;

        if (shape == CursorShape::Hidden)
        {
            SDL_HideCursor();
            return;
        }

        static constexpr SDL_SystemCursor systemCursors[CursorCount] =
        {
            SDL_SYSTEM_CURSOR_DEFAULT,
            SDL_SYSTEM_CURSOR_TEXT,
            SDL_SYSTEM_CURSOR_MOVE,
            SDL_SYSTEM_CURSOR_NS_RESIZE,
            SDL_SYSTEM_CURSOR_EW_RESIZE,
            SDL_SYSTEM_CURSOR_NESW_RESIZE,
            SDL_SYSTEM_CURSOR_NWSE_RESIZE,
            SDL_SYSTEM_CURSOR_POINTER,
            SDL_SYSTEM_CURSOR_WAIT,
            SDL_SYSTEM_CURSOR_PROGRESS,
            SDL_SYSTEM_CURSOR_NOT_ALLOWED,
        };

        const int index = static_cast<int>(shape);
        if (index < 0 || index >= CursorCount)
            return;
        if (!_cursors[index])
            _cursors[index] = SDL_CreateSystemCursor(systemCursors[index]);
        if (_cursors[index])
            SDL_SetCursor(_cursors[index]);
        SDL_ShowCursor();
    }

    void WindowSDL::SetClipboardText(const char* text)
    {
        SDL_SetClipboardText(text ? text : "");
    }

    const char* WindowSDL::GetClipboardText()
    {
        char* text = SDL_GetClipboardText();
        _clipboard = text ? text : "";
        SDL_free(text);
        return _clipboard.c_str();
    }

    void WindowSDL::StartTextInput()
    {
        SDL_StartTextInput(_sdlWindow);
    }

    void WindowSDL::StopTextInput()
    {
        SDL_StopTextInput(_sdlWindow);
    }

    void WindowSDL::SetTextInputArea(const WindowRect& rect, int cursorX)
    {
        const SDL_Rect area = { rect.X, rect.Y, rect.Width, rect.Height };
        SDL_SetTextInputArea(_sdlWindow, &area, cursorX);
    }

    float WindowSDL::GetDisplayScale() const noexcept
    {
        const float scale = SDL_GetWindowDisplayScale(_sdlWindow);
        return scale > 0.0f ? scale : 1.0f;
    }

    void WindowSDL::SetIconRGBA(const void* rgba, int width, int height)
    {
        if (!rgba || width <= 0 || height <= 0)
            return;
        SDL_Surface* surface = SDL_CreateSurfaceFrom(width, height, SDL_PIXELFORMAT_RGBA32, const_cast<void*>(rgba), width * 4);
        if (!surface)
            return;
        SDL_SetWindowIcon(_sdlWindow, surface);
        SDL_DestroySurface(surface);
    }

    void WindowSDL::SetPosition(int x, int y)
    {
        SDL_SetWindowPosition(_sdlWindow, x, y);
    }

    void WindowSDL::GetPosition(int* x, int* y) const noexcept
    {
        int px = 0, py = 0;
        SDL_GetWindowPosition(_sdlWindow, &px, &py);
        if (x) *x = px;
        if (y) *y = py;
    }

    void WindowSDL::SetSize(int width, int height)
    {
        SDL_SetWindowSize(_sdlWindow, (std::max)(width, 1), (std::max)(height, 1));
    }

    void WindowSDL::Show()
    {
        SDL_ShowWindow(_sdlWindow);
    }

    void WindowSDL::ShowWithoutActivation()
    {
        SDL_SetHint(SDL_HINT_WINDOW_ACTIVATE_WHEN_SHOWN, "0");
        SDL_ShowWindow(_sdlWindow);
        SDL_SetHint(SDL_HINT_WINDOW_ACTIVATE_WHEN_SHOWN, "1");
    }

    void WindowSDL::Focus()
    {
        SDL_RaiseWindow(_sdlWindow);
    }

    void WindowSDL::SetOpacity(float opacity)
    {
        SDL_SetWindowOpacity(_sdlWindow, opacity);
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
