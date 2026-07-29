#pragma once
#include "xgSceneNode.h"
#include "xgVec3.h"
#include "xgQuat.h"
#include "xgVec4.h"
#include "xgMat4x4.h"
#include "xgTransform.h"

namespace xg
{
    
    struct SceneNodeMixinBase
    {
        SceneNode* Owner = nullptr;
    };


    //
    // ------------------------------------------------------------
    // Built-in mixins (engine-level)
    // ------------------------------------------------------------
    //

    struct SceneNodeHierarchy : SceneNodeMixinBase
    {
        static constexpr SceneNodeFlags Mask = SceneNode_Hierarchy;

        SceneNodeHierarchy* Parent = nullptr;
        SceneNodeHierarchy* FirstChild = nullptr;
        SceneNodeHierarchy* NextSibling = nullptr;
    };

    struct SceneNodeTransform : SceneNodeMixinBase
    {
        static constexpr SceneNodeFlags Mask = SceneNode_Transform;

        Transform Transform;
    };

    struct SceneNodeMetadata : SceneNodeMixinBase
    {
        static constexpr SceneNodeFlags Mask = SceneNode_Metadata;

        const char* Name = nullptr;
        const char* Tag = nullptr;
    };

    struct SceneNodeConstraints : SceneNodeMixinBase
    {
        static constexpr SceneNodeFlags Mask = SceneNode_Constraints;

        bool Locked = false;
    };


    //
    // ------------------------------------------------------------
    // User-defined mixins (example pattern)
    //
    // Users define their own mixins using XG_DEFINE_USER_FLAG(n)
    //
    // Example:
    //
    //     struct SceneNodeLight
    //     {
    //         static constexpr SceneNodeFlags Mask = XG_DEFINE_USER_FLAG(1);
    //
    //         float Intensity = 1.0f;
    //         float Range     = 10.0f;
    //     };
    //
    // ------------------------------------------------------------
    //

} // namespace xg
