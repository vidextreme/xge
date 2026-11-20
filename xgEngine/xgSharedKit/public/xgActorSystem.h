#pragma once
#include "xgSceneNode.h"
#include "xgTransforms.h"

XG_MODULE_EXPORT xg::ActorID XG_CreateActor(xg::ScriptEngine* engine);
XG_MODULE_EXPORT xg::ActorID XG_CreateActorWithName(xg::ScriptEngine* engine, const char* actorName);
XG_MODULE_EXPORT const char* XG_GetActorName(xg::ScriptEngine* engine, xg::ActorID id);
XG_MODULE_EXPORT void XG_SetActorName(xg::ScriptEngine* engine, xg::ActorID id, const char* newName);
XG_MODULE_EXPORT void XG_DestroyActor(xg::ScriptEngine* engine, xg::ActorID id);

// Actor tree (root SceneGraph). The Get functions write up to capacity ids and return the total count.
XG_MODULE_EXPORT int XG_ActorGetRoots(xg::ScriptEngine* engine, xg::ActorID* out, int capacity);
XG_MODULE_EXPORT int XG_ActorGetChildren(xg::ScriptEngine* engine, xg::ActorID id, xg::ActorID* out, int capacity);
// Returns 0 when the actor has no parent or does not exist.
XG_MODULE_EXPORT xg::ActorID XG_ActorGetParent(xg::ScriptEngine* engine, xg::ActorID id);
// parent 0 makes the actor a root. Returns false for unknown ids or when parent is the actor or one of its descendants.
XG_MODULE_EXPORT bool XG_ActorSetParent(xg::ScriptEngine* engine, xg::ActorID child, xg::ActorID parent);
// keepWorld keeps the child's world pose when it has a transform (XG_ActorSetParent passes true).
XG_MODULE_EXPORT bool XG_ActorSetParentEx(xg::ScriptEngine* engine, xg::ActorID child, xg::ActorID parent, bool keepWorld);

// Destroy listeners run once per destroyed actor (children first) while its node still exists; removed with the module.
typedef void (*XG_ActorDestroyedFn)(void* user, xg::ActorID id);
XG_MODULE_EXPORT void XG_ActorRegisterDestroyListener(xg::ScriptEngine* engine, uint32_t moduleId, XG_ActorDestroyedFn fn, void* user);
XG_MODULE_EXPORT void XG_ActorUnregisterDestroyListener(xg::ScriptEngine* engine, uint32_t moduleId, XG_ActorDestroyedFn fn, void* user);

// Transforms (xg::TransformSystem). Calls return false / 0 for actors without a transform.
XG_MODULE_EXPORT bool XG_TransformAdd(xg::ScriptEngine* engine, xg::ActorID id);
XG_MODULE_EXPORT bool XG_TransformRemove(xg::ScriptEngine* engine, xg::ActorID id);
XG_MODULE_EXPORT bool XG_TransformHas(xg::ScriptEngine* engine, xg::ActorID id);
XG_MODULE_EXPORT bool XG_TransformGetLocal(xg::ScriptEngine* engine, xg::ActorID id, xg::TransformData* out);
XG_MODULE_EXPORT bool XG_TransformSetLocal(xg::ScriptEngine* engine, xg::ActorID id, const xg::TransformData* value);
XG_MODULE_EXPORT bool XG_TransformGetWorld(xg::ScriptEngine* engine, xg::ActorID id, xg::TransformData* out);
XG_MODULE_EXPORT bool XG_TransformSetWorld(xg::ScriptEngine* engine, xg::ActorID id, const xg::TransformData* value);
// Row-major, row vectors (v * M).
XG_MODULE_EXPORT bool XG_TransformGetWorldMatrix(xg::ScriptEngine* engine, xg::ActorID id, float* out16);
XG_MODULE_EXPORT int XG_TransformGetLocals(xg::ScriptEngine* engine, const xg::ActorID* ids, int count, xg::TransformData* out);
XG_MODULE_EXPORT int XG_TransformSetLocals(xg::ScriptEngine* engine, const xg::ActorID* ids, int count, const xg::TransformData* values);
XG_MODULE_EXPORT int XG_TransformGetWorlds(xg::ScriptEngine* engine, const xg::ActorID* ids, int count, xg::TransformData* out);
XG_MODULE_EXPORT int XG_TransformSetWorlds(xg::ScriptEngine* engine, const xg::ActorID* ids, int count, const xg::TransformData* values);
XG_MODULE_EXPORT uint64_t XG_TransformGetVersion(xg::ScriptEngine* engine);
// Writes up to capacity actors whose transform changed after since and returns the total count.
XG_MODULE_EXPORT int XG_TransformCollectChanged(xg::ScriptEngine* engine, uint64_t since, xg::ActorID* out, int capacity);
// Writes up to capacity actors that have a transform and returns the total count.
XG_MODULE_EXPORT int XG_TransformGetActors(xg::ScriptEngine* engine, xg::ActorID* out, int capacity);

XG_MODULE_EXPORT void XG_RegisterActorUpdate(xg::ScriptEngine* engine, uint32_t moduleId, void (*update)(float dt));
XG_MODULE_EXPORT void XG_UnregisterActorUpdate(xg::ScriptEngine* engine, uint32_t moduleId);

// Component sources: each managed module registers a callback that writes GCHandle values (as intptr_t) for an actor's
// components. runtimeKind tells which runtime owns the handles (1 = CoreCLR, 2 = NativeAOT); handles are only valid
// inside that runtime and stay valid until the source's next call.
typedef int (*XG_ActorComponentSourceFn)(xg::ActorID id, intptr_t* out, int capacity);
XG_MODULE_EXPORT void XG_ActorRegisterComponentSource(xg::ScriptEngine* engine, uint32_t moduleId, int runtimeKind, XG_ActorComponentSourceFn source);
XG_MODULE_EXPORT void XG_ActorUnregisterComponentSource(xg::ScriptEngine* engine, uint32_t moduleId);
// Writes up to capacity handles from every source of runtimeKind and returns the total count.
XG_MODULE_EXPORT int XG_ActorGetComponentHandles(xg::ScriptEngine* engine, xg::ActorID id, int runtimeKind, intptr_t* out, int capacity);

// Component factories: each managed module registers a callback that adds a component, given its type key
// ("Full.Name, AssemblyName"), to an actor. adopt = 0 asks only modules that already have the actor; adopt = 1 lets a
// module whose own assembly defines the type take the actor id. Returns 1 when added, 0 when not handled, -1 on error.
typedef int (*XG_ActorComponentFactoryFn)(xg::ActorID id, const char* typeKey, int adopt);
XG_MODULE_EXPORT void XG_ActorRegisterComponentFactory(xg::ScriptEngine* engine, uint32_t moduleId, int runtimeKind, XG_ActorComponentFactoryFn factory);
XG_MODULE_EXPORT void XG_ActorUnregisterComponentFactory(xg::ScriptEngine* engine, uint32_t moduleId);
// Adds a component of typeKey to the actor through the factories of runtimeKind: first a module that has the actor,
// then a module that defines the type. Returns 1 when added, 0 when no module handled it, -1 on error.
XG_MODULE_EXPORT int XG_ActorAddComponent(xg::ScriptEngine* engine, xg::ActorID id, const char* typeKey, int runtimeKind);
