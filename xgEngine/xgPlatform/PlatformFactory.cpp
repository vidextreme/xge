// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.
#include "pch.h"
#include "FileWin.h"

namespace xg
{
    XG_DECLARE_UNIQUE_TYPE(FileWin)
    FileUnique LoadFile(const wchar_t* path, FileAccessMode mode)
    {
        // Create raw object
        auto* raw = new FileWin(path);

        // Try opening
        if (!raw->Open(mode))
        {
            delete raw;
            return FileUnique(XG_EMPTY_UNIQUE_HANDLE());
        }

        // Wrap into ABI-stable unique handle
        return FileUnique(MakeFileWinUniqueHandle(raw));
    }
}
