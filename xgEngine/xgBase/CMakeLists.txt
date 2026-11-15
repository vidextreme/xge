# Copyright (c) 2026 John David Uy
# Licensed under the MIT License. See LICENSE for details.
# Mirrors xgBase.vcxproj (static library).
xge_target(xgBase STATIC
    SOURCES pch.cpp xgDynamicObject.cpp xgID.cpp xgJSON.cpp xgReflectJSON.cpp xgLog.cpp MemoryStreamImpl.cpp
            xgPathUtils.cpp xgRoute.cpp xgTypeRegistry.cpp
    INCLUDES Generated ${XGE_ROOT}/3rdparty/json)
target_include_directories(xgBase PUBLIC public math)
if(NOT WIN32)
    target_include_directories(xgBase PUBLIC ${XGE_ROOT}/3rdparty/DirectXMath/Inc ${XGE_ROOT}/3rdparty/DirectXMath/Compat)
endif()
target_link_libraries(xgBase PUBLIC Threads::Threads ${CMAKE_DL_LIBS})
add_dependencies(xgBase xge_reflect)
