#include "pch.h"
#include "CompositeSceneGraph.h"

namespace xg
{
	void CompositeSceneGraph::InitializeComponentSet(const char* contextName, size_t strideSize)
	{
		_componentPool = new StridePool(strideSize);
		_contextName = contextName;
	}
	void CompositeSceneGraph::RegisterNode(SceneNode* node)
	{
		if (!(node->TypeMask & SceneNode_Components))
			return;
		
		auto cs = node->Get<SceneNodeComponentSet>();
		cs->Owner = node;
		_nodes[node->Id] = cs;
		//cs->Size = _componentPool->GetStride();		
		_registry->RegisterGraphNode(node->Id, this, node);
	}
	void CompositeSceneGraph::DestroyNode(SceneNode * node)
	{
		auto ID = node->Id;
		auto it = _nodes.find(ID);
		if (it == _nodes.end())
			return;

		int strideIdx = _componentPool->Find(ID);
		_componentPool->RemoveAt(strideIdx);

		auto* h = it->second;

		_registry->UnregisterGraphNode(ID, this);

		_nodes.erase(it);
		delete node;
	}
	void CompositeSceneGraph::Traverse(SceneNode * root, void(*visitor)(SceneNode*))
	{}
	SceneNode* CompositeSceneGraph::GetNode(ActorID id) const
	{
		auto it = _nodes.find(id);
		if (it == _nodes.end())
			return nullptr;
		return it->second->Owner;
	}

	void CompositeSceneGraph::Shutdown()
	{
		for (auto& [key, value] : _nodes)
		{
			delete value->Owner;
		}
		_nodes.clear();

		XG_DELETE(_componentPool);
	}

	ComponentRaw CompositeSceneGraph::CreateComponentRaw(ActorID id)
	{
		int index = _componentPool->Add(id);
		auto raw = _componentPool->GetPtr(index);
		return raw;
	}
}