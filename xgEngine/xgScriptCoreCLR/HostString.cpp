// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.
#include "pch.h"
#include "HostString.h"
#include <filesystem>

#if XG_PLATFORM_WINDOWS
#define XG_CORECLR_HOSTFXR_PATH "\\coreclr\\host\\fxr\\10.0.8\\hostfxr.dll"
#else
#include <nethost.h>
#endif

namespace xg::coreclr
{
    HostString ToHostString(const std::string& utf8)
    {
#if XG_PLATFORM_WINDOWS
        if (utf8.empty())
            return HostString();
        const int length = ::MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), static_cast<int>(utf8.size()), nullptr, 0);
        if (length <= 0)
            return HostString();
        HostString wide(static_cast<size_t>(length), L'\0');
        ::MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), static_cast<int>(utf8.size()), wide.data(), length);
        return wide;
#else
        return utf8;
#endif
    }

    std::filesystem::path PathFromUtf8(const std::string& utf8)
    {
        return std::filesystem::path(std::u8string(utf8.begin(), utf8.end()));
    }

    std::string PathToUtf8(const std::filesystem::path& path)
    {
        const std::u8string text = path.u8string();
        return std::string(text.begin(), text.end());
    }

    std::string GetFullPath(const char* path)
    {
        if (!path)
            return std::string();
        std::error_code error;
        const std::filesystem::path full = std::filesystem::absolute(PathFromUtf8(path), error);
        if (error)
            return std::string(path);
        return PathToUtf8(full.lexically_normal());
    }

    std::string ResolveHostfxrPath(const char* engineRoot)
    {
#if XG_PLATFORM_WINDOWS
        return std::string(engineRoot ? engineRoot : "") + XG_CORECLR_HOSTFXR_PATH;
#else
        XG_UNUSED(engineRoot);
        char_t buffer[4096];
        size_t size = sizeof(buffer) / sizeof(char_t);
        if (get_hostfxr_path(buffer, &size, nullptr) != 0)
            return std::string();
        return std::string(buffer);
#endif
    }
}
