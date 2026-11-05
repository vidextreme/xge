#pragma once
#include "xgSceneNode.h"

XG_MODULE_EXPORT xg::ActorID XGCreateActor(xg::ScriptEngine* engine);
XG_MODULE_EXPORT xg::ActorID XGCreateActorWithName(xg::ScriptEngine* engine, const char* actorName);
XG_MODULE_EXPORT const char* XGGetActorName(xg::ScriptEngine* engine, xg::ActorID id);
XG_MODULE_EXPORT void XGSetActorName(xg::ScriptEngine* engine, xg::ActorID id, const char* newName);
XG_MODULE_EXPORT void XGDestroyActor(xg::ScriptEngine* engine, xg::ActorID id);

XG_MODULE_EXPORT void XGRegisterActorUpdate(xg::ScriptEngine* engine, uint32_t moduleId, void (*update)(float dt));
XG_MODULE_EXPORT void XGUnregisterActorUpdate(xg::ScriptEngine* engine, uint32_t moduleId);