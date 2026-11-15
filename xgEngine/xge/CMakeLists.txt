# Copyright (c) 2026 John David Uy
# Licensed under the MIT License. See LICENSE for details.
# Mirrors xge.vcxproj (the engine executable).
xge_target(xge EXECUTABLE
    SOURCES xge.cpp
    INCLUDES ${XGE_ROOT}/xgCore/public ${XGE_ROOT}/xgCore/Generated
    LINKS xgBase xgPlatform xgCore
    NO_EXPORTS)
