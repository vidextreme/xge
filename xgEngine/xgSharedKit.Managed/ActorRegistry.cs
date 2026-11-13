// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.

using System.Diagnostics.CodeAnalysis;
using System.Runtime.CompilerServices;

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

        internal static void Attach(nint enginePtr, uint moduleId)
        {
            if (_attached || enginePtr == 0)
                return;
            XG_RegisterActorUpdate(enginePtr, moduleId, &NativeUpdate);
            XG_ActorRegisterComponentSource(enginePtr, moduleId, RuntimeFeature.IsDynamicCodeSupported ? 1 : 2, &NativeComponentSource);
            _moduleId = moduleId;
            _attached = true;
        }

        internal static void Detach(nint enginePtr)
        {
            if (_attached)
            {
                XG_UnregisterActorUpdate(enginePtr, _moduleId);
                XG_ActorUnregisterComponentSource(enginePtr, _moduleId);
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
    }
}
