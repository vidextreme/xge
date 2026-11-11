#include "pch.h"
#include "xgActorSystem.h"
#include "xgHierarchyNode.h"
#include "xgSceneGraph.h"
#include "xgScriptEngine.h"
#include "xgActorRegistry.h"

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

ActorID XG_CreateActor(ScriptEngine* engine)
{
    return XG_CreateActorWithName(engine, DEFAULT_ACTOR_NAME);
}

ActorID XG_CreateActorWithName(ScriptEngine* engine, const char* actorName)
{
    SceneGraph* sceneGraph = engine->GetSubsystem<SceneGraph>();
    HierarchyNode* node = CreateNode<HierarchyNode>(*sceneGraph);
    node->TypeMask = HierarchyNode::Mask;
    _SetActorName(node, actorName);
    return node->Id;
}

const char* XG_GetActorName(ScriptEngine* engine, ActorID id)
{
	SceneGraph* sceneGraph = engine ? engine->GetSubsystem<SceneGraph>() : nullptr;
	SceneNode* node = sceneGraph ? sceneGraph->GetNode(id) : nullptr;
	HierarchyNode* hNode = node ? node->Get<HierarchyNode>() : nullptr;
	return hNode ? hNode->Name : nullptr;
}

namespace
{
    SceneNodeHierarchy* HierarchyOf(SceneGraph* sceneGraph, ActorID id)
    {
        SceneNode* node = sceneGraph && id != 0 ? sceneGraph->GetNode(id) : nullptr;
        return node ? node->Get<SceneNodeHierarchy>() : nullptr;
    }
}

int XG_ActorGetRoots(ScriptEngine* engine, ActorID* out, int capacity)
{
    SceneGraph* sceneGraph = engine ? engine->GetSubsystem<SceneGraph>() : nullptr;
    return sceneGraph ? sceneGraph->CopyRootIds(out, capacity) : 0;
}

int XG_ActorGetChildren(ScriptEngine* engine, ActorID id, ActorID* out, int capacity)
{
    SceneNodeHierarchy* node = HierarchyOf(engine ? engine->GetSubsystem<SceneGraph>() : nullptr, id);
    if (!node)
        return 0;
    int count = 0;
    for (SceneNodeHierarchy* child = node->FirstChild; child; child = child->NextSibling)
    {
        if (out && count < capacity)
            out[count] = child->Owner->Id;
        ++count;
    }
    return count;
}

ActorID XG_ActorGetParent(ScriptEngine* engine, ActorID id)
{
    SceneNodeHierarchy* node = HierarchyOf(engine ? engine->GetSubsystem<SceneGraph>() : nullptr, id);
    return node && node->Parent ? node->Parent->Owner->Id : 0;
}

bool XG_ActorSetParent(ScriptEngine* engine, ActorID child, ActorID parent)
{
    SceneGraph* sceneGraph = engine ? engine->GetSubsystem<SceneGraph>() : nullptr;
    SceneNodeHierarchy* childNode = HierarchyOf(sceneGraph, child);
    if (!childNode)
        return false;
    if (parent == 0)
    {
        sceneGraph->SetParent(childNode->Owner, nullptr);
        return true;
    }
    SceneNodeHierarchy* parentNode = HierarchyOf(sceneGraph, parent);
    if (!parentNode)
        return false;
    for (SceneNodeHierarchy* walk = parentNode; walk; walk = walk->Parent)
    {
        if (walk == childNode)
            return false;
    }
    sceneGraph->SetParent(childNode->Owner, parentNode->Owner);
    return true;
}

void XG_SetActorName(xg::ScriptEngine* engine, xg::ActorID id, const char* newName)
{
    SceneGraph* sceneGraph = engine->GetSubsystem<SceneGraph>();
    SceneNode* node = sceneGraph->GetNode(id);
    HierarchyNode* hNode = node->Get<HierarchyNode>();

    _SetActorName(hNode, newName);
}

void XG_DestroyActor(xg::ScriptEngine* engine, xg::ActorID id)
{
    SceneGraph* sceneGraph = engine->GetSubsystem<SceneGraph>();
    SceneNode* node = sceneGraph ? sceneGraph->GetNode(id) : nullptr;
    if (!node)
        return;

    if (HierarchyNode* hNode = node->Get<HierarchyNode>())
    {
        if (hNode->Name && hNode->Name != DEFAULT_ACTOR_NAME)
            free((void*)hNode->Name);
        hNode->Name = nullptr;
    }

    sceneGraph->DestroyNode(node);
}

void XG_RegisterActorUpdate(xg::ScriptEngine* engine, uint32_t moduleId, void (*update)(float dt))
{
    if (ActorRegistry* actors = engine->GetSubsystem<ActorRegistry>())
        actors->AddUpdateCallback(moduleId, update);
}

void XG_UnregisterActorUpdate(xg::ScriptEngine* engine, uint32_t moduleId)
{
    if (ActorRegistry* actors = engine->GetSubsystem<ActorRegistry>())
        actors->RemoveUpdateCallback(moduleId);
}
