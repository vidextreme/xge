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
// Export / import macro
//
#if XG_PLATFORM_WINDOWS
#ifdef XG_EXPORTS
#define XG_API __declspec(dllexport)
#else
#define XG_API __declspec(dllimport)
#endif
#else
#define XG_API
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

//
// Assertions
//
#if XG_DEBUG
#include <cassert>
#define XG_ASSERT(expr) assert(expr)
#else
#define XG_ASSERT(expr) ((void)0)
#endif
