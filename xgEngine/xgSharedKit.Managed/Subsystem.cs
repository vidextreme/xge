// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.

namespace xg
{
    public interface ISubsystem
    {
        void Initialize();
        void Update(float dt);
        void Shutdown();
    }

    // Native → managed trampolines for InteropSubsystem. Exceptions are reported here, never thrown into native code.
    public static class SubsystemBridge
    {
        [UnmanagedCallersOnly]
        public static void Initialize(nint instance)
        {
            try
            {
                Resolve(instance)?.Initialize();
            }
            catch (Exception e)
            {
                Debug.LogException(e);
            }
        }

        [UnmanagedCallersOnly]
        public static void Shutdown(nint instance)
        {
            try
            {
                Resolve(instance)?.Shutdown();
            }
            catch (Exception e)
            {
                Debug.LogException(e);
            }
        }

        [UnmanagedCallersOnly]
        public static void Update(nint instance, float dt)
        {
            try
            {
                Resolve(instance)?.Update(dt);
            }
            catch (Exception e)
            {
                Debug.LogException(e);
            }
        }

        private static ISubsystem? Resolve(nint instance) =>
            instance == 0 ? null : GCHandle.FromIntPtr(instance).Target as ISubsystem;
    }

    // Managed subsystems registered on this module's node, keyed by alias. Native lookup walks up the module tree,
    // so a name registered here overrides the same alias on parent modules. ScriptRuntime.Shutdown calls UnregisterAll.
    public static class Subsystem
    {
        private readonly struct Entry
        {
            public Entry(ISubsystem instance, GCHandle handle)
            {
                Instance = instance;
                Handle = handle;
            }

            public ISubsystem Instance { get; }
            public GCHandle Handle { get; }
        }

        private static readonly Dictionary<string, Entry> _entries = new();
        private static readonly List<string> _order = new();

        public static IReadOnlyCollection<string> Names => _order;

        public static T Register<T>(string name) where T : ISubsystem, new()
        {
            if (_entries.TryGetValue(name, out Entry existing))
                return Existing<T>(name, existing);
            return Register(new T(), name);
        }

        public static unsafe T Register<T>(T subsystem, string name) where T : ISubsystem
        {
            ArgumentNullException.ThrowIfNull(subsystem);
            ArgumentException.ThrowIfNullOrEmpty(name);

            if (_entries.TryGetValue(name, out Entry existing))
                return Existing<T>(name, existing);

            GCHandle handle = GCHandle.Alloc(subsystem, GCHandleType.Normal);
            IntPtr native = ScriptRuntime.RegisterSubsystemWithAlias(ScriptRuntime._enginePtr, ScriptRuntime.ModuleID,
                GCHandle.ToIntPtr(handle), name, &SubsystemBridge.Initialize, &SubsystemBridge.Shutdown, &SubsystemBridge.Update);
            if (native == IntPtr.Zero)
            {
                handle.Free();
                Debug.LogError($"Subsystem '{name}' could not be registered (engine not initialized or module not found).");
                return subsystem;
            }

            _entries.Add(name, new Entry(subsystem, handle));
            _order.Add(name);
            return subsystem;
        }

        public static bool Unregister(string name)
        {
            if (!_entries.TryGetValue(name, out Entry entry))
                return false;

            _entries.Remove(name);
            _order.Remove(name);
            try
            {
                if (ScriptRuntime._enginePtr != 0)
                    ScriptRuntime.UnregisterSubsystemWithAlias(ScriptRuntime._enginePtr, ScriptRuntime.ModuleID, name);
            }
            finally
            {
                if (entry.Handle.IsAllocated)
                    entry.Handle.Free();
            }
            return true;
        }

        public static bool Unregister(ISubsystem subsystem)
        {
            foreach (string name in _order)
            {
                if (ReferenceEquals(_entries[name].Instance, subsystem))
                    return Unregister(name);
            }
            return false;
        }

        public static void UnregisterAll()
        {
            for (int i = _order.Count - 1; i >= 0; --i)
            {
                try
                {
                    Unregister(_order[i]);
                }
                catch (Exception e)
                {
                    Debug.LogException(e);
                }
            }
            _entries.Clear();
            _order.Clear();
        }

        public static T? Get<T>(string name) where T : class, ISubsystem =>
            _entries.TryGetValue(name, out Entry entry) ? entry.Instance as T : null;

        public static bool IsRegistered(string name) => _entries.ContainsKey(name);

        private static T Existing<T>(string name, Entry existing)
        {
            if (existing.Instance is T same)
            {
                Debug.LogError($"Subsystem '{name}' is already registered; returning the existing instance.");
                return same;
            }
            throw new InvalidOperationException(
                $"Subsystem '{name}' is already registered as {existing.Instance.GetType().Name}, not {typeof(T).Name}.");
        }
    }
}
