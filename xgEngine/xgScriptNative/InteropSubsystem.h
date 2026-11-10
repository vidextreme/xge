#pragma once

#include "xgSubsystem.h"
namespace xg
{
    enum class SystemPhase : uint8_t
    {
        Update,
        Fixed,
        Render,
        // etc.
    };

    using ManagedFn = void(*)(void* instance);
    using ManagedUpdateFn = void(*)(void* instance, float dt);
    //using ManagedDispatchFn = void(*)(SystemPhase phase, float dt);
	class InteropSubsystem : public Subsystem
	{
		XG_DECLARE_BASE_SUBSYSTEM(InteropSubsystem)
	public:
        InteropSubsystem(void* instance, ManagedFn initFunc, ManagedFn shutdownFunc, ManagedUpdateFn updateFunc);
        void Initialize() override;
        void Shutdown() override;
        void Update();
    private:
        void* _instanceInterop;
        ManagedFn _initializeFunc;
        ManagedFn _shutdownFunc;

        ManagedUpdateFn _updateFunc;
        bool _initialized = false;
	};
}