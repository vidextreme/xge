#include "pch.h"
#include "InteropSystem.h"

XG_MODULE_EXPORT xg::System* XGRegiterSystemWithAlias(xg::ScriptEngine* engine, uint32_t moduleID, void* instancePtr, const char* systemName, xg::ManagedFn initFunc, xg::ManagedFn shutdownFunc, xg::ManagedUpdateFn updateFunc)
{
	auto module = engine->GetScriptModule(moduleID);
	xg::InteropSystem* system = new xg::InteropSystem(instancePtr, initFunc, shutdownFunc, updateFunc);
	engine->RegisterSystemWithAlias(module, system, systemName);
	return system;
}