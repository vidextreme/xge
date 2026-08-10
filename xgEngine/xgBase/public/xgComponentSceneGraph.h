#pragma once
#include "xgSceneGraph.h"
#include "xgSceneNodeEngine.h"
#include "xgComponentNode.h"
namespace xg

{
	class ComponentSceneGraph : public SceneGraph
	{
		XG_DECLARE_BASE_SYSTEM(ComponentSceneGraph)
	public:
		void SetParent(SceneNode* child, SceneNode* parent) override {}
		virtual void InitializeComponentSet(const char* contextName, size_t strideSize) = 0;
	protected:
		virtual ComponentRaw CreateComponentRaw(ActorID id) = 0;
	public:
		template<typename T>
		T* CreateComponent(ActorID id)
		{
			T* node = new T(id);
			auto componentSet = node->Get<SceneNodeComponentSet>();
			componentSet->Data = CreateComponentRaw(id);
			this->RegisterNode(node);
			return node;
		}
	};
}