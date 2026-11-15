// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.
#pragma once
#include "hostfxr.h"
#include <filesystem>
#include <string>

namespace xg::coreclr
{
    // hostfxr's native string: UTF-16 on Windows, UTF-8 elsewhere.
    using HostString = std::basic_string<char_t>;

    HostString ToHostString(const std::string& utf8);

    std::filesystem::path PathFromUtf8(const std::string& utf8);
    std::string PathToUtf8(const std::filesystem::path& path);

    // Absolute, normalized UTF-8 path.
    std::string GetFullPath(const char* path);

    // Full path of the hostfxr library: the runtime bundled under engineRoot on Windows, the installed dotnet elsewhere.
    std::string ResolveHostfxrPath(const char* engineRoot);
}
