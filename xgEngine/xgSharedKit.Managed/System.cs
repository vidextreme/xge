// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.

namespace xg
{
    public interface ISystem
    {
        void Initialize();
        void Update(float dt);
        void Shutdown();
    }

    public class SystemBridge
    {
        [UnmanagedCallersOnly]
        public static void Initialize(nint instance)
        {
            var sys = (ISystem)GCHandle.FromIntPtr(instance).Target!;
            sys.Initialize();
        }
        [UnmanagedCallersOnly]
        public static void Shutdown(nint instance)
        {
            var sys = (ISystem)GCHandle.FromIntPtr(instance).Target!;
            sys.Shutdown();
        }
        [UnmanagedCallersOnly]
        public static void Update(nint instance, float dt)
        {
            var sys = (ISystem)GCHandle.FromIntPtr(instance).Target!;
            sys.Update(dt);
        }
    }

    public class xgSystem : ISystem
    {

        public void Initialize()
        {

        }

        public void Shutdown()
        {

        }

        public void Update(float dt)
        {

        }
    }
}