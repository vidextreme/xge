// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.

using System.Runtime.CompilerServices;

namespace xg
{
    // Read access to the engine's actor hierarchy (the root SceneGraph), covering actors from every module.
    // Ids can disappear between frames; GetName returns an empty string for destroyed actors.
    public static unsafe partial class ActorTree
    {
        [ThreadStatic] private static ulong[]? _buffer;

        public static void GetRoots(List<ActorID> result)
        {
            result.Clear();
            nint engine = ScriptRuntime._enginePtr;
            if (engine == 0)
                return;
            ulong[] buffer = Buffer(64);
            int count;
            fixed (ulong* p = buffer)
                count = XG_ActorGetRoots(engine, p, buffer.Length);
            if (count > buffer.Length)
            {
                buffer = Buffer(count);
                fixed (ulong* p = buffer)
                    count = Math.Min(XG_ActorGetRoots(engine, p, buffer.Length), buffer.Length);
            }
            Append(result, buffer, count);
        }

        public static void GetChildren(ActorID id, List<ActorID> result)
        {
            result.Clear();
            nint engine = ScriptRuntime._enginePtr;
            if (engine == 0)
                return;
            ulong[] buffer = Buffer(16);
            int count;
            fixed (ulong* p = buffer)
                count = XG_ActorGetChildren(engine, id, p, buffer.Length);
            if (count > buffer.Length)
            {
                buffer = Buffer(count);
                fixed (ulong* p = buffer)
                    count = Math.Min(XG_ActorGetChildren(engine, id, p, buffer.Length), buffer.Length);
            }
            Append(result, buffer, count);
        }

        public static int ChildCount(ActorID id)
        {
            nint engine = ScriptRuntime._enginePtr;
            return engine == 0 ? 0 : XG_ActorGetChildren(engine, id, null, 0);
        }

        // Returns null when the actor has no parent or does not exist.
        public static ActorID? GetParent(ActorID id)
        {
            nint engine = ScriptRuntime._enginePtr;
            ulong parent = engine == 0 ? 0 : XG_ActorGetParent(engine, id);
            return parent == 0 ? null : new ActorID(parent);
        }

        // parent null makes the actor a root. Returns false for unknown ids or a parent inside the actor's own subtree.
        public static bool SetParent(ActorID child, ActorID? parent) => SetParent(child, parent, true);

        // keepWorld keeps the child's world pose when it has a transform.
        public static bool SetParent(ActorID child, ActorID? parent, bool keepWorld)
        {
            nint engine = ScriptRuntime._enginePtr;
            return engine != 0 && XG_ActorSetParentEx(engine, child, parent?.Value ?? 0, keepWorld ? (byte)1 : (byte)0) != 0;
        }

        public static string GetName(ActorID id)
        {
            nint engine = ScriptRuntime._enginePtr;
            nint name = engine == 0 ? 0 : Actor.ActorUtils.XG_GetActorName(engine, id);
            return Marshal.PtrToStringUTF8(name) ?? string.Empty;
        }

        // Live component objects of an actor from every CoreCLR module (they share this runtime), including other
        // modules' components. Their types may come from another load context, so compare them by name, not identity.
        public static void GetComponents(ActorID id, List<object> result)
        {
            result.Clear();
            nint engine = ScriptRuntime._enginePtr;
            if (engine == 0)
                return;
            if (Transform.Exists(id))
                result.Add(Transform.Of(id));
            if (!RuntimeFeature.IsDynamicCodeSupported)
                return;
            nint* handles = stackalloc nint[64];
            int count = XG_ActorGetComponentHandles(engine, id, 1, handles, 64);
            if (count > 64)
            {
                nint[] large = new nint[count];
                fixed (nint* p = large)
                {
                    count = Math.Min(XG_ActorGetComponentHandles(engine, id, 1, p, count), count);
                    Resolve(p, count, result);
                }
                return;
            }
            Resolve(handles, count, result);
        }

        // Adds a component of type to the actor, through whichever CoreCLR module owns the actor or defines the type (types
        // may come from any load context). Returns false when no module could add it; errors are logged by the module.
        public static bool AddComponent(ActorID id, Type type)
        {
            nint engine = ScriptRuntime._enginePtr;
            if (engine == 0)
                return false;
            if (type == typeof(Transform) || type.FullName == typeof(Transform).FullName)
                return Transform.Exists(id) || Transform.Add(id);
            if (!RuntimeFeature.IsDynamicCodeSupported)
                return false;
            string key = $"{type.FullName}, {type.Assembly.GetName().Name}";
            return XG_ActorAddComponent(engine, id, key, 1) == 1;
        }

        // Adds a component by its type key ("Full.Name, Assembly"), through the module that owns the actor or defines the
        // type. Returns false when no module could add it.
        public static bool AddComponent(ActorID id, string typeKey)
        {
            nint engine = ScriptRuntime._enginePtr;
            if (engine == 0 || string.IsNullOrEmpty(typeKey) || !RuntimeFeature.IsDynamicCodeSupported)
                return false;
            return XG_ActorAddComponent(engine, id, typeKey, 1) == 1;
        }

        private static void Resolve(nint* handles, int count, List<object> result)
        {
            for (int i = 0; i < count; ++i)
            {
                object? target = GCHandle.FromIntPtr(handles[i]).Target;
                if (target is not null)
                    result.Add(target);
            }
        }

        private static ulong[] Buffer(int size)
        {
            if (_buffer is null || _buffer.Length < size)
                _buffer = new ulong[Math.Max(size, _buffer is null ? 64 : _buffer.Length * 2)];
            return _buffer;
        }

        private static void Append(List<ActorID> result, ulong[] buffer, int count)
        {
            for (int i = 0; i < count; ++i)
                result.Add(new ActorID(buffer[i]));
        }

        [LibraryImport("xgGameKernel", EntryPoint = "XG_ActorGetRoots")]
        private static partial int XG_ActorGetRoots(nint engine, ulong* output, int capacity);

        [LibraryImport("xgGameKernel", EntryPoint = "XG_ActorGetChildren")]
        private static partial int XG_ActorGetChildren(nint engine, ulong id, ulong* output, int capacity);

        [LibraryImport("xgGameKernel", EntryPoint = "XG_ActorGetParent")]
        private static partial ulong XG_ActorGetParent(nint engine, ulong id);

        [LibraryImport("xgGameKernel", EntryPoint = "XG_ActorAddComponent", StringMarshalling = StringMarshalling.Utf8)]
        private static partial int XG_ActorAddComponent(nint engine, ulong id, string typeKey, int runtimeKind);

        [LibraryImport("xgGameKernel", EntryPoint = "XG_ActorGetComponentHandles")]
        private static partial int XG_ActorGetComponentHandles(nint engine, ulong id, int runtimeKind, nint* output, int capacity);

        [LibraryImport("xgGameKernel", EntryPoint = "XG_ActorSetParentEx")]
        private static partial byte XG_ActorSetParentEx(nint engine, ulong child, ulong parent, byte keepWorld);
    }
}
