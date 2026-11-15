# Copyright (c) 2026 John David Uy
# Licensed under the MIT License. See LICENSE for details.
# Mirrors xgPlatform.vcxproj.
xge_target(xgPlatform SHARED
    SOURCES pch.cpp PlatformFactory.cpp WindowFactory.cpp WindowSDL.cpp FileWin.cpp FilePosix.cpp UIRenderTargetSDLGPU.cpp
            ImageSDL.cpp DialogSDL.cpp PlatformServicesSDL.cpp
    INCLUDES ${XGE_ROOT}/xgCore/public ${XGE_ROOT}/xgUtility
    DEFINES XGPLATFORM_EXPORTS
    LINKS xgBase XGE::SDL3)
