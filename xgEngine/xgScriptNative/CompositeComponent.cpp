#include "pch.h"
#include "xgScriptEngine.h"
#include "xgComponentSceneGraph.h"

XG_MODULE_EXPORT void* XGAllocateComponent(xg::ScriptEngine* engine, xg::ActorID actorID, const char* componentType, uint32_t size)
{		
	xg::ComponentSceneGraph* graph = engine->GetSystemWithAlias<xg::ComponentSceneGraph>(nullptr, componentType);
	xg::SceneNode* node = graph->GetNode(actorID);
	auto test = xg::SceneNodeComponentSet::Mask;
	auto test2 = xg::ComponentSetNode::Mask;
	xg::SceneNodeComponentSet* cs = node->Get<xg::SceneNodeComponentSet>();

	return cs->Data;
}