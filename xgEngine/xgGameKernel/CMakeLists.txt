# Copyright (c) 2026 John David Uy
# Licensed under the MIT License. See LICENSE for details.
# Mirrors xgGameKernel.vcxproj.
xge_target(xgGameKernel SHARED
    SOURCES pch.cpp GameKernel.cpp
    INCLUDES ${XGE_ROOT}/xgCore/public ${XGE_ROOT}/xgCore/Generated ${XGE_ROOT}/xgSharedKit ${XGE_ROOT}/xgSharedKit/public
             ${XGE_ROOT}/xgUtility Generated
    WHOLE_ARCHIVE xgSharedKit
    LINKS xgBase)
