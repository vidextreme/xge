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

        internal static void Attach(nint enginePtr, uint moduleId)
        {
            if (_attached || enginePtr == 0)
                return;
            XGRegisterActorUpdate(enginePtr, moduleId, &NativeUpdate);
            _moduleId = moduleId;
            _attached = true;
        }

        internal static void Detach(nint enginePtr)
        {
            if (_attached)
            {
                XGUnregisterActorUpdate(enginePtr, _moduleId);
                _attached = false;
            }
            foreach (Actor actor in _actors.Values.ToArray())
                actor.Release();
            _actors.Clear();
            foreach (IComponentUpdateGroup group in _groups)
                group.Clear();
        }

        [LibraryImport("xgGameKernel", EntryPoint = "XGRegisterActorUpdate")]
        private static partial void XGRegisterActorUpdate(nint engine, uint moduleId, delegate* unmanaged[Cdecl]<float, void> update);

        [LibraryImport("xgGameKernel", EntryPoint = "XGUnregisterActorUpdate")]
        private static partial void XGUnregisterActorUpdate(nint engine, uint moduleId);
    }
}
