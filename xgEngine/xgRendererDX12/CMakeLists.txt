# Copyright (c) 2026 John David Uy
# Licensed under the MIT License. See LICENSE for details.
# Mirrors xgRendererDX12.vcxproj (Windows only).
xge_target(xgRendererDX12 SHARED
    SOURCES pch.cpp RendererDX12.cpp RendererDX12Views.cpp RendererFactory.cpp
    INCLUDES ${XGE_ROOT}/xgCore/public ${XGE_ROOT}/xgShaders ${XGE_ROOT}/xgShaders/compiled
    LINKS xgBase d3d12 dxgi dxguid d3dcompiler)
