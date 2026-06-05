#pragma once
#include "xgBase.h"
#include <string>
#include <map>

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

#define XG_DECLARE_MODULE_FUNCTION(FUNCNAME, RETTYPE, ...) \
    using FUNCNAME##Func = RETTYPE (*)(__VA_ARGS__)

#define XG_DECLARE_MODULE(NS, MODULENAME) \
namespace NS { \
    extern const char* MODULENAME##DLL; \
    extern xg::ModuleHandle MODULENAME##Lib; \
    extern std::map<std::string, void*>* MODULENAME##Procs; \
    void Load##MODULENAME##Lib(); \
    void Free##MODULENAME##Lib(); \
}

#define XG_IMPLEMENT_MODULE(NS, MODULENAME) \
namespace NS { \
    const char* MODULENAME##DLL = nullptr; \
    xg::ModuleHandle MODULENAME##Lib = nullptr; \
    std::map<std::string, void*>* MODULENAME##Procs = nullptr; \
    void Load##MODULENAME##Lib() { \
        if (!MODULENAME##Lib) { \
            MODULENAME##Lib = xg::LoadModule(MODULENAME##DLL); \
            if (!MODULENAME##Procs) \
                MODULENAME##Procs = new std::map<std::string, void*>(); \
        } \
    } \
    void Free##MODULENAME##Lib() { \
        if (MODULENAME##Lib) { \
            xg::UnloadModule(MODULENAME##Lib); \
            MODULENAME##Lib = nullptr; \
        } \
        if (MODULENAME##Procs) { \
            MODULENAME##Procs->clear(); \
            delete MODULENAME##Procs; \
            MODULENAME##Procs = nullptr; \
        } \
    } \
}

#define XG_MODULE_PROCEDURE(NS, MODULENAME, FUNCNAME) \
    FUNCNAME##Func func = nullptr; \
    if (NS::MODULENAME##Procs) { \
        auto it = NS::MODULENAME##Procs->find(#FUNCNAME); \
        if (it != NS::MODULENAME##Procs->end()) \
            func = (FUNCNAME##Func)it->second; \
    } \
    if (!func) { \
        func = (FUNCNAME##Func)xg::GetSymbol(NS::MODULENAME##Lib, #FUNCNAME); \
        if (func) { \
            if (!NS::MODULENAME##Procs) \
                NS::MODULENAME##Procs = new std::map<std::string, void*>(); \
            (*NS::MODULENAME##Procs)[#FUNCNAME] = (void*)func; \
        } \
    }

#define XG_MODULE_CALL(FUNCNAME) func
#define XG_MODULE_EXPORT extern "C" XG_API

XG_DECLARE_MODULE(xg, Renderer)
