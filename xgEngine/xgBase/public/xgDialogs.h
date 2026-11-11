// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.
#pragma once
#include "xgBase.h"

namespace xg {

    class Window;

    // path is the chosen UTF-8 path, or null when the dialog was cancelled or failed.
    // Can be called on another thread, after the Show call returns.
    using DialogCallback = void (*)(void* user, const char* path);

    // Implemented in xgPlatform. The dialogs are modal to parent when it is not null.
    XG_API void ShowOpenFolderDialog(Window* parent, const char* defaultLocation, DialogCallback callback, void* user);

    // filterPattern lists extensions separated by ';' without dots ("png;jpg"); null or empty shows all files.
    XG_API void ShowOpenFileDialog(Window* parent, const char* filterName, const char* filterPattern,
        const char* defaultLocation, DialogCallback callback, void* user);

} // namespace xg
