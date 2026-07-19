#pragma once
#include <cstdint>
#include "xgBase.h"

namespace xg
{
    using SceneNodeFlags = uint32_t;

    //
    // Engine-defined SceneNode flags
    // (Bits 0–7 reserved for built-in mixins)
    //
    enum SceneNodeTypeFlags : SceneNodeFlags
    {
        SceneNode_None = 0,

        SceneNode_Hierarchy = 1 << 0,
        SceneNode_Transform = 1 << 1,
        SceneNode_Metadata = 1 << 2,
        SceneNode_Constraints = 1 << 3,

        SceneNode_InitialUserFlag = 1 << 7
    };

    //
    // Compile-time mask composer
    //
    template<typename... Mixins>
    static constexpr SceneNodeFlags ComposeNodeMask()
    {
        return (0 | ... | Mixins::Mask);
    }

    //
    // SceneNode (base class)
    //
    class SceneNode
    {
    public:
        using ActorID = uint32_t;

        ActorID        Id;
        SceneNodeFlags TypeMask;

        SceneNode(ActorID id, SceneNodeFlags mask)
            : Id(id)
            , TypeMask(mask)
        {}

        virtual ~SceneNode() = default;
    };

} // namespace xg

//
// User flag helper
//
#define XG_DEFINE_USER_FLAG(n) (xg::SceneNode_InitialUserFlag << (n))

//
// XG_BUILDNODETYPE
//
// Declares a SceneNode-derived type with:
//
//   • Inheritance from SceneNode + mixins
//   • static constexpr Mask composed at compile time
//   • Constructor that passes Mask automatically
//
// Usage:
//
//   struct LightNode
//       : XG_BUILDNODETYPE(LightNode,
//                          SceneNodeLight,
//                          SceneNodeTransform)
//   {
//       // custom fields...
//   };
#define XG_BUILDNODETYPE(NodeName, ...)                                \
    xg::SceneNode, __VA_ARGS__                                         \
public:                                                                \
    static constexpr xg::SceneNodeFlags Mask =                         \
        xg::ComposeNodeMask<__VA_ARGS__>();                            \
    NodeName(xg::SceneNode::ActorID id)                                \
        : xg::SceneNode(id, Mask) {}