#pragma once
#include <cstdlib>
#include "xgSceneNode.h"
#include "xgSceneNodeEngine.h"

namespace xg
{
    inline constexpr uint32_t NoTransformIndex = 0xFFFFFFFFu;

    // The actor node of the root scene graph: tree links (SceneNodeHierarchy) and name (SceneNodeMetadata).
    XG_BUILDNODETYPE(HierarchyNode,
            SceneNodeHierarchy,
            SceneNodeMetadata)
    {
        XG_BUILDNODE_CONSTRUCTOR(HierarchyNode)
        {
        }

        ~HierarchyNode() override
        {
            if (OwnsName)
                std::free(const_cast<char*>(Name));
        }

        // Slot of the actor's element in the TransformSystem pool, maintained by the TransformSystem.
        uint32_t TransformIndex = NoTransformIndex;
        // Name was allocated with malloc and is freed with the node.
        bool OwnsName = false;
    };
}
