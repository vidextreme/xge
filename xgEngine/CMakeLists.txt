# Copyright (c) 2026 John David Uy
# Licensed under the MIT License. See LICENSE for details.
#
# Cross-platform build of the native engine. On Windows the Visual Studio solution (XGE.slnx) stays the main build;
# this file mirrors its projects so the engine also builds on Linux and macOS.
#
#   cmake -S xgEngine -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
#   cmake --build build
#
# Output goes to gameroot/bin (XGE_OUTPUT_DIR). The managed projects are built with dotnet as part of the build
# (XGE_BUILD_MANAGED), and the reflection generator runs before the native targets that use its output.
cmake_minimum_required(VERSION 3.25)
project(XGE LANGUAGES C CXX)

set(XGE_ROOT ${CMAKE_CURRENT_SOURCE_DIR})
set(XGE_OUTPUT_DIR "${XGE_ROOT}/../gameroot/bin" CACHE PATH "Folder that receives xge, the engine modules and the managed assemblies")
option(XGE_BUILD_MANAGED "Build the managed projects (dotnet) with the native targets" ON)
option(XGE_BUILD_RENDERERS "Build the renderer modules (Windows only for now)" ${WIN32})

get_filename_component(XGE_OUTPUT_DIR "${XGE_OUTPUT_DIR}" ABSOLUTE)
list(APPEND CMAKE_MODULE_PATH "${XGE_ROOT}/cmake")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_C_VISIBILITY_PRESET hidden)
set(CMAKE_VISIBILITY_INLINES_HIDDEN ON)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${XGE_OUTPUT_DIR})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${XGE_OUTPUT_DIR})
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
foreach(config ${CMAKE_CONFIGURATION_TYPES} Debug Release RelWithDebInfo MinSizeRel)
    string(TOUPPER ${config} config)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${config} ${XGE_OUTPUT_DIR})
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_${config} ${XGE_OUTPUT_DIR})
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_${config} ${CMAKE_BINARY_DIR}/lib)
endforeach()

# Modules find each other next to the executable.
if(APPLE)
    set(CMAKE_BUILD_RPATH "@loader_path")
    set(CMAKE_INSTALL_RPATH "@loader_path")
elseif(UNIX)
    set(CMAKE_BUILD_RPATH "$ORIGIN")
    set(CMAKE_INSTALL_RPATH "$ORIGIN")
endif()

include(XGEHelpers)
include(XGEDependencies)
include(XGEReflect)

add_subdirectory(xgBase)
add_subdirectory(xgSharedKit)
add_subdirectory(xgCore)
add_subdirectory(xgPlatform)
add_subdirectory(xgImUIKit)
add_subdirectory(xgEditorKit)
add_subdirectory(xgScriptNative)
add_subdirectory(xgScriptCoreCLR)
add_subdirectory(xgGameKernel)
add_subdirectory(xgEditorKernel)
add_subdirectory(xge)

if(XGE_BUILD_RENDERERS)
    if(WIN32)
        add_subdirectory(xgRendererDX12)
        add_subdirectory(xgRendererVulkan)
    else()
        message(STATUS "XGE: renderer modules are Windows-only for now; set RendererModule to \"\" (SDL GPU fallback)")
    endif()
endif()

include(XGEManaged)
