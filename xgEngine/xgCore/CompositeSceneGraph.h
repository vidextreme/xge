#pragma once
#include "xgComponentSceneGraph.h"
#include "StridePool.h"
namespace xg
{
    class CompositeSceneGraph : public ComponentSceneGraph
    {
        XG_DECLARE_SYSTEM(CompositeSceneGraph, ComponentSceneGraph)

    public:
        explicit CompositeSceneGraph(ActorRegistry& registry)
        {
            _registry = &registry;
        }

        void InitializeComponentSet(const char* contextName, size_t strideSize) override;

        void RegisterNode(SceneNode* node) override;
        void DestroyNode(SceneNode* node) override;

        // SceneGraph API
        void Traverse(SceneNode* root, void(*visitor)(SceneNode*)) override;
        SceneNode* GetNode(ActorID id) const override;

        void Shutdown() override;
    protected:
        ComponentRaw CreateComponentRaw(ActorID id) override;
    private:
        std::string _contextName;
        std::unordered_map<ActorID, SceneNodeComponentSet*> _nodes;
        StridePool* _componentPool;
    };

}
