#pragma once
#include "xgSceneNode.h"

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
