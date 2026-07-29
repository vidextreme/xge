#pragma once
#include "xgSceneNode.h"
#include "xgSceneNodeEngine.h"

namespace xg
{
    XG_BUILDNODETYPE(HierarchyNode,
            SceneNodeHierarchy,
            SceneNodeTransform,
            SceneNodeMetadata)
    {

        XG_BUILDNODE_CONSTRUCTOR(HierarchyNode)
        {

        }
        // You can add per-node data here if needed.
        // But keep it minimal — hierarchy nodes should stay lightweight.
       
    };
}
