# Copyright (c) 2026 John David Uy
# Licensed under the MIT License. See LICENSE for details.
# Mirrors xgScriptCoreCLR.vcxproj. Outside Windows, hostfxr is located through nethost.
xge_target(xgScriptCoreCLR SHARED
    SOURCES pch.cpp HostString.cpp ScriptCoreCLR.cpp ScriptHostCoreCLR.cpp ScriptModuleCoreCLR.cpp
    INCLUDES ${XGE_ROOT}/xgCore/public ${XGE_ROOT}/xgCore/Generated ${XGE_ROOT}/xgUtility
    LINKS xgBase)
if(NOT WIN32)
    target_link_libraries(xgScriptCoreCLR PRIVATE XGE::nethost)
endif()
