#include "pch.h"
#include "InteropSubsystem.h"

XG_MODULE_EXPORT xg::Subsystem* XG_RegisterSubsystemWithAlias(xg::ScriptEngine* engine, uint32_t moduleID, void* instancePtr, const char* subsystemName, xg::ManagedFn initFunc, xg::ManagedFn shutdownFunc, xg::ManagedUpdateFn updateFunc)
{
	if (!engine || !subsystemName)
		return nullptr;
	xg::ScriptModule* module = engine->GetScriptModule(moduleID);
	if (!module)
		return nullptr;
	xg::InteropSubsystem* system = new xg::InteropSubsystem(instancePtr, initFunc, shutdownFunc, updateFunc);
	engine->RegisterSubsystemWithAlias(module, system, subsystemName);
	return system;
}

XG_MODULE_EXPORT bool XG_UnregisterSubsystemWithAlias(xg::ScriptEngine* engine, uint32_t moduleID, const char* subsystemName)
{
	if (!engine || !subsystemName)
		return false;
	xg::ScriptModule* module = engine->GetScriptModule(moduleID);
	if (!module)
		return false;
	return engine->UnregisterSubsystemWithAlias(module, subsystemName);
}