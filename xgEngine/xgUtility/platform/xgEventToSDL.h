#pragma once
#include <SDL3/SDL.h>
#include "xgEvent.h"

namespace xg
{
    inline SDL_Event ToSDL(const xgEvent& e, SDL_Window* window)
    {
        SDL_Event sdl{};
        SDL_zero(sdl);

        Uint32 windowID = window ? SDL_GetWindowID(window) : 0;
        Uint64 timestamp = SDL_GetTicksNS();

        switch (e.Type)
        {
        case xgEventType::Quit:
            sdl.type = SDL_EVENT_QUIT;
            break;

        case xgEventType::WindowResize:
            sdl.type = SDL_EVENT_WINDOW_RESIZED;
            sdl.window.windowID = windowID;
            sdl.window.data1 = e.WindowResize.Width;
            sdl.window.data2 = e.WindowResize.Height;
            break;

        case xgEventType::WindowMinimized:
            sdl.type = SDL_EVENT_WINDOW_MINIMIZED;
            sdl.window.windowID = windowID;
            break;

        case xgEventType::WindowRestored:
            sdl.type = SDL_EVENT_WINDOW_RESTORED;
            sdl.window.windowID = windowID;
            break;

        case xgEventType::WindowFocusGained:
            sdl.type = SDL_EVENT_WINDOW_FOCUS_GAINED;
            sdl.window.windowID = windowID;
            break;

        case xgEventType::WindowFocusLost:
            sdl.type = SDL_EVENT_WINDOW_FOCUS_LOST;
            sdl.window.windowID = windowID;
            break;

        case xgEventType::KeyDown:
            sdl.type = SDL_EVENT_KEY_DOWN;
            sdl.key.windowID = windowID;
            sdl.key.key = e.Key.Key;
            sdl.key.repeat = e.Key.Repeat ? 1 : 0;
            sdl.key.timestamp = timestamp;
            break;

        case xgEventType::KeyUp:
            sdl.type = SDL_EVENT_KEY_UP;
            sdl.key.windowID = windowID;
            sdl.key.key = e.Key.Key;
            sdl.key.repeat = e.Key.Repeat ? 1 : 0;
            sdl.key.timestamp = timestamp;
            break;

        case xgEventType::TextInput:
            sdl.type = SDL_EVENT_TEXT_INPUT;
            sdl.text.windowID = windowID;
            sdl.text.text = e.TextInput.Text;
            break;

        case xgEventType::MouseMove:
            sdl.type = SDL_EVENT_MOUSE_MOTION;
            sdl.motion.windowID = windowID;
            sdl.motion.x = e.MouseMove.X;
            sdl.motion.y = e.MouseMove.Y;
            sdl.motion.timestamp = timestamp;
            break;

        case xgEventType::MouseButtonDown:
            sdl.type = SDL_EVENT_MOUSE_BUTTON_DOWN;
            sdl.button.windowID = windowID;
            sdl.button.button = static_cast<uint8_t>(e.MouseButton.Button);
            sdl.button.clicks = 1;
            sdl.button.x = e.MouseButton.X;
            sdl.button.y = e.MouseButton.Y;
            sdl.button.timestamp = timestamp;
            break;

        case xgEventType::MouseButtonUp:
            sdl.type = SDL_EVENT_MOUSE_BUTTON_UP;
            sdl.button.windowID = windowID;
            sdl.button.button = static_cast<uint8_t>(e.MouseButton.Button);
            sdl.button.clicks = 1;
            sdl.button.x = e.MouseButton.X;
            sdl.button.y = e.MouseButton.Y;
            sdl.button.timestamp = timestamp;
            break;

        case xgEventType::MouseWheel:
            sdl.type = SDL_EVENT_MOUSE_WHEEL;
            sdl.wheel.windowID = windowID;
            sdl.wheel.x = e.MouseWheel.X;
            sdl.wheel.y = e.MouseWheel.Y;
            sdl.wheel.timestamp = timestamp;
            break;

        case xgEventType::GamepadConnected:
            sdl.type = SDL_EVENT_GAMEPAD_ADDED;
            sdl.gdevice.which = e.GamepadConnection.DeviceId;
            break;

        case xgEventType::GamepadDisconnected:
            sdl.type = SDL_EVENT_GAMEPAD_REMOVED;
            sdl.gdevice.which = e.GamepadConnection.DeviceId;
            break;

        case xgEventType::GamepadButtonDown:
            sdl.type = SDL_EVENT_GAMEPAD_BUTTON_DOWN;
            sdl.gbutton.which = e.GamepadButton.DeviceId;
            sdl.gbutton.button = e.GamepadButton.Button;
            break;

        case xgEventType::GamepadButtonUp:
            sdl.type = SDL_EVENT_GAMEPAD_BUTTON_UP;
            sdl.gbutton.which = e.GamepadButton.DeviceId;
            sdl.gbutton.button = e.GamepadButton.Button;
            break;

        case xgEventType::GamepadAxisMotion:
            sdl.type = SDL_EVENT_GAMEPAD_AXIS_MOTION;
            sdl.gaxis.which = e.GamepadAxis.DeviceId;
            sdl.gaxis.axis = e.GamepadAxis.Axis;
            sdl.gaxis.value = e.GamepadAxis.Value;
            break;

        default:
            break;
        }

        return sdl;
    }
}
