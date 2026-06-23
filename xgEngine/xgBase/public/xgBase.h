#pragma once

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
