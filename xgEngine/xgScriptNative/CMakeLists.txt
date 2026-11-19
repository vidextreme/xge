# Copyright (c) 2026 John David Uy
# Licensed under the MIT License. See LICENSE for details.
# Mirrors xgScriptNative.vcxproj.
xge_target(xgScriptNative SHARED
    SOURCES pch.cpp CompositeComponent.cpp InteropSubsystem.cpp interop/xgLogInterop.cpp interop/xgProjectInterop.cpp interop/xgResourceInterop.cpp
            interop/xgUIHostInterop.cpp interop/xgMat4x4Interop.cpp interop/xgQuatInterop.cpp
            interop/xgScriptMessageInterop.cpp interop/xgVec2Interop.cpp interop/xgVec3Interop.cpp
            interop/xgGeometryInterop.cpp NativeSubsystem.cpp ScriptModuleNative.cpp ScriptModuleFunctions.cpp ScriptHostNative.cpp ScriptNative.cpp
    INCLUDES ${XGE_ROOT}/xgCore/public public ${XGE_ROOT}/xgCore/Generated ${XGE_ROOT}/xgEditorKit/public
             ${XGE_ROOT}/xgImUIKit/public
    LINKS xgBase xgEditorKit xgImUIKit xgPlatform)
