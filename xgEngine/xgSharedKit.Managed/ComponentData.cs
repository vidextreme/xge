// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.

using System.Reflection;
using System.Runtime.CompilerServices;
using System.Text;

namespace xg
{
    // Binds a data struct to an existing native data type instead of registering its own (e.g. "xg.Transform").
    [AttributeUsage(AttributeTargets.Struct)]
    public sealed class ComponentDataKeyAttribute : Attribute
    {
        public string Key { get; }
        public ComponentDataKeyAttribute(string key) => Key = key;
    }

    [Flags]
    public enum ComponentDataFlags : uint
    {
        None = 0,
        EngineOwned = 1u << 0,
        Shared = 1u << 1,
    }

    // The native pool of a data struct: registered on first use in each module (load context). Script types are shared,
    // so they survive hot reload; a changed layout is migrated field by field by the registry.
    public static unsafe class ComponentDataType<T> where T : unmanaged, IComponentData
    {
        private static uint _id;
        private static bool _resolved;
        private static ComponentDataFlags _flags;

        public static uint Id => _resolved ? _id : Resolve();
        public static bool IsEngineOwned => Id != 0 && (_flags & ComponentDataFlags.EngineOwned) != 0;
        public static string Key => typeof(T).GetCustomAttribute<ComponentDataKeyAttribute>()?.Key ?? ComponentData.KeyOf(typeof(T));

        private static uint Resolve()
        {
            nint engine = ScriptRuntime._enginePtr;
            if (engine == 0)
                return 0;
            string? fixedKey = typeof(T).GetCustomAttribute<ComponentDataKeyAttribute>()?.Key;
            uint id;
            if (fixedKey is not null)
            {
                id = ComponentData.Native.FindType(engine, fixedKey);
                uint size = 0;
                if (id != 0 && (!ComponentData.Native.GetTypeInfo(engine, id, out _, out size, out _, out uint flags) || size != (uint)sizeof(T)))
                {
                    Debug.LogError($"[ComponentData] {typeof(T).Name} is {sizeof(T)} bytes but '{fixedKey}' is {size}.");
                    id = 0;
                }
                else if (id != 0)
                {
                    ComponentData.Native.GetTypeInfo(engine, id, out _, out _, out _, out uint f);
                    _flags = (ComponentDataFlags)f;
                }
            }
            else
            {
                ComponentDataLayout layout = ComponentDataLayout.Describe(typeof(T), sizeof(T));
                id = ComponentData.Native.RegisterType(engine, ScriptRuntime.ModuleID, ComponentData.KeyOf(typeof(T)), (uint)sizeof(T),
                    layout.Align, layout.Hash, (uint)ComponentDataFlags.Shared, layout.Text);
                _flags = ComponentDataFlags.Shared;
                if (id == 0)
                    Debug.LogError($"[ComponentData] Could not register {typeof(T).FullName}.");
            }
            _id = id;
            _resolved = id != 0;
            return id;
        }

        internal static void Reset()
        {
            _id = 0;
            _resolved = false;
        }
    }

    // Native component data owned by the engine, one element per actor and data type. Elements are reference-counted:
    // ComponentDataSet components add one reference each; Add/Release here are the raw form.
    public static unsafe partial class ComponentData
    {
        private const string NativeLib = "xgScriptNative";

        public static string KeyOf(Type type) => $"{type.FullName}, {type.Assembly.GetName().Name}";

        public static bool Has<T>(ActorID actor) where T : unmanaged, IComponentData => Pointer<T>(actor) != null;

        // A reference to the actor's element; valid until the next add or remove of that type. Throws when there is none.
        public static ref T Get<T>(ActorID actor) where T : unmanaged, IComponentData
        {
            T* p = Pointer<T>(actor);
            if (p == null)
                throw new InvalidOperationException($"Actor {(ulong)actor} has no {typeof(T).Name}.");
            return ref *p;
        }

        public static bool TryGet<T>(ActorID actor, out T value) where T : unmanaged, IComponentData
        {
            T* p = Pointer<T>(actor);
            value = p != null ? *p : default;
            return p != null;
        }

