#pragma once
#include <cstdint>
#include <type_traits>
#include "xgBase.h"
#include "xgID.h"

namespace xg
{
    using ActorID = xgID;
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
        SceneNode_Components = 1 << 4,

        SceneNode_InitialUserFlag = 1 << 7
    };

    //
    // SceneNode (base class)
    //
    class SceneNode
    {
    public:
        ActorID        Id;
        SceneNodeFlags TypeMask;

        SceneNode(ActorID id, SceneNodeFlags mask)
            : Id(id)
            , TypeMask(mask)
        {}

        virtual ~SceneNode() = default;

        virtual void* GetMixin(SceneNodeFlags mixinMask)
        {
            (void)mixinMask;
            return nullptr;
        }

        template<typename T>
        T* Get()
        {
            if ((TypeMask & T::Mask) != T::Mask)
                return nullptr;

            if constexpr (std::is_base_of_v<SceneNode, T>)
                return static_cast<T*>(this);
            else
                return static_cast<T*>(GetMixin(T::Mask));
        }

        template<typename T>
        const T* Get() const
        {
            return const_cast<SceneNode*>(this)->Get<T>();
        }
    };

    //
// Compile-time mask composer
//
    template<typename... Mixins>
    static constexpr SceneNodeFlags ComposeNodeMask()
    {
        return (0 | ... | Mixins::Mask);
    }

    template<typename Mixin>
    inline void AssignOwner(Mixin* mixin, SceneNode* owner)
    {
        mixin->Owner = owner;
    }

    template<typename... Mixins, typename Node>
    inline void AssignOwners(Node* node)
    {
        (AssignOwner(static_cast<Mixins*>(node), node), ...);
    }

    template<typename... Mixins, typename Node>
    inline void* FindMixin(Node* node, SceneNodeFlags mixinMask)
    {
        void* found = nullptr;
        (void)((Mixins::Mask == mixinMask && (found = static_cast<Mixins*>(node), true)) || ...);
        return found;
    }

} // namespace xg

//
// User flag helper
//
#define XG_DEFINE_USER_FLAG(n) (xg::SceneNode_InitialUserFlag << (n))

#define XG_ASSIGN_OWNERS(...) \
    xg::AssignOwner(static_cast<__VA_ARGS__*>(this), this)

#define XG_ASSIGN_OWNERS_MULTI(...) \
    int dummy[] = { (xg::AssignOwner(static_cast<__VA_ARGS__*>(this), this), 0)... }; \
    (void)dummy;

#define XG_BUILDNODE_CONSTRUCTOR_MANY(NodeName, ...) \
    NodeName(xg::ActorID id, __VA_ARGS__) \
    : _##NodeName##Base(id)

#define XG_BUILDNODE_CONSTRUCTOR(NodeName) \
    NodeName(xg::ActorID id) \
    : _##NodeName##Base(id)

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
//   XG_BUILDNODETYPE(LightNode,
//                          SceneNodeLight,
//                          SceneNodeTransform)
//   {
//       // custom fields...
//   };
#define XG_BUILDNODETYPE(NodeName, ...)                                        \
    struct _##NodeName##Base : public xg::SceneNode, public __VA_ARGS__           \
    {                                                                          \
        static constexpr xg::SceneNodeFlags Mask =                             \
            xg::ComposeNodeMask<__VA_ARGS__>();                                \
                                                                               \
        _##NodeName##Base(xg::ActorID id)                              \
            : xg::SceneNode(id, Mask)                                          \
        {                                                                      \
            xg::AssignOwners<__VA_ARGS__>(this);                               \
        }                                                                      \
                                                                               \
        void* GetMixin(xg::SceneNodeFlags mixinMask) override                  \
        {                                                                      \
            return xg::FindMixin<__VA_ARGS__>(this, mixinMask);                \
        }                                                                      \
    };                                                                         \
    struct NodeName : public _##NodeName##Base
