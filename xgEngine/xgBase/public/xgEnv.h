// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.
#pragma once
#include <cstddef>
#include <cstdlib>
#include <cstring>

namespace xg
{
    // Copies an environment variable into buffer (UTF-8, null-terminated). Returns false when it isn't set or
    // doesn't fit, leaving buffer empty.
    inline bool GetEnv(const char* name, char* buffer, size_t capacity)
    {
        if (!buffer || capacity == 0)
            return false;
        buffer[0] = '\0';
#if defined(_WIN32)
        size_t length = 0;
        if (getenv_s(&length, nullptr, 0, name) != 0 || length == 0)
            return false;
        if (length > capacity)
            return false;
        return getenv_s(&length, buffer, capacity, name) == 0;
#else
        const char* value = std::getenv(name);
        if (!value)
            return false;
        const size_t length = std::strlen(value);
        if (length >= capacity)
            return false;
        std::memcpy(buffer, value, length + 1);
        return true;
#endif
    }

    // True when an environment variable is set to exactly value.
    inline bool EnvEquals(const char* name, const char* value)
    {
        char buffer[256];
        return GetEnv(name, buffer, sizeof(buffer)) && std::strcmp(buffer, value) == 0;
    }
}
