# Copyright (c) 2026 John David Uy
# Licensed under the MIT License. See LICENSE for details.
# Mirrors xgEditorKernel.vcxproj (its Generated/*.cpp are not compiled, as in the vcxproj).
xge_target(xgEditorKernel SHARED
    SOURCES pch.cpp EditorKernel.cpp NativeTestWindow.cpp SceneViewWindow.cpp EditorSceneViewExports.cpp EditorTextureExports.cpp
    INCLUDES ${XGE_ROOT}/xgImUIKit/public ${XGE_ROOT}/xgCore/public ${XGE_ROOT}/xgCore/Generated ${XGE_ROOT}/xgSharedKit
             ${XGE_ROOT}/xgSharedKit/public ${XGE_ROOT}/xgEditorKit ${XGE_ROOT}/xgEditorKit/public Generated
    WHOLE_ARCHIVE xgEditorKit xgSharedKit
    LINKS xgBase xgPlatform xgImUIKit)
add_dependencies(xgEditorKernel xge_reflect)
