#include "pch.h"
#include "GameKernel.h"
#include "xgScriptHost.h"
#include "xgScriptEngine.h"
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
		auto sceneGraphFactory = engine->GetSubsystem<SceneGraphFactory>(this);
		auto actorRegistry = engine->GetSubsystem<ActorRegistry>(this);
		auto sceneGraph = sceneGraphFactory->Create("tree", *actorRegistry);
		engine->RegisterSubsystem<SceneGraph>(nullptr, sceneGraph);

		auto sceneRoot = xg::CreateNode<HierarchyNode>(*sceneGraph);
		sceneRoot->Name = "mr root";

		auto componentSG = sceneGraphFactory->Create("compset", *actorRegistry);
		auto componentSet = static_cast<ComponentSceneGraph*>(componentSG);
		engine->RegisterSubsystemWithAlias(nullptr, componentSet, "PlayerComponent");
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
XG_MODULE_EXPORT xg::ScriptModule* XG_CreateScriptModule(const char* id, int moduleID, xg::ScriptHost* host, const char* group)
{
	if (host && host->GetEngine())
		xg::SetLogger(host->GetEngine()->GetLogger());
	return new xg::GameKernelModule(id, moduleID, host, group);
}