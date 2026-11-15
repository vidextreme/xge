# Copyright (c) 2026 John David Uy
# Licensed under the MIT License. See LICENSE for details.

# xge_target(<name> <EXECUTABLE|SHARED|STATIC> SOURCES ... [INCLUDES ...] [LINKS ...] [WHOLE_ARCHIVE ...]
#            [DEFINES ...] [NO_EXPORTS])
#
# Mirrors the settings every vcxproj shares: C++20, XG_EXPORTS (except NO_EXPORTS), _DEBUG in Debug, the project
# folder on the include path (for pch.h). Shared modules get no "lib" prefix, so they load as "<name>.so" like
# NativeAOT modules, and bind their own symbols first (-Bsymbolic), so every module keeps its own copy of the
# static libraries' globals, as each Windows DLL does. WHOLE_ARCHIVE links every object of those static libraries
# (UseLibraryDependencyInputs in the vcxprojs), so their XG_ exports survive even when nothing references them.
function(xge_target name kind)
    cmake_parse_arguments(ARG "NO_EXPORTS" "" "SOURCES;INCLUDES;LINKS;WHOLE_ARCHIVE;DEFINES" ${ARGN})

    if(kind STREQUAL "EXECUTABLE")
        add_executable(${name} ${ARG_SOURCES})
    elseif(kind STREQUAL "SHARED")
        add_library(${name} SHARED ${ARG_SOURCES})
        set_target_properties(${name} PROPERTIES PREFIX "")
        if(UNIX AND NOT APPLE)
            target_link_options(${name} PRIVATE "LINKER:-Bsymbolic" "LINKER:--no-undefined")
        endif()
    elseif(kind STREQUAL "STATIC")
        add_library(${name} STATIC ${ARG_SOURCES})
    else()
        message(FATAL_ERROR "xge_target: unknown kind ${kind}")
    endif()

    target_include_directories(${name} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${ARG_INCLUDES})
    target_compile_definitions(${name} PRIVATE $<$<CONFIG:Debug>:_DEBUG> ${ARG_DEFINES})
    if(NOT ARG_NO_EXPORTS)
        target_compile_definitions(${name} PRIVATE XG_EXPORTS)
    endif()
    if(MSVC)
        target_compile_options(${name} PRIVATE $<$<COMPILE_LANGUAGE:CXX>:/Zc:__cplusplus /permissive->)
    endif()
    foreach(lib ${ARG_WHOLE_ARCHIVE})
        target_link_libraries(${name} PRIVATE "$<LINK_LIBRARY:WHOLE_ARCHIVE,${lib}>")
    endforeach()
    if(ARG_LINKS)
        target_link_libraries(${name} PRIVATE ${ARG_LINKS})
    endif()
endfunction()

# Third-party sources compiled into a target with their warnings off, as the vcxprojs do.
function(xge_quiet_sources)
    if(MSVC)
        set_source_files_properties(${ARGN} PROPERTIES COMPILE_OPTIONS "/W0")
    else()
        set_source_files_properties(${ARGN} PROPERTIES COMPILE_OPTIONS "-w")
    endif()
endfunction()
