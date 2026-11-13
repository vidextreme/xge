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
        public static bool SetParent(ActorID child, ActorID? parent)
        {
            nint engine = ScriptRuntime._enginePtr;
            return engine != 0 && XG_ActorSetParent(engine, child, parent?.Value ?? 0) != 0;
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
            if (engine == 0 || !RuntimeFeature.IsDynamicCodeSupported)
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

        [LibraryImport("xgGameKernel", EntryPoint = "XG_ActorGetComponentHandles")]
        private static partial int XG_ActorGetComponentHandles(nint engine, ulong id, int runtimeKind, nint* output, int capacity);

        [LibraryImport("xgGameKernel", EntryPoint = "XG_ActorSetParent")]
        private static partial byte XG_ActorSetParent(nint engine, ulong child, ulong parent);
    }
}
