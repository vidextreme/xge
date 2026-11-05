// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.

using System.Diagnostics.CodeAnalysis;
using System.Runtime.CompilerServices;

namespace xg
{
    public static unsafe partial class ActorRegistry
    {
        private static readonly Dictionary<ActorID, Actor> s_actors = new();
        private static readonly List<IComponentUpdateGroup> s_groups = new();
        private static readonly ConditionalWeakTable<object, Actor> s_owners = new();
        private static readonly HashSet<IComponentUpdateGroup> s_failedGroups = new();
        private static bool s_attached;
        private static uint s_moduleId;

        public static int Count => s_actors.Count;
        public static IReadOnlyCollection<Actor> Actors => s_actors.Values;

        public static long FrameCount { get; private set; }

        public static bool TryGet(ActorID id, [NotNullWhen(true)] out Actor? actor)
        {
            return s_actors.TryGetValue(id, out actor);
        }

        public static Actor? GetOwner(object component)
        {
            return s_owners.TryGetValue(component, out Actor? owner) ? owner : null;
        }

        internal static void Register(Actor actor) => s_actors[actor.ID] = actor;
        internal static void Unregister(Actor actor) => s_actors.Remove(actor.ID);
        internal static void SetOwner(object component, Actor owner) => s_owners.AddOrUpdate(component, owner);
        internal static void ClearOwner(object component) => s_owners.Remove(component);
        internal static void AddGroup(IComponentUpdateGroup group) => s_groups.Add(group);

        internal static void Update(float deltaTime)
        {
            int groupCount = s_groups.Count;
            for (int i = 0; i < groupCount; i++)
            {
                try
                {
                    s_groups[i].Update(deltaTime);
                }
                catch (Exception e)
                {
                    if (s_failedGroups.Add(s_groups[i]))
                        Debug.LogError($"[ActorRegistry] {s_groups[i].GetType().GenericTypeArguments[0].Name}.OnUpdate threw (reported once): {e}");
                }
            }
            for (int i = 0; i < s_groups.Count; i++)
                s_groups[i].FlushRemovals();
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
            if (s_attached || enginePtr == 0)
                return;
            XGRegisterActorUpdate(enginePtr, moduleId, &NativeUpdate);
            s_moduleId = moduleId;
            s_attached = true;
        }

        internal static void Detach(nint enginePtr)
        {
            if (s_attached)
            {
                XGUnregisterActorUpdate(enginePtr, s_moduleId);
                s_attached = false;
            }
            foreach (Actor actor in s_actors.Values.ToArray())
                actor.Release();
            s_actors.Clear();
            foreach (IComponentUpdateGroup group in s_groups)
                group.Clear();
        }

        [LibraryImport("xgGameKernel", EntryPoint = "XGRegisterActorUpdate")]
        private static partial void XGRegisterActorUpdate(nint engine, uint moduleId, delegate* unmanaged[Cdecl]<float, void> update);

        [LibraryImport("xgGameKernel", EntryPoint = "XGUnregisterActorUpdate")]
        private static partial void XGUnregisterActorUpdate(nint engine, uint moduleId);
    }
}