        // Adds one reference (creating a zero-filled element) and returns the element.
        public static ref T Add<T>(ActorID actor) where T : unmanaged, IComponentData
        {
            ScriptThread.CheckMain("ComponentData.Add");
            if (ComponentDataType<T>.IsEngineOwned)
                throw new InvalidOperationException($"{typeof(T).Name} is owned by the engine; use its own API.");
            T* p = (T*)Native.Add(ScriptRuntime._enginePtr, ComponentDataType<T>.Id, actor);
            if (p == null)
                throw new InvalidOperationException($"Cannot add {typeof(T).Name} to actor {(ulong)actor}.");
            return ref *p;
        }

        // Drops one reference; returns true when the element was removed.
        public static bool Release<T>(ActorID actor) where T : unmanaged, IComponentData
        {
            ScriptThread.CheckMain("ComponentData.Release");
            return Native.Release(ScriptRuntime._enginePtr, ComponentDataType<T>.Id, actor);
        }

        // Removes the element whatever its reference count.
        public static bool Remove<T>(ActorID actor) where T : unmanaged, IComponentData
        {
            ScriptThread.CheckMain("ComponentData.Remove");
            return Native.Remove(ScriptRuntime._enginePtr, ComponentDataType<T>.Id, actor);
        }

        // Every element of the type and its owner, slot order. Valid until the next add or remove of that type.
        public static Span<T> All<T>(out ReadOnlySpan<ActorID> owners) where T : unmanaged, IComponentData
        {
            ActorID* o = null;
            uint count = 0;
            void* dense = Native.GetDense(ScriptRuntime._enginePtr, ComponentDataType<T>.Id, (ulong**)&o, &count);
            owners = new ReadOnlySpan<ActorID>(o, (int)count);
            return new Span<T>(dense, (int)count);
        }

        internal static T* Pointer<T>(ActorID actor) where T : unmanaged, IComponentData
        {
            nint engine = ScriptRuntime._enginePtr;
            uint id = engine == 0 ? 0 : ComponentDataType<T>.Id;
            return id == 0 ? null : (T*)Native.Get(engine, id, actor);
        }

        // Element bytes of every script (non-engine) data type; Restore writes them back, removes elements created since
        // and leaves elements whose actor is gone (play mode).
        public sealed class Snapshot
        {
            private readonly List<(uint Type, ulong[] Owners, byte[] Bytes, uint Stride)> _types = new();

            public int Count => _types.Count;

            public static Snapshot Capture()
            {
                Snapshot snapshot = new();
                nint engine = ScriptRuntime._enginePtr;
                if (engine == 0)
                    return snapshot;
                foreach (uint type in Types())
                {
                    if (!Native.GetTypeInfo(engine, type, out _, out uint size, out uint align, out uint flags) || (flags & (uint)ComponentDataFlags.EngineOwned) != 0)
                        continue;
                    ulong* owners = null;
                    uint count = 0;
                    byte* dense = (byte*)Native.GetDense(engine, type, &owners, &count);
                    uint stride = Stride(size, align);
                    ulong[] ids = new ReadOnlySpan<ulong>(owners, (int)count).ToArray();
                    byte[] bytes = new ReadOnlySpan<byte>(dense, (int)(count * stride)).ToArray();
                    snapshot._types.Add((type, ids, bytes, stride));
                }
                return snapshot;
            }

            public (int Restored, int Removed) Restore()
            {
                nint engine = ScriptRuntime._enginePtr;
                int restored = 0;
                int removed = 0;
                if (engine == 0)
                    return (0, 0);
                foreach ((uint type, ulong[] owners, byte[] bytes, uint stride) in _types)
                {
                    HashSet<ulong> captured = new(owners);
                    for (int i = 0; i < owners.Length; i++)
                    {
                        byte* element = (byte*)Native.Get(engine, type, owners[i]);
                        if (element == null)
                            continue;
                        bytes.AsSpan(i * (int)stride, (int)stride).CopyTo(new Span<byte>(element, (int)stride));
                        restored++;
                    }
                    ulong* current = null;
                    uint count = 0;
                    Native.GetDense(engine, type, &current, &count);
                    List<ulong> extra = new();
                    for (uint i = 0; i < count; i++)
                        if (!captured.Contains(current[i]))
                            extra.Add(current[i]);
                    foreach (ulong id in extra)
                        if (Native.Remove(engine, type, id))
                            removed++;
                    Native.MarkAllChanged(engine, type);
                }
                return (restored, removed);
            }
        }

