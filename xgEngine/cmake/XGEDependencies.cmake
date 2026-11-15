# Copyright (c) 2026 John David Uy
# Licensed under the MIT License. See LICENSE for details.
include(FetchContent)

find_package(Threads REQUIRED)

# ------------------------------------------------------------
# SDL3: the prebuilt Windows SDK in 3rdparty, otherwise an installed SDL3 or a pinned source build.
# ------------------------------------------------------------
if(WIN32)
    add_library(xge_sdl3 SHARED IMPORTED GLOBAL)
    set_target_properties(xge_sdl3 PROPERTIES
        IMPORTED_IMPLIB "${XGE_ROOT}/3rdparty/SDL3/lib/x64/SDL3.lib"
        IMPORTED_LOCATION "${XGE_ROOT}/3rdparty/SDL3/lib/x64/SDL3.dll"
        INTERFACE_INCLUDE_DIRECTORIES "${XGE_ROOT}/3rdparty/SDL3/include")
    add_library(XGE::SDL3 ALIAS xge_sdl3)
    file(COPY "${XGE_ROOT}/3rdparty/SDL3/lib/x64/SDL3.dll" DESTINATION "${XGE_OUTPUT_DIR}")
else()
    find_package(SDL3 3.4 CONFIG QUIET)
    if(SDL3_FOUND)
        message(STATUS "XGE: using installed SDL3 ${SDL3_VERSION}")
        add_library(xge_sdl3 INTERFACE)
        target_link_libraries(xge_sdl3 INTERFACE SDL3::SDL3)
    else()
        message(STATUS "XGE: SDL3 not installed, building release-3.4.20 from source")
        set(SDL_SHARED ON CACHE BOOL "" FORCE)
        set(SDL_STATIC OFF CACHE BOOL "" FORCE)
        set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
        set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)
        FetchContent_Declare(SDL3
            GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
            GIT_TAG release-3.4.20
            GIT_SHALLOW TRUE)
        FetchContent_MakeAvailable(SDL3)
        add_library(xge_sdl3 INTERFACE)
        target_link_libraries(xge_sdl3 INTERFACE SDL3-shared)
    endif()
    add_library(XGE::SDL3 ALIAS xge_sdl3)
endif()

# ------------------------------------------------------------
# nethost: finds the installed .NET runtime's hostfxr outside Windows (Windows uses the runtime bundled in
# gameroot/coreclr). Comes from the dotnet SDK's apphost pack.
# ------------------------------------------------------------
if(NOT WIN32)
    find_program(XGE_DOTNET dotnet REQUIRED)
    get_filename_component(_dotnetReal "${XGE_DOTNET}" REALPATH)
    get_filename_component(_dotnetRoot "${_dotnetReal}" DIRECTORY)
    if(DEFINED ENV{DOTNET_ROOT})
        set(_dotnetRoot "$ENV{DOTNET_ROOT}")
    endif()

    if(APPLE)
        set(_rid osx)
    else()
        set(_rid linux)
    endif()
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64|ARM64")
        string(APPEND _rid "-arm64")
    else()
        string(APPEND _rid "-x64")
    endif()
    set(XGE_DOTNET_RID ${_rid} CACHE STRING "Runtime identifier for NativeAOT modules")

    string(REGEX REPLACE "^[a-z]+-" "" _arch ${_rid})
    file(GLOB _nethostHeaders "${_dotnetRoot}/packs/Microsoft.NETCore.App.Host.${_rid}/*/runtimes/*/native/nethost.h")
    if(NOT _nethostHeaders)
        file(GLOB _nethostHeaders "${_dotnetRoot}/packs/Microsoft.NETCore.App.Host.*-${_arch}/*/runtimes/*/native/nethost.h")
    endif()
    if(NOT _nethostHeaders)
        message(FATAL_ERROR "XGE: nethost not found under ${_dotnetRoot}/packs (install the .NET 10 SDK)")
    endif()
    list(SORT _nethostHeaders COMPARE NATURAL ORDER DESCENDING)
    list(GET _nethostHeaders 0 _nethostHeader)
    get_filename_component(_nethostDir "${_nethostHeader}" DIRECTORY)
    message(STATUS "XGE: nethost from ${_nethostDir}")

    add_library(xge_nethost STATIC IMPORTED GLOBAL)
    set_target_properties(xge_nethost PROPERTIES
        IMPORTED_LOCATION "${_nethostDir}/libnethost.a"
        INTERFACE_INCLUDE_DIRECTORIES "${_nethostDir}"
        INTERFACE_COMPILE_DEFINITIONS NETHOST_USE_AS_STATIC)
    add_library(XGE::nethost ALIAS xge_nethost)
else()
    find_program(XGE_DOTNET dotnet REQUIRED)
    set(XGE_DOTNET_RID win-x64 CACHE STRING "Runtime identifier for NativeAOT modules")
endif()
