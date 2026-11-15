// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.
#include "pch.h"
#include "FileWin.h"
#include "FilePosix.h"

namespace xg
{
#if XG_PLATFORM_WINDOWS
    using PlatformFile = FileWin;
#else
    using PlatformFile = FilePosix;
#endif

    XG_DECLARE_UNIQUE_TYPE(PlatformFile)
    FileUnique LoadFile(const char* path, FileAccessMode mode)
    {
        // Create raw object
        auto* raw = new PlatformFile(path);

        // Try opening
        if (!raw->Open(mode))
        {
            delete raw;
            return FileUnique(XG_EMPTY_UNIQUE_HANDLE());
        }

        // Wrap into ABI-stable unique handle
        return FileUnique(MakePlatformFileUniqueHandle(raw));
    }
}
