// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.
#pragma once

#define XGE_VERSION "0.1.0-dev"

#if defined(_DEBUG) || !defined(NDEBUG)
#define XGE_BUILD_CONFIG "Debug"
#else
#define XGE_BUILD_CONFIG "Release"
#endif
