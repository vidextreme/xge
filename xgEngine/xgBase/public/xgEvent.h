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

    struct xgKeyEvent { int Key; bool Repeat; };

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
    };

} // namespace xg
