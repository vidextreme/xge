# Copyright (c) 2026 John David Uy
# Licensed under the MIT License. See LICENSE for details.
# Mirrors xgRendererVulkan.vcxproj (Windows only for now: it creates Win32 surfaces).
xge_quiet_sources(${XGE_ROOT}/3rdparty/volk/volk.c)
xge_target(xgRendererVulkan SHARED
    SOURCES pch.cpp RendererVulkan.cpp RendererVulkanViews.cpp RendererFactory.cpp ${XGE_ROOT}/3rdparty/volk/volk.c
    INCLUDES ${XGE_ROOT}/xgCore/public ${XGE_ROOT}/xgShaders ${XGE_ROOT}/xgShaders/compiled
             ${XGE_ROOT}/3rdparty/vulkan/include ${XGE_ROOT}/3rdparty/volk
    DEFINES VK_USE_PLATFORM_WIN32_KHR VK_NO_PROTOTYPES
    LINKS xgBase)
