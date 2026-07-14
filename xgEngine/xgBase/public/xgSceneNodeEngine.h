#pragma once
#include "xgSceneNode.h"

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

        float Position[3]{ 0, 0, 0 };
        float Rotation[3]{ 0, 0, 0 };
        float Scale[3]{ 1, 1, 1 };
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
