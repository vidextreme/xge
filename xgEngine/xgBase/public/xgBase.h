// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.
#pragma once

namespace xg
{
    constexpr uint32_t UnassignedID = 0;
}
//
// Platform detection
//
#if defined(_WIN32) || defined(_WIN64)
#define XG_PLATFORM_WINDOWS 1
#else
#define XG_PLATFORM_WINDOWS 0
#endif

#if defined(__linux__)
#define XG_PLATFORM_LINUX 1
#else
#define XG_PLATFORM_LINUX 0
#endif

#if defined(__APPLE__)
#define XG_PLATFORM_MACOS 1
#else
#define XG_PLATFORM_MACOS 0
#endif

//
// Build configuration
//
#if defined(_DEBUG) || defined(DEBUG)
#define XG_DEBUG 1
#else
#define XG_DEBUG 0
#endif

//
// API macro
// xgCore = static library → NO dllimport/dllexport
// Renderer DLLs define XG_EXPORTS → dllexport
//
#if XG_PLATFORM_WINDOWS
#ifdef XG_EXPORTS
#define XG_API __declspec(dllexport)
#else
#define XG_API
#endif
#else
#ifdef XG_EXPORTS
#define XG_API __attribute__((visibility("default")))
#else
#define XG_API
#endif
#endif

//
// Utility macros
//
#define XG_UNUSED(x) (void)(x)

#if defined(_MSC_VER)
#define XG_FORCEINLINE __forceinline
#else
#define XG_FORCEINLINE inline __attribute__((always_inline))
#endif

#if XG_DEBUG
#include <cassert>
#define XG_ASSERT(expr) assert(expr)
#else
#define XG_ASSERT(expr) ((void)0)
#endif


#define XG_STRINGIFY2(x) #x
#define XG_STRINGIFY(x) XG_STRINGIFY2(x)

#define XG_ENABLE_FLAGS(E) \
inline E operator|(E a, E b) { \
    return static_cast<E>(static_cast<uint8_t>(a) | static_cast<uint8_t>(b)); \
} \
inline E operator&(E a, E b) { \
    return static_cast<E>(static_cast<uint8_t>(a) & static_cast<uint8_t>(b)); \
} \
inline bool HasFlag(E value, E flag) { \
    return (static_cast<uint8_t>(value) & static_cast<uint8_t>(flag)) == static_cast<uint8_t>(flag); \
}

// Emits a comment Tree-sitter can detect
#define XG_SERIALIZABLE() /* @xg:serializable */

// Emits a comment with the field name
#define XG_FIELD(...)    /* @xg:field __VA_ARGS__ */
#define XG_ENUM(...)    /* @xg:field __VA_ARGS__ */

//
// Refcount interface (abstract)
//
#define XG_ABSTRACT_REFCOUNTED() \
    virtual int32_t AddRef() = 0; \
    virtual int32_t Release() = 0; \
    virtual int32_t GetRefCount() const = 0;

//
// Refcount implementation (starts at 1)
//
#define XG_IMPL_REFCOUNTED() \
    int32_t _refCount = 1; \
    virtual int32_t AddRef() override { return ++_refCount; } \
    virtual int32_t Release() override { \
        int32_t r = --_refCount; \
        if (r <= 0) { delete this; return 0; } \
        return r; \
    } \
    virtual int32_t GetRefCount() const override { return _refCount; }

#define XG_ADDREF(x) \
    (x)->AddRef();

#define XG_RELEASE_ONE(x) \
    do { if (x) { (x)->Release(); } } while(0)

#define XG_SAFE_RELEASE(x) \
    do { if (x) { (x)->Release(); (x) = nullptr; } } while(0)

#define XG_DELETE(x) \
    if (x) { delete x; x = nullptr; }

//
// MODULE DECLARATION MACROS
//

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


//
// PROCEDURE LOOKUP MACROS
//

// Legacy: binds to a variable named 'func'
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


//
// NEW: Named procedure lookup macro
// Allows binding multiple functions in the same scope without collisions
// Works with ANY function signature (1, 2, 3, N parameters)
//
#define XG_MODULE_PROCEDURE_NAMED(NS, MODULENAME, FUNCNAME, LOCALNAME) \
    FUNCNAME##Func LOCALNAME = nullptr; \
    if (NS::MODULENAME##Procs) { \
        auto it = NS::MODULENAME##Procs->find(#FUNCNAME); \
        if (it != NS::MODULENAME##Procs->end()) \
            LOCALNAME = (FUNCNAME##Func)it->second; \
    } \
    if (!LOCALNAME) { \
        LOCALNAME = (FUNCNAME##Func)xg::GetSymbol(NS::MODULENAME##Lib, #FUNCNAME); \
        if (LOCALNAME) { \
            if (!NS::MODULENAME##Procs) \
                NS::MODULENAME##Procs = new std::map<std::string, void*>(); \
            (*NS::MODULENAME##Procs)[#FUNCNAME] = (void*)LOCALNAME; \
        } \
    }

#define XG_MODULE_CALL_NAMED(LOCALNAME) LOCALNAME


//
// MODULE LOADER MACRO
//
#define XG_DEFINE_MODULE_LOADER(MODULE_PREFIX, MODULE_NAME) \
    static inline void Ensure##MODULE_NAME##Loaded() { \
        if ((MODULE_PREFIX::MODULE_NAME##DLL) == nullptr) { \
            xg::Log(xg::MessageType::Error," " #MODULE_NAME " DLL name is null. Engine must set it before use."); \
            return; \
        } \
        if ((MODULE_PREFIX::MODULE_NAME##Lib) == nullptr) { \
            xg::Log(xg::MessageType::Info,"Loading " #MODULE_NAME " DLL: %s", (MODULE_PREFIX::MODULE_NAME##DLL)); \
            (MODULE_PREFIX::Load##MODULE_NAME##Lib)(); \
            if ((MODULE_PREFIX::MODULE_NAME##Lib) == nullptr) { \
                xg::Log(xg::MessageType::Error,"FAILED to load " #MODULE_NAME " DLL: %s", (MODULE_PREFIX::MODULE_NAME##DLL)); \
                return; \
            } \
            xg::Log(xg::MessageType::Success, "Loaded " #MODULE_NAME " DLL: %s", (MODULE_PREFIX::MODULE_NAME##DLL)); \
        } \
    }

#define XG_MODULE_EXPORT extern "C" XG_API

#if XG_PLATFORM_WINDOWS
#include <combaseapi.h>
#define XG_MANAGED_ALLOC(size) CoTaskMemAlloc(size)
#define XG_MANAGED_FREE(ptr)   CoTaskMemFree(ptr)
#else
#include <cstdlib>
#define XG_MANAGED_ALLOC(size) malloc(size)
#define XG_MANAGED_FREE(ptr)   free(ptr)
#endif


using TypeID = std::uint64_t;

// FNV-1a constexpr hash
constexpr std::uint64_t fnv1a(const char* str) {
    std::uint64_t hash = 1469598103934665603ull;
    while (*str) {
        hash ^= static_cast<std::uint64_t>(*str++);
        hash *= 1099511628211ull;
    }
    return hash;
}

#define XG_DECLARE_SYSTEM(Type, Parent)                     \
public:\
    static constexpr const char* TypeName = #Type;          \
    static constexpr const char* ParentTypeName = #Parent;  \
    static constexpr TypeID SuperTypeID = fnv1a(#Parent);


#define XG_DECLARE_BASE_SYSTEM(Type)                     \
public:\
    static constexpr const char* TypeName = #Type;          \
    static constexpr TypeID SuperTypeID = fnv1a(#Type);

