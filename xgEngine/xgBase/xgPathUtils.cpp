// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.
#include "pch.h"
#include "public/xgPathUtils.h"
#include "public/xgBase.h"

#if XG_PLATFORM_WINDOWS
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#elif XG_PLATFORM_MACOS
#include <mach-o/dyld.h>
#include <climits>
#include <cstdlib>
#else
#include <unistd.h>
#endif

namespace xg
{
    size_t GetExecutablePath(char* output, size_t capacity)
    {
        if (!output || capacity == 0)
            return 0;
        output[0] = '\0';
#if XG_PLATFORM_WINDOWS
        std::wstring wide(MAX_PATH, L'\0');
        for (;;)
        {
            const DWORD length = ::GetModuleFileNameW(nullptr, wide.data(), static_cast<DWORD>(wide.size()));
            if (length == 0)
                return 0;
            if (length < wide.size())
            {
                wide.resize(length);
                break;
            }
            wide.resize(wide.size() * 2);
        }
        const int bytes = ::WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), static_cast<int>(wide.size()), nullptr, 0, nullptr, nullptr);
        if (bytes <= 0 || static_cast<size_t>(bytes) >= capacity)
            return 0;
        ::WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), static_cast<int>(wide.size()), output, bytes, nullptr, nullptr);
        output[bytes] = '\0';
        return static_cast<size_t>(bytes);
#elif XG_PLATFORM_MACOS
        uint32_t size = 0;
        _NSGetExecutablePath(nullptr, &size);
        std::string raw(size, '\0');
        if (_NSGetExecutablePath(raw.data(), &size) != 0)
            return 0;
        char resolved[PATH_MAX];
        if (!realpath(raw.c_str(), resolved))
            return 0;
        const size_t length = strlen(resolved);
        if (length >= capacity)
            return 0;
        memcpy(output, resolved, length + 1);
        return length;
#else
        const ssize_t length = readlink("/proc/self/exe", output, capacity - 1);
        if (length <= 0 || static_cast<size_t>(length) >= capacity - 1)
        {
            output[0] = '\0';
            return 0;
        }
        output[length] = '\0';
        return static_cast<size_t>(length);
#endif
    }

    const char* GetExtension(const char* path)
    {
        if (!path)
            return "";

        const char* dot = strrchr(path, '.');
        return dot ? dot : "";
    }

    const char* GetFilename(const char* path)
    {
        if (!path)
            return "";

        const char* slash1 = strrchr(path, '/');
        const char* slash2 = strrchr(path, '\\');
        const char* slash = (slash1 > slash2) ? slash1 : slash2;

        return slash ? slash + 1 : path;
    }

    const char* GetDirectory(const char* path)
    {
        static std::string buffer;

        if (!path)
        {
            buffer.clear();
            return buffer.c_str();
        }

        const char* slash1 = strrchr(path, '/');
        const char* slash2 = strrchr(path, '\\');
        const char* slash = (slash1 > slash2) ? slash1 : slash2;

        if (!slash)
        {
            buffer.clear();
            return buffer.c_str();
        }

        buffer.assign(path, slash - path);
        return buffer.c_str();
    }
}
