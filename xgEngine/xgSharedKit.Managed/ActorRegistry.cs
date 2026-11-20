// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.

using System.Diagnostics.CodeAnalysis;
using System.Reflection;
using System.Runtime.CompilerServices;
using System.Runtime.Loader;

namespace xg
{
    public static unsafe partial class ActorRegistry
    {
        private static readonly Dictionary<ActorID, Actor> _actors = new();
        private static readonly List<IComponentUpdateGroup> _groups = new();
        private static readonly ConditionalWeakTable<object, Actor> _owners = new();
        private static readonly HashSet<IComponentUpdateGroup> _failedGroups = new();
        private static bool _attached;
        private static uint _moduleId;

        public static int Count => _actors.Count;
        public static IReadOnlyCollection<Actor> Actors => _actors.Values;

        public static long FrameCount { get; private set; }

        public static bool TryGet(ActorID id, [NotNullWhen(true)] out Actor? actor)
        {
            return _actors.TryGetValue(id, out actor);
        }

        public static Actor? GetOwner(object component)
        {
            return _owners.TryGetValue(component, out Actor? owner) ? owner : null;
        }

        internal static void Register(Actor actor) => _actors[actor.ID] = actor;
        internal static void Unregister(Actor actor) => _actors.Remove(actor.ID);
        internal static void SetOwner(object component, Actor owner) => _owners.AddOrUpdate(component, owner);
        internal static void ClearOwner(object component) => _owners.Remove(component);
        internal static void AddGroup(IComponentUpdateGroup group) => _groups.Add(group);

        internal static void Update(float deltaTime)
        {
            int groupCount = _groups.Count;
            for (int i = 0; i < groupCount; i++)
            {
                try
                {
                    _groups[i].Update(deltaTime);
                }
                catch (Exception e)
                {
                    if (_failedGroups.Add(_groups[i]))
                        Debug.LogError($"[ActorRegistry] {_groups[i].GetType().GenericTypeArguments[0].Name}.OnUpdate threw (reported once): {e}");
                }
            }
            for (int i = 0; i < _groups.Count; i++)
                _groups[i].FlushRemovals();
            FrameCount++;
        }

        [UnmanagedCallersOnly(CallConvs = new[] { typeof(CallConvCdecl) })]
        private static void NativeUpdate(float deltaTime)
        {
            try
            {
                Update(deltaTime);
            }
            catch (Exception e)
            {
                Debug.LogException(e);
            }
        }

        private static readonly List<GCHandle> _componentHandles = new();

        private static void FreeComponentHandles()
        {
            foreach (GCHandle handle in _componentHandles)
                handle.Free();
            _componentHandles.Clear();
        }

        [UnmanagedCallersOnly(CallConvs = new[] { typeof(CallConvCdecl) })]
        private static int NativeComponentSource(ulong id, nint* output, int capacity)
        {
            try
            {
                FreeComponentHandles();
                if (!_actors.TryGetValue(new ActorID(id), out Actor? actor))
                    return 0;
                IReadOnlyList<object> components = actor.Components;
                int count = Math.Min(components.Count, output == null ? 0 : capacity);
                for (int i = 0; i < count; i++)
                {
                    GCHandle handle = GCHandle.Alloc(components[i]);
                    _componentHandles.Add(handle);
                    output[i] = GCHandle.ToIntPtr(handle);
                }
                return components.Count;
            }
            catch (Exception e)
            {
                Debug.LogException(e);
                return 0;
            }
        }

        [UnmanagedCallersOnly(CallConvs = new[] { typeof(CallConvCdecl) })]
        private static void NativeActorDestroyed(nint user, ulong id)
        {
            try
            {
                if (!_actors.TryGetValue(new ActorID(id), out Actor? actor))
                    return;
                actor.Release();
                _actors.Remove(actor.ID);
            }
            catch (Exception e)
            {
                Debug.LogException(e);
            }
        }

        [UnmanagedCallersOnly(CallConvs = new[] { typeof(CallConvCdecl) })]
        private static int NativeComponentFactory(ulong id, byte* typeKey, int adopt)
        {
            try
            {
                string? key = Marshal.PtrToStringUTF8((nint)typeKey);
                Type? type = key is null ? null : FindOwnType(key, adopt != 0);
                if (type is null)
                    return 0;
                ActorID actorId = new(id);
                if (!_actors.TryGetValue(actorId, out Actor? actor))
                {
                    if (adopt == 0)
                        return 0;
                    actor = Actor.Adopt(actorId);
                }
                actor.AddComponent(type);
                return 1;
            }
            catch (Exception e)
            {
                Debug.LogException(e);
                return -1;
            }
        }

