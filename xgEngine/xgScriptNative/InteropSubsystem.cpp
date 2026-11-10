#include "pch.h"
#include "InteropSubsystem.h"

xg::InteropSubsystem::InteropSubsystem(void* instance, ManagedFn initFunc, ManagedFn shutdownFunc, ManagedUpdateFn updateFunc)
	: _instanceInterop(instance), _initializeFunc(initFunc), _shutdownFunc(shutdownFunc),
		_updateFunc(updateFunc)
{
}

void xg::InteropSubsystem::Initialize()
{
	if (_initialized)
		return;
	_initialized = true;
	_initializeFunc(_instanceInterop);
}

void xg::InteropSubsystem::Shutdown()
{
	if (!_initialized)
		return;
	_initialized = false;
	_shutdownFunc(_instanceInterop);
}

void xg::InteropSubsystem::Update()
{
	_updateFunc(_instanceInterop, 100);
}
