# Copyright (c) 2026 John David Uy
# Licensed under the MIT License. See LICENSE for details.
#
# Runs xgReflectGen like the vcxproj pre-build steps. The generator loads tree-sitter natively: the Windows DLLs
# are in 3rdparty/TreeSitter; elsewhere tree-sitter and tree-sitter-cpp are built from pinned sources into tools/.
# Generation runs once at configure time (so globs see the generated sources) and again before every build.

set(XGE_TOOLS_DIR "${XGE_ROOT}/tools")
set(XGE_REFLECT_DLL "${XGE_TOOLS_DIR}/xgReflectGen.dll")
set(XGE_REFLECT_SCRIPT "${XGE_ROOT}/cmake/XGEReflectRun.cmake")

if(NOT WIN32)
    set(_tsLib "${XGE_TOOLS_DIR}/libtree-sitter${CMAKE_SHARED_LIBRARY_SUFFIX}")
    set(_tsCppLib "${XGE_TOOLS_DIR}/libtree-sitter-cpp${CMAKE_SHARED_LIBRARY_SUFFIX}")
    if(NOT EXISTS "${_tsLib}" OR NOT EXISTS "${_tsCppLib}")
        message(STATUS "XGE: building tree-sitter for xgReflectGen")
        set(_tsBuild "${CMAKE_BINARY_DIR}/_treesitter")
        execute_process(
            COMMAND ${CMAKE_COMMAND} -S "${XGE_ROOT}/cmake/treesitter" -B "${_tsBuild}" -G "${CMAKE_GENERATOR}"
                    -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=${CMAKE_C_COMPILER} -DXGE_TOOLS_DIR=${XGE_TOOLS_DIR}
            RESULT_VARIABLE _result)
        if(_result EQUAL 0)
            execute_process(COMMAND ${CMAKE_COMMAND} --build "${_tsBuild}" --config Release RESULT_VARIABLE _result)
        endif()
        if(NOT _result EQUAL 0)
            message(FATAL_ERROR "XGE: building tree-sitter failed")
        endif()
    endif()
endif()

message(STATUS "XGE: building xgReflectGen")
execute_process(
    COMMAND ${XGE_DOTNET} build "${XGE_TOOLS_DIR}/xgReflectGen/xgReflectGen.csproj" -c Debug -nologo -v:q
    RESULT_VARIABLE _result)
if(NOT _result EQUAL 0 OR NOT EXISTS "${XGE_REFLECT_DLL}")
    message(FATAL_ERROR "XGE: building xgReflectGen failed")
endif()

set(XGE_REFLECT_COMMANDS "")

# xge_reflect(<source folder> <native output> [managed output])
function(xge_reflect source native)
    set(managed "${ARGV2}")
    set(args -DXGE_DOTNET=${XGE_DOTNET} -DXGE_REFLECT_DLL=${XGE_REFLECT_DLL} -DSOURCE=${source} -DNATIVE=${native}
             -DMANAGED=${managed} -DSTAGING=${CMAKE_BINARY_DIR}/_reflect)
    execute_process(COMMAND ${CMAKE_COMMAND} ${args} -P "${XGE_REFLECT_SCRIPT}" RESULT_VARIABLE result)
    if(NOT result EQUAL 0)
        message(FATAL_ERROR "XGE: xgReflectGen failed for ${source}")
    endif()
    set(commands ${XGE_REFLECT_COMMANDS})
    list(APPEND commands COMMAND ${CMAKE_COMMAND} ${args} -P "${XGE_REFLECT_SCRIPT}")
    set(XGE_REFLECT_COMMANDS ${commands} PARENT_SCOPE)
endfunction()

xge_reflect("${XGE_ROOT}/xgBase" "${XGE_ROOT}/xgBase/Generated" "${XGE_ROOT}/xgSharedKit.Managed/Generated")
xge_reflect("${XGE_ROOT}/xgCore" "${XGE_ROOT}/xgCore/Generated" "${XGE_ROOT}/xgSharedKit.Managed/Generated")
xge_reflect("${XGE_ROOT}/xgEditorKernel" "${XGE_ROOT}/xgEditorKernel/Generated")

add_custom_target(xge_reflect ALL ${XGE_REFLECT_COMMANDS} COMMENT "Running xgReflectGen" VERBATIM)
