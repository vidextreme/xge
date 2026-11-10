// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.
#pragma once
#include <SDL3/SDL.h>
#include <cstring>
#include "xgEvent.h"

namespace xg
{
    inline xgKey ToXgKey(SDL_Keycode key, SDL_Scancode scancode)
    {
        switch (scancode)
        {
        case SDL_SCANCODE_KP_0: return xgKey::Keypad0;
        case SDL_SCANCODE_KP_1: return xgKey::Keypad1;
        case SDL_SCANCODE_KP_2: return xgKey::Keypad2;
        case SDL_SCANCODE_KP_3: return xgKey::Keypad3;
        case SDL_SCANCODE_KP_4: return xgKey::Keypad4;
        case SDL_SCANCODE_KP_5: return xgKey::Keypad5;
        case SDL_SCANCODE_KP_6: return xgKey::Keypad6;
        case SDL_SCANCODE_KP_7: return xgKey::Keypad7;
        case SDL_SCANCODE_KP_8: return xgKey::Keypad8;
        case SDL_SCANCODE_KP_9: return xgKey::Keypad9;
        case SDL_SCANCODE_KP_PERIOD: return xgKey::KeypadDecimal;
        case SDL_SCANCODE_KP_DIVIDE: return xgKey::KeypadDivide;
        case SDL_SCANCODE_KP_MULTIPLY: return xgKey::KeypadMultiply;
        case SDL_SCANCODE_KP_MINUS: return xgKey::KeypadSubtract;
        case SDL_SCANCODE_KP_PLUS: return xgKey::KeypadAdd;
        case SDL_SCANCODE_KP_ENTER: return xgKey::KeypadEnter;
        case SDL_SCANCODE_KP_EQUALS: return xgKey::KeypadEqual;
        default: break;
        }

        if (key >= SDLK_A && key <= SDLK_Z)
            return static_cast<xgKey>(static_cast<int>(xgKey::A) + static_cast<int>(key - SDLK_A));
        if (key >= SDLK_0 && key <= SDLK_9)
            return static_cast<xgKey>(static_cast<int>(xgKey::Num0) + static_cast<int>(key - SDLK_0));
        if (key >= SDLK_F1 && key <= SDLK_F12)
            return static_cast<xgKey>(static_cast<int>(xgKey::F1) + static_cast<int>(key - SDLK_F1));

        switch (key)
        {
        case SDLK_TAB: return xgKey::Tab;
        case SDLK_LEFT: return xgKey::Left;
        case SDLK_RIGHT: return xgKey::Right;
        case SDLK_UP: return xgKey::Up;
        case SDLK_DOWN: return xgKey::Down;
        case SDLK_PAGEUP: return xgKey::PageUp;
        case SDLK_PAGEDOWN: return xgKey::PageDown;
        case SDLK_HOME: return xgKey::Home;
        case SDLK_END: return xgKey::End;
        case SDLK_INSERT: return xgKey::Insert;
        case SDLK_DELETE: return xgKey::Delete;
        case SDLK_BACKSPACE: return xgKey::Backspace;
        case SDLK_SPACE: return xgKey::Space;
        case SDLK_RETURN: return xgKey::Enter;
        case SDLK_ESCAPE: return xgKey::Escape;
        case SDLK_LCTRL: return xgKey::LeftCtrl;
        case SDLK_LSHIFT: return xgKey::LeftShift;
        case SDLK_LALT: return xgKey::LeftAlt;
        case SDLK_LGUI: return xgKey::LeftSuper;
        case SDLK_RCTRL: return xgKey::RightCtrl;
        case SDLK_RSHIFT: return xgKey::RightShift;
        case SDLK_RALT: return xgKey::RightAlt;
        case SDLK_RGUI: return xgKey::RightSuper;
        case SDLK_APPLICATION: return xgKey::Menu;
        case SDLK_APOSTROPHE: return xgKey::Apostrophe;
        case SDLK_COMMA: return xgKey::Comma;
        case SDLK_MINUS: return xgKey::Minus;
        case SDLK_PERIOD: return xgKey::Period;
        case SDLK_SLASH: return xgKey::Slash;
        case SDLK_SEMICOLON: return xgKey::Semicolon;
        case SDLK_EQUALS: return xgKey::Equal;
        case SDLK_LEFTBRACKET: return xgKey::LeftBracket;
        case SDLK_BACKSLASH: return xgKey::Backslash;
        case SDLK_RIGHTBRACKET: return xgKey::RightBracket;
        case SDLK_GRAVE: return xgKey::GraveAccent;
        case SDLK_CAPSLOCK: return xgKey::CapsLock;
        case SDLK_SCROLLLOCK: return xgKey::ScrollLock;
        case SDLK_NUMLOCKCLEAR: return xgKey::NumLock;
        case SDLK_PRINTSCREEN: return xgKey::PrintScreen;
        case SDLK_PAUSE: return xgKey::Pause;
        default: return xgKey::None;
        }
    }

    inline uint16_t ToXgKeyMods(SDL_Keymod mod)
    {
        uint16_t mods = xgKeyModNone;
        if (mod & SDL_KMOD_CTRL) mods |= xgKeyModCtrl;
        if (mod & SDL_KMOD_SHIFT) mods |= xgKeyModShift;
        if (mod & SDL_KMOD_ALT) mods |= xgKeyModAlt;
        if (mod & SDL_KMOD_GUI) mods |= xgKeyModSuper;
        return mods;
    }

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

        case SDL_EVENT_WINDOW_MOUSE_LEAVE:
            e.Type = xgEventType::MouseLeave;
            break;

        case SDL_EVENT_WINDOW_MOVED:
            e.Type = xgEventType::WindowMoved;
            e.MouseMove.X = sdl.window.data1;
            e.MouseMove.Y = sdl.window.data2;
            break;

        case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
            e.Type = xgEventType::WindowCloseRequested;
            break;

        case SDL_EVENT_KEY_DOWN:
            e.Type = xgEventType::KeyDown;
            e.Key.Key = static_cast<int>(sdl.key.key);
            e.Key.Scancode = static_cast<int>(sdl.key.scancode);
            e.Key.Mod = static_cast<uint16_t>(sdl.key.mod);
            e.Key.Repeat = sdl.key.repeat != 0;
            e.Key.Code = ToXgKey(sdl.key.key, sdl.key.scancode);
            e.Key.Mods = ToXgKeyMods(sdl.key.mod);
            break;

        case SDL_EVENT_KEY_UP:
            e.Type = xgEventType::KeyUp;
            e.Key.Key = static_cast<int>(sdl.key.key);
            e.Key.Scancode = static_cast<int>(sdl.key.scancode);
            e.Key.Mod = static_cast<uint16_t>(sdl.key.mod);
            e.Key.Repeat = sdl.key.repeat != 0;
            e.Key.Code = ToXgKey(sdl.key.key, sdl.key.scancode);
            e.Key.Mods = ToXgKeyMods(sdl.key.mod);
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
            e.MouseMove.X = static_cast<int>(sdl.motion.x);
            e.MouseMove.Y = static_cast<int>(sdl.motion.y);
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
            e.MouseButton.X = static_cast<int>(sdl.button.x);
            e.MouseButton.Y = static_cast<int>(sdl.button.y);
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
