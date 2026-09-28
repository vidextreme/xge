#include "pch.h"
#include "InteropSystem.h"

xg::InteropSystem::InteropSystem(void* instance, ManagedFn initFunc, ManagedFn shutdownFunc, ManagedUpdateFn updateFunc)
	: _instanceInterop(instance), _initializeFunc(initFunc), _shutdownFunc(shutdownFunc),
		_updateFunc(updateFunc)
{
}

void xg::InteropSystem::Initialize()
{
	_initializeFunc(_instanceInterop);
}

void xg::InteropSystem::Shutdown()
{
	_shutdownFunc(_instanceInterop);
}

void xg::InteropSystem::Update()
{
	_updateFunc(_instanceInterop, 100);
}
