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
    if (hNode->OwnsName)
        free(const_cast<char*>(hNode->Name));
    hNode->Name = nullptr;
    hNode->OwnsName = false;

    if (!newName || strcmp(newName, DEFAULT_ACTOR_NAME) == 0)
    {
        hNode->Name = DEFAULT_ACTOR_NAME;
        return;
    }

    const size_t len = strlen(newName);
    char* buffer = static_cast<char*>(malloc(len + 1));
    memcpy(buffer, newName, len + 1);
    hNode->Name = buffer;
    hNode->OwnsName = true;
}

ActorID XG_CreateActor(ScriptEngine* engine)
{
    return XG_CreateActorWithName(engine, DEFAULT_ACTOR_NAME);
}

ActorID XG_CreateActorWithName(ScriptEngine* engine, const char* actorName)
{
    SceneGraph* sceneGraph = engine->GetSubsystem<SceneGraph>();
    HierarchyNode* node = CreateNode<HierarchyNode>(*sceneGraph);
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
    return XG_ActorSetParentEx(engine, child, parent, true);
}

bool XG_ActorSetParentEx(ScriptEngine* engine, ActorID child, ActorID parent, bool keepWorld)
{
    TransformSystem* transforms = engine ? engine->GetSubsystem<TransformSystem>() : nullptr;
    if (transforms)
        return transforms->SetParent(child, parent, keepWorld);
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
    SceneGraph* sceneGraph = engine ? engine->GetSubsystem<SceneGraph>() : nullptr;
    SceneNode* node = sceneGraph ? sceneGraph->GetNode(id) : nullptr;
    if (HierarchyNode* hNode = node ? node->Get<HierarchyNode>() : nullptr)
        _SetActorName(hNode, newName);
}

void XG_DestroyActor(xg::ScriptEngine* engine, xg::ActorID id)
{
    SceneGraph* sceneGraph = engine ? engine->GetSubsystem<SceneGraph>() : nullptr;
    SceneNode* node = sceneGraph ? sceneGraph->GetNode(id) : nullptr;
    if (node)
        sceneGraph->DestroyNode(node);
}

void XG_ActorRegisterDestroyListener(xg::ScriptEngine* engine, uint32_t moduleId, XG_ActorDestroyedFn fn, void* user)
{
    if (ActorRegistry* actors = engine ? engine->GetSubsystem<ActorRegistry>() : nullptr)
        actors->AddDestroyListener(moduleId, fn, user);
}

void XG_ActorUnregisterDestroyListener(xg::ScriptEngine* engine, uint32_t moduleId, XG_ActorDestroyedFn fn, void* user)
{
    if (ActorRegistry* actors = engine ? engine->GetSubsystem<ActorRegistry>() : nullptr)
        actors->RemoveDestroyListener(moduleId, fn, user);
}

namespace
{
    TransformSystem* TransformsOf(ScriptEngine* engine)
    {
        return engine ? engine->GetSubsystem<TransformSystem>() : nullptr;
    }
}

bool XG_TransformAdd(xg::ScriptEngine* engine, xg::ActorID id)
{
    TransformSystem* t = TransformsOf(engine);
    return t && t->Add(id);
}

bool XG_TransformRemove(xg::ScriptEngine* engine, xg::ActorID id)
{
    TransformSystem* t = TransformsOf(engine);
    return t && t->Remove(id);
}

bool XG_TransformHas(xg::ScriptEngine* engine, xg::ActorID id)
{
    TransformSystem* t = TransformsOf(engine);
    return t && t->Has(id);
}

bool XG_TransformGetLocal(xg::ScriptEngine* engine, xg::ActorID id, xg::TransformData* out)
{
    TransformSystem* t = TransformsOf(engine);
    return t && t->GetLocal(id, out);
}

bool XG_TransformSetLocal(xg::ScriptEngine* engine, xg::ActorID id, const xg::TransformData* value)
{
    TransformSystem* t = TransformsOf(engine);
    return t && t->SetLocal(id, value);
}

bool XG_TransformGetWorld(xg::ScriptEngine* engine, xg::ActorID id, xg::TransformData* out)
{
    TransformSystem* t = TransformsOf(engine);
    return t && t->GetWorld(id, out);
}

bool XG_TransformSetWorld(xg::ScriptEngine* engine, xg::ActorID id, const xg::TransformData* value)
{
    TransformSystem* t = TransformsOf(engine);
    return t && t->SetWorld(id, value);
}

bool XG_TransformGetWorldMatrix(xg::ScriptEngine* engine, xg::ActorID id, float* out16)
{
    TransformSystem* t = TransformsOf(engine);
    return t && t->GetWorldMatrix(id, out16);
}