        internal static uint Stride(uint size, uint align) => align == 0 ? size : (size + align - 1) / align * align;

        internal static List<uint> Types()
        {
            List<uint> result = new();
            nint engine = ScriptRuntime._enginePtr;
            if (engine == 0)
                return result;
            int count = Native.GetTypes(engine, null, 0);
            uint[] ids = new uint[count];
            fixed (uint* p = ids)
                count = Math.Min(Native.GetTypes(engine, p, count), count);
            for (int i = 0; i < count; i++)
                result.Add(ids[i]);
            return result;
        }

        internal static partial class Native
        {
            [LibraryImport(NativeLib, EntryPoint = "XG_ComponentDataRegisterType", StringMarshalling = StringMarshalling.Utf8)]
            public static partial uint RegisterType(nint engine, uint moduleId, string key, uint size, uint align, ulong layoutHash, uint flags, string layout);

            [LibraryImport(NativeLib, EntryPoint = "XG_ComponentDataFindType", StringMarshalling = StringMarshalling.Utf8)]
            public static partial uint FindType(nint engine, string key);

            [LibraryImport(NativeLib, EntryPoint = "XG_ComponentDataGetTypeInfo")]
            private static partial byte GetTypeInfo(nint engine, uint type, nint* key, uint* size, uint* align, uint* flags);

            public static bool GetTypeInfo(nint engine, uint type, out string key, out uint size, out uint align, out uint flags)
            {
                nint k = 0;
                uint s = 0, a = 0, f = 0;
                bool ok = GetTypeInfo(engine, type, &k, &s, &a, &f) != 0;
                key = Marshal.PtrToStringUTF8(k) ?? string.Empty;
                size = s;
                align = a;
                flags = f;
                return ok;
            }

            [LibraryImport(NativeLib, EntryPoint = "XG_ComponentDataGetTypes")]
            public static partial int GetTypes(nint engine, uint* output, int capacity);

            [LibraryImport(NativeLib, EntryPoint = "XG_ComponentDataAdd")]
            public static partial void* Add(nint engine, uint type, ulong actor);

            [LibraryImport(NativeLib, EntryPoint = "XG_ComponentDataRelease")]
            [return: MarshalAs(UnmanagedType.U1)]
            public static partial bool Release(nint engine, uint type, ulong actor);

            [LibraryImport(NativeLib, EntryPoint = "XG_ComponentDataRemove")]
            [return: MarshalAs(UnmanagedType.U1)]
            public static partial bool Remove(nint engine, uint type, ulong actor);

            [LibraryImport(NativeLib, EntryPoint = "XG_ComponentDataGet")]
            [SuppressGCTransition]
            public static partial void* Get(nint engine, uint type, ulong actor);

            [LibraryImport(NativeLib, EntryPoint = "XG_ComponentDataGetDense")]
            public static partial void* GetDense(nint engine, uint type, ulong** owners, uint* count);

            [LibraryImport(NativeLib, EntryPoint = "XG_ComponentDataMarkAllChanged")]
            public static partial void MarkAllChanged(nint engine, uint type);

            [LibraryImport(NativeLib, EntryPoint = "XG_ComponentDataJoin")]
            public static partial int Join(nint engine, uint type, uint other, int* slots, int capacity);

            [LibraryImport(NativeLib, EntryPoint = "XG_ComponentDataClaimUpdater")]
            [return: MarshalAs(UnmanagedType.U1)]
            public static partial bool ClaimUpdater(nint engine, uint type, uint moduleId);

            [LibraryImport(NativeLib, EntryPoint = "XG_ComponentDataReleaseUpdater")]
            public static partial void ReleaseUpdater(nint engine, uint type, uint moduleId);
        }
    }

