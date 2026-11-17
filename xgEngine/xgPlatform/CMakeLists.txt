# Copyright (c) 2026 John David Uy
# Licensed under the MIT License. See LICENSE for details.
# Mirrors xgPlatform.vcxproj. The splash renders its text with its own copy of the trimmed FreeType subset (the same
# list as xgImUIKit); the ftconfig folder comes first on the include path so its ftmodule.h overrides the module list.
set(_freetype ${XGE_ROOT}/3rdparty/freetype)
set(_freetypeSources
    ${_freetype}/src/base/ftbase.c ${_freetype}/src/base/ftinit.c ${_freetype}/src/base/ftsystem.c
    ${_freetype}/src/base/ftdebug.c ${_freetype}/src/base/ftbitmap.c ${_freetype}/src/base/ftglyph.c
    ${_freetype}/src/base/ftsynth.c ${_freetype}/src/base/ftmm.c ${_freetype}/src/sfnt/sfnt.c
    ${_freetype}/src/truetype/truetype.c ${_freetype}/src/smooth/smooth.c ${_freetype}/src/autofit/autofit.c
    ${_freetype}/src/psnames/psnames.c ${_freetype}/src/gzip/ftgzip.c)
set_source_files_properties(${_freetypeSources} PROPERTIES COMPILE_DEFINITIONS "FT2_BUILD_LIBRARY;_CRT_SECURE_NO_WARNINGS")
xge_quiet_sources(${_freetypeSources})

xge_target(xgPlatform SHARED
    SOURCES pch.cpp PlatformFactory.cpp WindowFactory.cpp WindowSDL.cpp FileWin.cpp FilePosix.cpp UIRenderTargetSDLGPU.cpp
            ImageSDL.cpp DialogSDL.cpp PlatformServicesSDL.cpp SplashSDL.cpp
            ${_freetypeSources}
    INCLUDES ftconfig ${XGE_ROOT}/xgCore/public ${XGE_ROOT}/xgUtility ${_freetype}/include
    DEFINES XGPLATFORM_EXPORTS
    LINKS xgBase XGE::SDL3)
