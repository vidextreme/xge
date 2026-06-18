#pragma once
#include <SDL3/SDL.h>
#include <cstring>
#include "xgEvent.h"

namespace xg
{
    inline xgEvent FromSDL(const SDL_Event& sdl)
    {
        xgEvent e{};
        e.Type = xgEventType::None;

        switch (sdl.type)
        {
        case SDL_EVENT_QUIT:
            e.Type = xgEventType::Quit;
            break;

        case SDL_EVENT_WINDOW_RESIZED:
            e.Type = xgEventType::WindowResize;
            e.WindowResize.Width = sdl.window.data1;
            e.WindowResize.Height = sdl.window.data2;
            break;

        case SDL_EVENT_WINDOW_MINIMIZED:
            e.Type = xgEventType::WindowMinimized;
            break;

        case SDL_EVENT_WINDOW_RESTORED:
            e.Type = xgEventType::WindowRestored;
            break;

        case SDL_EVENT_WINDOW_FOCUS_GAINED:
            e.Type = xgEventType::WindowFocusGained;
            break;

        case SDL_EVENT_WINDOW_FOCUS_LOST:
            e.Type = xgEventType::WindowFocusLost;
            break;

        case SDL_EVENT_KEY_DOWN:
            e.Type = xgEventType::KeyDown;
            e.Key.Key = sdl.key.key;
            e.Key.Repeat = sdl.key.repeat != 0;
            break;

        case SDL_EVENT_KEY_UP:
            e.Type = xgEventType::KeyUp;
            e.Key.Key = sdl.key.key;
            e.Key.Repeat = sdl.key.repeat != 0;
            break;

        case SDL_EVENT_TEXT_INPUT:
            e.Type = xgEventType::TextInput;
#if defined(_MSC_VER)
            strncpy_s(e.TextInput.Text, sizeof(e.TextInput.Text), sdl.text.text, _TRUNCATE);
#else
            std::strncpy(e.TextInput.Text, sdl.text.text, sizeof(e.TextInput.Text));
            e.TextInput.Text[sizeof(e.TextInput.Text) - 1] = '\0';
#endif
            break;

        case SDL_EVENT_MOUSE_MOTION:
            e.Type = xgEventType::MouseMove;
            e.MouseMove.X = sdl.motion.x;
            e.MouseMove.Y = sdl.motion.y;
            break;

        case SDL_EVENT_MOUSE_BUTTON_DOWN:
            e.Type = xgEventType::MouseButtonDown;
            e.MouseButton.Button = static_cast<xgMouseButton>(sdl.button.button);
            e.MouseButton.X = static_cast<int>(sdl.button.x);
            e.MouseButton.Y = static_cast<int>(sdl.button.y);
            break;


        case SDL_EVENT_MOUSE_BUTTON_UP:
            e.Type = xgEventType::MouseButtonUp;
            e.MouseButton.Button = static_cast<xgMouseButton>(sdl.button.button);
            break;

        case SDL_EVENT_MOUSE_WHEEL:
            e.Type = xgEventType::MouseWheel;
            e.MouseWheel.X = sdl.wheel.x;
            e.MouseWheel.Y = sdl.wheel.y;
            break;

        case SDL_EVENT_GAMEPAD_ADDED:
            e.Type = xgEventType::GamepadConnected;
            e.GamepadConnection.DeviceId = sdl.gdevice.which;
            break;

        case SDL_EVENT_GAMEPAD_REMOVED:
            e.Type = xgEventType::GamepadDisconnected;
            e.GamepadConnection.DeviceId = sdl.gdevice.which;
            break;

        case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
            e.Type = xgEventType::GamepadButtonDown;
            e.GamepadButton.DeviceId = sdl.gbutton.which;
            e.GamepadButton.Button = sdl.gbutton.button;
            break;

        case SDL_EVENT_GAMEPAD_BUTTON_UP:
            e.Type = xgEventType::GamepadButtonUp;
            e.GamepadButton.DeviceId = sdl.gbutton.which;
            e.GamepadButton.Button = sdl.gbutton.button;
            break;

        case SDL_EVENT_GAMEPAD_AXIS_MOTION:
            e.Type = xgEventType::GamepadAxisMotion;
            e.GamepadAxis.DeviceId = sdl.gaxis.which;
            e.GamepadAxis.Axis = sdl.gaxis.axis;
            e.GamepadAxis.Value = sdl.gaxis.value;
            break;

        default:
            break;
        }

        return e;
    }
}