    // A data struct's field layout for the registry ("name:type:offset:size;...", nested structs flattened with dots)
    // and its hash. Offsets follow the runtime's sequential layout (Pack respected) or [FieldOffset] for explicit
    // layouts; when the computed size disagrees with the real one, only the size is hashed and migration is off.
    internal readonly struct ComponentDataLayout
    {
        public string Text { get; init; }
        public ulong Hash { get; init; }
        public uint Align { get; init; }

        public static ComponentDataLayout Describe(Type type, int realSize)
        {
            StringBuilder text = new();
            (int size, int align) = Walk(type, "", 0, text);
            string layout = size == realSize ? text.ToString() : string.Empty;
            if (layout.Length == 0)
                Debug.LogWarning($"[ComponentData] Could not describe the layout of {type.FullName}; layout changes will not be migrated.");
            ulong hash = 14695981039346656037ul;
            foreach (char c in $"{layout}|{realSize}")
            {
                hash ^= c;
                hash *= 1099511628211ul;
            }
            return new ComponentDataLayout { Text = layout, Hash = hash, Align = (uint)Math.Clamp(align, 1, 64) };
        }

        private static (int Size, int Align) Walk(Type type, string prefix, int baseOffset, StringBuilder text)
        {
            if (Leaf(type) is (int leafSize, string name))
            {
                if (text.Length > 0)
                    text.Append(';');
                text.Append(prefix.TrimEnd('.')).Append(':').Append(name).Append(':').Append(baseOffset).Append(':').Append(leafSize);
                return (leafSize, leafSize);
            }
            StructLayoutAttribute? layout = type.StructLayoutAttribute;
            bool isExplicit = layout?.Value == LayoutKind.Explicit;
            int pack = layout is { Pack: > 0 } ? layout.Pack : 0;
            int offset = 0;
            int maxAlign = 1;
            int end = 0;
            foreach (FieldInfo field in type.GetFields(BindingFlags.Instance | BindingFlags.Public | BindingFlags.NonPublic).OrderBy(f => f.MetadataToken))
            {
                StringBuilder probe = new();
                (int fieldSize, int fieldAlign) = Walk(field.FieldType, "", 0, probe);
                int align = pack > 0 ? Math.Min(fieldAlign, pack) : fieldAlign;
                int fieldOffset = isExplicit ? field.GetCustomAttribute<FieldOffsetAttribute>()?.Value ?? 0 : (offset + align - 1) / align * align;
                Walk(field.FieldType, $"{prefix}{field.Name}.", baseOffset + fieldOffset, text);
                offset = fieldOffset + fieldSize;
                end = Math.Max(end, offset);
                maxAlign = Math.Max(maxAlign, align);
            }
            int size = (end + maxAlign - 1) / maxAlign * maxAlign;
            if (layout is { Size: > 0 } && layout.Size > size)
                size = layout.Size;
            return (Math.Max(size, 1), maxAlign);
        }

        private static (int, string)? Leaf(Type type)
        {
            if (type.IsEnum)
                type = Enum.GetUnderlyingType(type);
            if (type == typeof(bool) || type == typeof(byte) || type == typeof(sbyte)) return (1, type.Name);
            if (type == typeof(char) || type == typeof(short) || type == typeof(ushort) || type == typeof(Half)) return (2, type.Name);
            if (type == typeof(int) || type == typeof(uint) || type == typeof(float)) return (4, type.Name);
            if (type == typeof(long) || type == typeof(ulong) || type == typeof(double) || type == typeof(nint) || type == typeof(nuint)) return (8, type.Name);
            if (type.IsPointer) return (8, "ptr");
            return null;
        }
    }

    // Main-thread checks for APIs that change actors, components or native data (data updates run on job workers).
    public static class ScriptThread
    {
        private static int _mainThread;
        private static volatile bool _inDataPass;

        // True while data updates run (structural changes would move the elements they are writing).
        public static bool InDataPass
        {
            get => _inDataPass;
            internal set => _inDataPass = value;
        }

        public static bool IsMain => _mainThread == 0 || Environment.CurrentManagedThreadId == _mainThread;

        internal static void SetMain() => _mainThread = Environment.CurrentManagedThreadId;

        public static void CheckMain(string api)
        {
            if (!IsMain || _inDataPass)
                throw new InvalidOperationException($"{api} is main-thread only; record it in the update's CommandBuffer instead.");
        }
    }
}