int XG_TransformGetLocals(xg::ScriptEngine* engine, const xg::ActorID* ids, int count, xg::TransformData* out)
{
    TransformSystem* t = TransformsOf(engine);
    return t ? t->GetLocals(ids, count, out) : 0;
}

int XG_TransformSetLocals(xg::ScriptEngine* engine, const xg::ActorID* ids, int count, const xg::TransformData* values)
{
    TransformSystem* t = TransformsOf(engine);
    return t ? t->SetLocals(ids, count, values) : 0;
}

int XG_TransformGetWorlds(xg::ScriptEngine* engine, const xg::ActorID* ids, int count, xg::TransformData* out)
{
    TransformSystem* t = TransformsOf(engine);
    return t ? t->GetWorlds(ids, count, out) : 0;
}

int XG_TransformSetWorlds(xg::ScriptEngine* engine, const xg::ActorID* ids, int count, const xg::TransformData* values)
{
    TransformSystem* t = TransformsOf(engine);
    return t ? t->SetWorlds(ids, count, values) : 0;
}

uint64_t XG_TransformGetVersion(xg::ScriptEngine* engine)
{
    TransformSystem* t = TransformsOf(engine);
    return t ? t->GetCurrentVersion() : 0;
}

int XG_TransformCollectChanged(xg::ScriptEngine* engine, uint64_t since, xg::ActorID* out, int capacity)
{
    TransformSystem* t = TransformsOf(engine);
    return t ? t->CollectChanged(since, out, capacity) : 0;
}

int XG_TransformGetActors(xg::ScriptEngine* engine, xg::ActorID* out, int capacity)
{
    TransformSystem* t = TransformsOf(engine);
    return t ? t->CopyActors(out, capacity) : 0;
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

namespace xg
{
    namespace
    {
        struct ComponentSource
        {
            ScriptEngine* Engine;
            uint32_t ModuleId;
            int RuntimeKind;
            XG_ActorComponentSourceFn Fn;
        };

        std::vector<ComponentSource> _componentSources;

        struct ComponentFactory
        {
            ScriptEngine* Engine;
            uint32_t ModuleId;
            int RuntimeKind;
            XG_ActorComponentFactoryFn Fn;
        };

        std::vector<ComponentFactory> _componentFactories;
    }
}

void XG_ActorRegisterComponentSource(xg::ScriptEngine* engine, uint32_t moduleId, int runtimeKind, XG_ActorComponentSourceFn source)
{
    XG_ActorUnregisterComponentSource(engine, moduleId);
    if (source)
        xg::_componentSources.push_back({ engine, moduleId, runtimeKind, source });
}

void XG_ActorUnregisterComponentSource(xg::ScriptEngine* engine, uint32_t moduleId)
{
    std::erase_if(xg::_componentSources, [&](const xg::ComponentSource& s) { return s.Engine == engine && s.ModuleId == moduleId; });
}

void XG_ActorRegisterComponentFactory(xg::ScriptEngine* engine, uint32_t moduleId, int runtimeKind, XG_ActorComponentFactoryFn factory)
{
    XG_ActorUnregisterComponentFactory(engine, moduleId);
    if (factory)
        xg::_componentFactories.push_back({ engine, moduleId, runtimeKind, factory });
}

void XG_ActorUnregisterComponentFactory(xg::ScriptEngine* engine, uint32_t moduleId)
{
    std::erase_if(xg::_componentFactories, [&](const xg::ComponentFactory& f) { return f.Engine == engine && f.ModuleId == moduleId; });
}

int XG_ActorAddComponent(xg::ScriptEngine* engine, xg::ActorID id, const char* typeKey, int runtimeKind)
{
    if (!typeKey || !typeKey[0])
        return -1;
    const std::vector<xg::ComponentFactory> factories = xg::_componentFactories;
    for (int adopt = 0; adopt <= 1; ++adopt)
    {
        for (const xg::ComponentFactory& factory : factories)
        {
            if (factory.Engine != engine || factory.RuntimeKind != runtimeKind)
                continue;
            const int result = factory.Fn(id, typeKey, adopt);
            if (result != 0)
                return result;
        }
    }
    return 0;
}

int XG_ActorGetComponentHandles(xg::ScriptEngine* engine, xg::ActorID id, int runtimeKind, intptr_t* out, int capacity)
{
    int total = 0;
    for (const xg::ComponentSource& source : xg::_componentSources)
    {
        if (source.Engine != engine || source.RuntimeKind != runtimeKind)
            continue;
        int room = out ? (std::max)(capacity - total, 0) : 0;
        total += source.Fn(id, room > 0 ? out + total : nullptr, room);
    }
    return total;
}