        // The type named by key ("Full.Name, AssemblyName") in this module's load context. With ownAssemblyOnly, types of
        // the shared kit (loaded by every module) don't count, so only the module that defines the type adopts actors.
        internal static Type? FindOwnType(string key, bool ownAssemblyOnly)
        {
            int comma = key.LastIndexOf(',');
            if (comma <= 0)
                return null;
            string fullName = key[..comma].Trim();
            string assemblyName = key[(comma + 1)..].Trim();
            Assembly shared = typeof(ActorRegistry).Assembly;
            if (ownAssemblyOnly && assemblyName == shared.GetName().Name)
                return null;
            AssemblyLoadContext context = AssemblyLoadContext.GetLoadContext(shared) ?? AssemblyLoadContext.Default;
            foreach (Assembly assembly in context.Assemblies)
                if (assembly.GetName().Name == assemblyName)
                    return assembly.GetType(fullName, false);
            return null;
        }

        internal static void Attach(nint enginePtr, uint moduleId)
        {
            if (_attached || enginePtr == 0)
                return;
            XG_RegisterActorUpdate(enginePtr, moduleId, &NativeUpdate);
            XG_ActorRegisterComponentSource(enginePtr, moduleId, RuntimeFeature.IsDynamicCodeSupported ? 1 : 2, &NativeComponentSource);
            if (RuntimeFeature.IsDynamicCodeSupported)
                XG_ActorRegisterComponentFactory(enginePtr, moduleId, 1, &NativeComponentFactory);
            XG_ActorRegisterDestroyListener(enginePtr, moduleId, &NativeActorDestroyed, 0);
            _moduleId = moduleId;
            _attached = true;
        }

        internal static void Detach(nint enginePtr)
        {
            if (_attached)
            {
                XG_UnregisterActorUpdate(enginePtr, _moduleId);
                XG_ActorUnregisterComponentSource(enginePtr, _moduleId);
                XG_ActorUnregisterComponentFactory(enginePtr, _moduleId);
                XG_ActorUnregisterDestroyListener(enginePtr, _moduleId, &NativeActorDestroyed, 0);
                _attached = false;
            }
            FreeComponentHandles();
            foreach (Actor actor in _actors.Values.ToArray())
                actor.Release();
            _actors.Clear();
            foreach (IComponentUpdateGroup group in _groups)
                group.Clear();
        }

        [LibraryImport("xgGameKernel", EntryPoint = "XG_RegisterActorUpdate")]
        private static partial void XG_RegisterActorUpdate(nint engine, uint moduleId, delegate* unmanaged[Cdecl]<float, void> update);

        [LibraryImport("xgGameKernel", EntryPoint = "XG_UnregisterActorUpdate")]
        private static partial void XG_UnregisterActorUpdate(nint engine, uint moduleId);

        [LibraryImport("xgGameKernel", EntryPoint = "XG_ActorRegisterComponentSource")]
        private static partial void XG_ActorRegisterComponentSource(nint engine, uint moduleId, int runtimeKind, delegate* unmanaged[Cdecl]<ulong, nint*, int, int> source);

        [LibraryImport("xgGameKernel", EntryPoint = "XG_ActorUnregisterComponentSource")]
        private static partial void XG_ActorUnregisterComponentSource(nint engine, uint moduleId);

        [LibraryImport("xgGameKernel", EntryPoint = "XG_ActorRegisterComponentFactory")]
        private static partial void XG_ActorRegisterComponentFactory(nint engine, uint moduleId, int runtimeKind, delegate* unmanaged[Cdecl]<ulong, byte*, int, int> factory);

        [LibraryImport("xgGameKernel", EntryPoint = "XG_ActorUnregisterComponentFactory")]
        private static partial void XG_ActorUnregisterComponentFactory(nint engine, uint moduleId);

        [LibraryImport("xgGameKernel", EntryPoint = "XG_ActorRegisterDestroyListener")]
        private static partial void XG_ActorRegisterDestroyListener(nint engine, uint moduleId, delegate* unmanaged[Cdecl]<nint, ulong, void> listener, nint user);

        [LibraryImport("xgGameKernel", EntryPoint = "XG_ActorUnregisterDestroyListener")]
        private static partial void XG_ActorUnregisterDestroyListener(nint engine, uint moduleId, delegate* unmanaged[Cdecl]<nint, ulong, void> listener, nint user);
    }
}
