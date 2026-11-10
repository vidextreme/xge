// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.
#pragma once
#include "xgWindow.h"
#include "xgEventQueue.h"
#include "xgEventDispatcher.h"
#include "xgPlatformServices.h"

#include <string>

// Forward declare SDL types to avoid leaking SDL headers
struct SDL_Window;
struct SDL_Cursor;
union SDL_Event;

namespace xg {

    class WindowSDL : public Window {
    public:
        WindowSDL(const char* title,
            int width,
            int height);

        // Secondary window (editor viewport): shares the main window's event pump.
        explicit WindowSDL(const WindowDesc& desc);

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

        void SetTitle(const char* title) override;
        const char* GetTitle() const noexcept override;

        void Minimize() override;
        void Maximize() override;
        void Restore() override;
        bool IsMaximized() const noexcept override;
        void RequestClose() override { _shouldClose = true; }

        void SetCustomFrame(bool enabled) override;
        bool HasCustomFrame() const noexcept override { return _customFrame; }
        void SetDragRegions(const WindowRect* rects, int count) override;

        void SetCursorShape(CursorShape shape) override;
        void SetClipboardText(const char* text) override;
        const char* GetClipboardText() override;
        void StartTextInput() override;
        void StopTextInput() override;
        void SetTextInputArea(const WindowRect& rect, int cursorX) override;
        float GetDisplayScale() const noexcept override;
        void SetIconRGBA(const void* rgba, int width, int height) override;
        void SetPosition(int x, int y) override;
        void GetPosition(int* x, int* y) const noexcept override;
        void SetSize(int width, int height) override;
        void Show() override;
        void Focus() override;
        void SetOpacity(float opacity) override;
        bool RequestedClose() const noexcept override { return _closeRequested; }
        void ShowWithoutActivation() override;

        int HitTestPoint(int x, int y) const noexcept;

    private:
        void Register();
        void ApplyEvent(const SDL_Event& sdl);
        static WindowSDL* Find(SDL_Window* window);

        static constexpr int ResizeBorder = 6;

        bool _secondary = false;
        bool _closeRequested = false;

        bool _customFrame = false;
        WindowRect _dragRects[MaxDragRegions] = {};
        int _dragCount = 0;

        SDL_Window* _sdlWindow = nullptr;

        static constexpr int CursorCount = static_cast<int>(CursorShape::Hidden);
        SDL_Cursor* _cursors[CursorCount] = {};
        CursorShape _cursorShape = CursorShape::Arrow;
        std::string _clipboard;

        EventQueue* _queue = nullptr;
        EventDispatcher* _dispatcher = nullptr;

        int _width = 0;
        int _height = 0;

        bool _minimized = false;
        bool _focused = true;
        bool _shouldClose = false;
    };

} // namespace xg
