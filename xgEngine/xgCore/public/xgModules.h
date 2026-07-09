// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.
#pragma once
#include "xgBase.h"

#if XG_PLATFORM_WINDOWS
#include <Windows.h>
#else
#include <dlfcn.h>
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

    inline ModuleHandle LoadModule(const char* path) {
        return dlopen(path, RTLD_NOW);
    }

    inline void UnloadModule(ModuleHandle module) {
        dlclose(module);
    }

    inline void* GetSymbol(ModuleHandle module, const char* name) {
        return dlsym(module, name);
    }

#endif

} // namespace xg
