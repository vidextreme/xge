#include "pch.h"
#include "GameKernel.h"
#include "xgSceneGraph.h"
#include "xgSceneGraphFactory.h"
#include "xgActorRegistry.h"
#include "xgHierarchyNode.h"
#include "xgActorSystem.h"
#include "xgComponentSceneGraph.h"

namespace xg
{
	GameKernelModule::GameKernelModule(const char* id, int moduleID, ScriptHost* host, const char* group)
		: ScriptModule(id, moduleID, host, group)
	{}
	GameKernelModule::~GameKernelModule()
	{}
	bool GameKernelModule::Init(ScriptEngine * engine)
	{
		auto sceneGraphFactory = engine->GetSystem<SceneGraphFactory>(this);
		auto actorRegistry = engine->GetSystem<ActorRegistry>(this);
		auto sceneGraph = sceneGraphFactory->Create("tree", *actorRegistry);
		engine->RegisterSystem<SceneGraph>(nullptr, sceneGraph);

		auto sceneRoot = xg::CreateNode<HierarchyNode>(*sceneGraph);
		sceneRoot->Name = "mr root";

		auto componentSG = sceneGraphFactory->Create("compset", *actorRegistry);
		auto componentSet = static_cast<ComponentSceneGraph*>(componentSG);
		engine->RegisterSystemWithAlias(nullptr, componentSet, "PlayerComponent");
		componentSet->InitializeComponentSet("PlayerComponent", 12);
		ComponentSetNode* co = componentSet->CreateComponent<ComponentSetNode>(55);
		return true;
	}
	void GameKernelModule::Update(float dt)
	{}
	void GameKernelModule::Shutdown()
	{}
	SceneGraph* GameKernelModule::GetSceneGraph()
	{
		return nullptr;
	}
	void GameKernelModule::OnMessage(const ScriptMessage& msg)
	{}
}

//
// ------------------------------------------------------------
// ScriptHostNative factory
// ------------------------------------------------------------
//
extern "C"
XG_MODULE_EXPORT xg::ScriptModule* CreateScriptModule(const char* id, int moduleID, xg::ScriptHost* host, const char* group)
{
	return new xg::GameKernelModule(id, moduleID, host, group);
}