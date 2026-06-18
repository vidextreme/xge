#pragma once
#include "xgEvent.h"
#include <type_traits>
#include <cstdio> // for snprintf

namespace xg
{
    //
    // Is<T>(event)
    //   Returns true if the event payload matches the requested type T
    //
    template<typename T>
    inline bool Is(const xgEvent& e)
    {
        if constexpr (std::is_same_v<T, xgWindowResizeEvent>)
            return e.Type == xgEventType::WindowResize;

        if constexpr (std::is_same_v<T, xgKeyEvent>)
            return e.Type == xgEventType::KeyDown ||
            e.Type == xgEventType::KeyUp;

        if constexpr (std::is_same_v<T, xgTextInputEvent>)
            return e.Type == xgEventType::TextInput;

        if constexpr (std::is_same_v<T, xgMouseMoveEvent>)
            return e.Type == xgEventType::MouseMove;

        if constexpr (std::is_same_v<T, xgMouseButtonEvent>)
            return e.Type == xgEventType::MouseButtonDown ||
            e.Type == xgEventType::MouseButtonUp;

        if constexpr (std::is_same_v<T, xgMouseWheelEvent>)
            return e.Type == xgEventType::MouseWheel;

        if constexpr (std::is_same_v<T, xgGamepadConnectionEvent>)
            return e.Type == xgEventType::GamepadConnected ||
            e.Type == xgEventType::GamepadDisconnected;

        if constexpr (std::is_same_v<T, xgGamepadButtonEvent>)
            return e.Type == xgEventType::GamepadButtonDown ||
            e.Type == xgEventType::GamepadButtonUp;

        if constexpr (std::is_same_v<T, xgGamepadAxisEvent>)
            return e.Type == xgEventType::GamepadAxisMotion;

        return false;
    }

    //
    // As<T>(event)
    //   Returns a reference to the payload struct T inside the union
    //   Assumes Is<T>(event) is true
    //
    template<typename T>
    inline T& As(xgEvent& e)
    {
        return *reinterpret_cast<T*>(&e);
    }

    template<typename T>
    inline const T& As(const xgEvent& e)
    {
        return *reinterpret_cast<const T*>(&e);
    }

    //
    // Make<T>(type, args...)
    //   Helper to construct an event with payload T
    //
    template<typename T, typename... Args>
    inline xgEvent Make(xgEventType type, Args&&... args)
    {
        xgEvent e{};
        e.Type = type;
        *reinterpret_cast<T*>(&e) = T{ std::forward<Args>(args)... };
        return e;
    }

    //
    // Convenience constructors for common events
    //

    inline xgEvent MakeWindowResize(int w, int h)
    {
        return Make<xgWindowResizeEvent>(xgEventType::WindowResize, w, h);
    }

    inline xgEvent MakeQuit()
    {
        return Make<xgWindowResizeEvent>(xgEventType::Quit);
    }

    inline xgEvent MakeWindowMinimized()
    {
        return Make<xgWindowResizeEvent>(xgEventType::WindowMinimized);
    }

    inline xgEvent MakeWindowRestored()
    {
        return Make<xgWindowResizeEvent>(xgEventType::WindowRestored);
    }

    inline xgEvent MakeWindowFocusGained()
    {
        return Make<xgWindowResizeEvent>(xgEventType::WindowFocusGained);
    }

    inline xgEvent MakeWindowFocusLost()
    {
        return Make<xgWindowResizeEvent>(xgEventType::WindowFocusLost);
    }

    inline xgEvent MakeKeyDown(int key, bool repeat)
    {
        return Make<xgKeyEvent>(xgEventType::KeyDown, key, repeat);
    }

    inline xgEvent MakeKeyUp(int key, bool repeat)
    {
        return Make<xgKeyEvent>(xgEventType::KeyUp, key, repeat);
    }

    inline xgEvent MakeTextInput(const char* text)
    {
        xgEvent e{};
        e.Type = xgEventType::TextInput;
        std::snprintf(e.TextInput.Text, sizeof(e.TextInput.Text), "%s", text);
        return e;
    }

    inline xgEvent MakeMouseMove(int x, int y)
    {
        return Make<xgMouseMoveEvent>(xgEventType::MouseMove, x, y);
    }

    inline xgEvent MakeMouseButtonDown(xgMouseButton btn)
    {
        return Make<xgMouseButtonEvent>(xgEventType::MouseButtonDown, btn);
    }

    inline xgEvent MakeMouseButtonUp(xgMouseButton btn)
    {
        return Make<xgMouseButtonEvent>(xgEventType::MouseButtonUp, btn);
    }

    inline xgEvent MakeMouseWheel(float x, float y)
    {
        return Make<xgMouseWheelEvent>(xgEventType::MouseWheel, x, y);
    }

    inline xgEvent MakeGamepadConnected(int id)
    {
        return Make<xgGamepadConnectionEvent>(xgEventType::GamepadConnected, id);
    }

    inline xgEvent MakeGamepadDisconnected(int id)
    {
        return Make<xgGamepadConnectionEvent>(xgEventType::GamepadDisconnected, id);
    }

    inline xgEvent MakeGamepadButtonDown(int id, int button)
    {
        return Make<xgGamepadButtonEvent>(xgEventType::GamepadButtonDown, id, button);
    }

    inline xgEvent MakeGamepadButtonUp(int id, int button)
    {
        return Make<xgGamepadButtonEvent>(xgEventType::GamepadButtonUp, id, button);
    }

    inline xgEvent MakeGamepadAxis(int id, int axis, float value)
    {
        return Make<xgGamepadAxisEvent>(xgEventType::GamepadAxisMotion, id, axis, value);
    }

} // namespace xg
