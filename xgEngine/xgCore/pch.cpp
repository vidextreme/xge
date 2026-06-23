// pch.cpp: source file corresponding to the pre-compiled header

#include "pch.h"

// When you are using pre-compiled headers, this source file is necessary for compilation to succeed.
#pragma clang diagnostic ignored "-Wunknown-pragmas"
#pragma warning(push)
#pragma warning(disable: 4068)

#pragma message("c C++20 FOR PCH")
#if !defined(_MSVC_LANG) || _MSVC_LANG < 202002L
#error "PCH __cplusplus = " STR(__cplusplus)
#endif

#pragma warning(pop)

