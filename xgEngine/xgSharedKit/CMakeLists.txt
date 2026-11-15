# Copyright (c) 2026 John David Uy
# Licensed under the MIT License. See LICENSE for details.
# Mirrors xgSharedKit.vcxproj (static library).
xge_target(xgSharedKit STATIC
    SOURCES pch.cpp xgActorSystem.cpp xgSharedKit.cpp
    INCLUDES ${XGE_ROOT}/xgCore/public ${XGE_ROOT}/xgUtility ${XGE_ROOT}/xgCore/Generated public
    LINKS xgBase)
