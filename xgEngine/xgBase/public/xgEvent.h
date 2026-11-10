// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.
#pragma once
#include <cstdint>

namespace xg {

    //
    // Event type enum
    //
    enum class xgEventType : uint8_t
    {
        None = 0,

        Quit,

        WindowResize,
        WindowMinimized,
        WindowRestored,
        WindowFocusGained,
        WindowFocusLost,

        KeyDown,
        KeyUp,
        TextInput,

        MouseMove,
        MouseButtonDown,
        MouseButtonUp,
        MouseWheel,

        GamepadConnected,
        GamepadDisconnected,
        GamepadButtonDown,
        GamepadButtonUp,
        GamepadAxisMotion,

        Platform,

        MouseLeave,

        WindowMoved,
        WindowCloseRequested,
    };

    //
    // Platform-neutral key codes (filled by the platform layer)
    //
    enum class xgKey : uint16_t
    {
        None = 0,

        Tab, Left, Right, Up, Down, PageUp, PageDown, Home, End, Insert, Delete,
        Backspace, Space, Enter, Escape,

        LeftCtrl, LeftShift, LeftAlt, LeftSuper, RightCtrl, RightShift, RightAlt, RightSuper, Menu,

        Num0, Num1, Num2, Num3, Num4, Num5, Num6, Num7, Num8, Num9,

        A, B, C, D, E, F, G, H, I, J, K, L, M, N, O, P, Q, R, S, T, U, V, W, X, Y, Z,

        F1, F2, F3, F4, F5, F6, F7, F8, F9, F10, F11, F12,

        Apostrophe, Comma, Minus, Period, Slash, Semicolon, Equal,
        LeftBracket, Backslash, RightBracket, GraveAccent,

        CapsLock, ScrollLock, NumLock, PrintScreen, Pause,

        Keypad0, Keypad1, Keypad2, Keypad3, Keypad4, Keypad5, Keypad6, Keypad7, Keypad8, Keypad9,
        KeypadDecimal, KeypadDivide, KeypadMultiply, KeypadSubtract, KeypadAdd, KeypadEnter, KeypadEqual,

        Count
    };

    enum xgKeyMod : uint16_t
    {
        xgKeyModNone  = 0,
        xgKeyModCtrl  = 1 << 0,
        xgKeyModShift = 1 << 1,
        xgKeyModAlt   = 1 << 2,
        xgKeyModSuper = 1 << 3,
    };

    //
    // Mouse button enum
    //
    enum class xgMouseButton : uint8_t
    {
        Left = 1,
        Right,
        Middle,
        X1,
        X2
    };

    //
    // Payload structs
    //
    struct xgWindowResizeEvent { int Width, Height; };

    struct xgKeyEvent
    {
        int Key;
        int Scancode;
        uint16_t Mod;
        bool Repeat;
        xgKey Code;
        uint16_t Mods;
    };

    struct xgTextInputEvent { char Text[32]; };

    struct xgMouseMoveEvent { int X, Y; };

    struct xgMouseButtonEvent
    {
        xgMouseButton Button;
        int X;
        int Y;
    };

    struct xgMouseWheelEvent { float X, Y; };

    struct xgGamepadConnectionEvent { int DeviceId; };

    struct xgGamepadButtonEvent { int DeviceId; int Button; };

    struct xgGamepadAxisEvent { int DeviceId; int Axis; float Value; };

    //
    // Main event union
    //
    struct xgEvent
    {
        xgEventType Type = xgEventType::None;

        union
        {
            xgWindowResizeEvent      WindowResize;
            xgKeyEvent               Key;
            xgTextInputEvent         TextInput;
            xgMouseMoveEvent         MouseMove;
            xgMouseButtonEvent       MouseButton;
            xgMouseWheelEvent        MouseWheel;
            xgGamepadConnectionEvent GamepadConnection;
            xgGamepadButtonEvent     GamepadButton;
            xgGamepadAxisEvent       GamepadAxis;
        };

        const void* Native = nullptr;

        // Window that produced the event (main or secondary). Null for app-level events such as Quit.
        class Window* Source = nullptr;
    };

} // namespace xg
