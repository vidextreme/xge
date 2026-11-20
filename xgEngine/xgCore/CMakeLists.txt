# Copyright (c) 2026 John David Uy
# Licensed under the MIT License. See LICENSE for details.
# Mirrors xgCore.vcxproj, including Generated/*.generated.cpp (xgCore/Directory.Build.targets).
file(GLOB _generated CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/Generated/*.generated.cpp)
xge_target(xgCore SHARED
    SOURCES pch.cpp ActorRegistryImpl.cpp ComponentPool.cpp ComponentDataRegistryImpl.cpp TransformSystemImpl.cpp ResourceSystemImpl.cpp JobSystemImpl.cpp TextureResources.cpp ModelResources.cpp LoggerImpl.cpp MessengerImpl.cpp
            SceneGraphFactoryImpl.cpp ScriptHostNull.cpp ScriptModuleNull.cpp CodecRegistryImpl.cpp DynamicObjectImpl.cpp
            CodecBinary.cpp CodecJSON.cpp TreeSceneGraph.cpp xgEngine.cpp EventDispatcherImpl.cpp
            EventQueueImpl.cpp xgModules.cpp xgRendererAPI.cpp xgScriptAPI.cpp ScriptTree.cpp ${_generated}
    INCLUDES Generated public
    LINKS xgBase)
add_dependencies(xgCore xge_reflect)
