#pragma once
#include "xgSceneNode.h"
#include "math/xgVec3.h"
#include "math/xgQuat.h"
#include "math/xgVec4.h"
#include "math/xgMat4x4.h"
#include "math/xgTransform.h"

namespace xg
{
    //
    // ------------------------------------------------------------
    // Built-in mixins (engine-level)
    // ------------------------------------------------------------
    //

    struct SceneNodeHierarchy
    {
        static constexpr SceneNodeFlags Mask = SceneNode_Hierarchy;

        SceneNode* Parent = nullptr;
        SceneNode* FirstChild = nullptr;
        SceneNode* NextSibling = nullptr;
    };

    struct SceneNodeTransform
    {
        static constexpr SceneNodeFlags Mask = SceneNode_Transform;

        Transform Transform;
    };

    struct SceneNodeMetadata
    {
        static constexpr SceneNodeFlags Mask = SceneNode_Metadata;

        const char* Name = nullptr;
        const char* Tag = nullptr;
    };

    struct SceneNodeConstraints
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
