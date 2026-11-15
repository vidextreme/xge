// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.
#pragma once
#include "xgBase.h"

#if XG_PLATFORM_WINDOWS
#include <Windows.h>
#else
#include <dlfcn.h>
#include <strings.h>
#include <climits>
#include <cstdio>
#include <cstring>
#endif

namespace xg {

    using ModuleHandle = void*;

#if XG_PLATFORM_WINDOWS

    inline ModuleHandle LoadModule(const char* path) {
        return reinterpret_cast<ModuleHandle>(LoadLibraryA(path));
    }

    inline void UnloadModule(ModuleHandle module) {
        FreeLibrary(reinterpret_cast<HMODULE>(module));
    }

    inline void* GetSymbol(ModuleHandle module, const char* name) {
        return reinterpret_cast<void*>(GetProcAddress(reinterpret_cast<HMODULE>(module), name));
    }

#else

    // "name.dll" and "name" become "./name.so" (or .dylib): module names stay Windows-style in configs, and a
    // bare file name resolves against the working directory, as LoadLibrary does. Returns false when the result
    // doesn't fit in output.
    inline bool ToPlatformModulePath(const char* path, char* output, size_t capacity) {
        if (!path || !*path || !output || capacity == 0)
            return false;
        constexpr size_t dllLength = 4;
        const char* slash = std::strrchr(path, '/');
        const char* name = slash ? slash + 1 : path;
        const size_t pathLength = std::strlen(path);
        const size_t nameLength = std::strlen(name);
        const bool hasDll = nameLength > dllLength && strcasecmp(path + pathLength - dllLength, ".dll") == 0;
        const size_t stemLength = hasDll ? pathLength - dllLength : pathLength;
        const char* extension = (hasDll || !std::strchr(name, '.')) ? XG_MODULE_EXTENSION : "";
        const int written = std::snprintf(output, capacity, "%s%.*s%s", slash ? "" : "./", static_cast<int>(stemLength), path, extension);
        return written > 0 && static_cast<size_t>(written) < capacity;
    }

    inline ModuleHandle LoadModule(const char* path) {
        char platformPath[PATH_MAX];
        if (!ToPlatformModulePath(path, platformPath, sizeof(platformPath)))
            return nullptr;
        ModuleHandle module = dlopen(platformPath, RTLD_NOW);
        if (!module)
            fprintf(stderr, "dlopen %s: %s\n", platformPath, dlerror());
        return module;
    }

    inline void UnloadModule(ModuleHandle module) {
        dlclose(module);
    }

    inline void* GetSymbol(ModuleHandle module, const char* name) {
        return dlsym(module, name);
    }

#endif

} // namespace xg
