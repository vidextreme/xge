// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.
#pragma once
#include <cstddef>

namespace xg
{
    // UTF-8 path of the running executable. Returns its length, or 0 when it doesn't fit or can't be read.
    size_t GetExecutablePath(char* output, size_t capacity);

    const char* GetExtension(const char* path);

    const char* GetFilename(const char* path);

    const char* GetDirectory(const char* path);
}
