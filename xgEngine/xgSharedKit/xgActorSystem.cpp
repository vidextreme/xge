#include "pch.h"
#include "xgActorSystem.h"
#include "xgHierarchyNode.h"
#include "xgSceneGraph.h"
#include "xgScriptEngine.h"

using namespace xg;

constexpr const char* DEFAULT_ACTOR_NAME = "actor";

void _SetActorName(HierarchyNode* hNode, const char* newName)
{
    if (hNode->Name != DEFAULT_ACTOR_NAME)
    {
        free((void*)hNode->Name);
    }

    // Empty string → treat as valid empty name
    if (newName[0] == '\0')
    {
        // Allocate a 1-byte empty string
        char* buffer = (char*)malloc(1);
        buffer[0] = '\0';
        hNode->Name = buffer;
        return;
    }

    // If new name is the literal "actor"
    if (strcmp(newName, DEFAULT_ACTOR_NAME) == 0)
    {
        hNode->Name = DEFAULT_ACTOR_NAME;
        return;
    }

    // Otherwise allocate new heap memory
    size_t len = strlen(newName);
    char* buffer = (char*)malloc(len + 1);
    memcpy(buffer, newName, len + 1);

    hNode->Name = buffer;
}

ActorID XGCreateActor(ScriptEngine* engine)
{
    return XGCreateActorWithName(engine, DEFAULT_ACTOR_NAME);
}

ActorID XGCreateActorWithName(ScriptEngine* engine, const char* actorName)
{
    SceneGraph* sceneGraph = engine->GetSystem<SceneGraph>();
    HierarchyNode* node = CreateNode<HierarchyNode>(*sceneGraph);
    node->TypeMask = HierarchyNode::Mask;
    _SetActorName(node, actorName);
    return node->Id;
}

const char* XGGetActorName(ScriptEngine* engine, ActorID id)
{
	SceneGraph* sceneGraph = engine->GetSystem<SceneGraph>();
	SceneNode* node = sceneGraph->GetNode(id);
	HierarchyNode* hNode = node->Get<HierarchyNode>();
	return hNode->Name;
}

void XGSetActorName(xg::ScriptEngine* engine, xg::ActorID id, const char* newName)
{
    SceneGraph* sceneGraph = engine->GetSystem<SceneGraph>();
    SceneNode* node = sceneGraph->GetNode(id);
    HierarchyNode* hNode = node->Get<HierarchyNode>();

    _SetActorName(hNode, newName);
}
