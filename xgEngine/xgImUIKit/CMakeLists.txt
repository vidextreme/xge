# Copyright (c) 2026 John David Uy
# Licensed under the MIT License. See LICENSE for details.
# Mirrors xgImUIKit.vcxproj: the kit, Dear ImGui (docking) and the trimmed FreeType subset. The ftconfig folder comes
# first on the include path so its ftmodule.h overrides FreeType's module list.
set(_imgui ${XGE_ROOT}/3rdparty/imgui-docking)
set(_freetype ${XGE_ROOT}/3rdparty/freetype)

set(_freetypeSources
    ${_freetype}/src/base/ftbase.c ${_freetype}/src/base/ftinit.c ${_freetype}/src/base/ftsystem.c
    ${_freetype}/src/base/ftdebug.c ${_freetype}/src/base/ftbitmap.c ${_freetype}/src/base/ftglyph.c
    ${_freetype}/src/base/ftsynth.c ${_freetype}/src/base/ftmm.c ${_freetype}/src/sfnt/sfnt.c
    ${_freetype}/src/truetype/truetype.c ${_freetype}/src/smooth/smooth.c ${_freetype}/src/autofit/autofit.c
    ${_freetype}/src/psnames/psnames.c ${_freetype}/src/gzip/ftgzip.c)
set_source_files_properties(${_freetypeSources} PROPERTIES COMPILE_DEFINITIONS "FT2_BUILD_LIBRARY;_CRT_SECURE_NO_WARNINGS")
xge_quiet_sources(${_freetypeSources} ${_imgui}/misc/freetype/imgui_freetype.cpp)

xge_target(xgImUIKit SHARED
    SOURCES Api.cpp Host.cpp Theme.cpp Icons.cpp Widgets.cpp SceneViewDraw.cpp PrototypePanels.cpp Shell.cpp
            backends/UIPlatformBackend.cpp backends/UIRenderBackend.cpp
            ${_imgui}/imgui.cpp ${_imgui}/imgui_demo.cpp ${_imgui}/imgui_draw.cpp ${_imgui}/imgui_tables.cpp
            ${_imgui}/imgui_widgets.cpp ${_imgui}/misc/freetype/imgui_freetype.cpp
            ${_freetypeSources}
    INCLUDES ftconfig public ${_imgui} ${_freetype}/include
    DEFINES IMGUI_ENABLE_FREETYPE
    LINKS xgBase)
