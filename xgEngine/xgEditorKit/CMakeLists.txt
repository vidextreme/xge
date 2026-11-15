# Copyright (c) 2026 John David Uy
# Licensed under the MIT License. See LICENSE for details.
# Mirrors xgEditorKit.vcxproj (static library).
xge_target(xgEditorKit STATIC
    SOURCES pch.cpp xgEditorKit.cpp UIHostSession.cpp
    INCLUDES ${XGE_ROOT}/xgCore/public ${XGE_ROOT}/xgCore/Generated ${XGE_ROOT}/xgUtility ${XGE_ROOT}/xgSharedKit/Generated
             ${XGE_ROOT}/xgImUIKit/public public
    LINKS xgBase)
