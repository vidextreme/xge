# Copyright (c) 2026 John David Uy
# Licensed under the MIT License. See LICENSE for details.
#
# Builds the managed projects with dotnet. Their csprojs place the output in gameroot/bin themselves, so this
# follows the default XGE_OUTPUT_DIR only. The NativeAOT test module is published for XGE_DOTNET_RID.
if(NOT XGE_BUILD_MANAGED)
    return()
endif()

set(_managedConfig "$<IF:$<CONFIG:Debug>,Debug,Release>")
set(_managedProjects
    xgEditor.CoreCLR/xgEditor.CoreCLR.csproj
    xgHub.CoreCLR/xgHub.CoreCLR.csproj
    GameDemoModule/GameDemoModule.csproj)

set(_commands "")
foreach(project ${_managedProjects})
    list(APPEND _commands COMMAND ${XGE_DOTNET} build "${XGE_ROOT}/${project}" -c ${_managedConfig} -nologo -v:m
        -p:XGERid=${XGE_DOTNET_RID})
endforeach()

add_custom_target(xge_managed ALL ${_commands} WORKING_DIRECTORY "${XGE_ROOT}" COMMENT "Building managed projects" VERBATIM)
add_dependencies(xge_managed xge_reflect xgScriptNative xgGameKernel)
