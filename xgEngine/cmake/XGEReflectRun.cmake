# Copyright (c) 2026 John David Uy
# Licensed under the MIT License. See LICENSE for details.
#
# cmake -DXGE_DOTNET=... -DXGE_REFLECT_DLL=... -DSOURCE=... -DNATIVE=... [-DMANAGED=...] -DSTAGING=... -P XGEReflectRun.cmake
#
# Runs xgReflectGen into a staging folder and copies only changed files, so unchanged generated sources keep their
# timestamps and don't trigger rebuilds.

get_filename_component(name "${SOURCE}" NAME)
set(stage "${STAGING}/${name}")
file(REMOVE_RECURSE "${stage}")
set(args "${SOURCE}" "${stage}/native")
if(MANAGED)
    list(APPEND args "${stage}/managed")
endif()

execute_process(COMMAND "${XGE_DOTNET}" "${XGE_REFLECT_DLL}" ${args}
    RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE output)
if(NOT result EQUAL 0 OR output MATCHES "ERROR:")
    message(FATAL_ERROR "xgReflectGen ${SOURCE} failed:\n${output}")
endif()

function(sync from to)
    file(GLOB files RELATIVE "${from}" "${from}/*")
    foreach(file ${files})
        execute_process(COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${from}/${file}" "${to}/${file}")
    endforeach()
endfunction()

file(MAKE_DIRECTORY "${NATIVE}")
sync("${stage}/native" "${NATIVE}")
if(MANAGED)
    file(MAKE_DIRECTORY "${MANAGED}")
    sync("${stage}/managed" "${MANAGED}")
endif()
