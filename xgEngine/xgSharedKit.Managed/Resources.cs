// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.

using System.Text;

namespace xg
{
    // Mirrors xg::ResourceGUID (xgBase/public/xgResources.h).
    [StructLayout(LayoutKind.Sequential)]
    public readonly struct ResourceGUID : IEquatable<ResourceGUID>
    {
        public readonly ulong Hi;
        public readonly ulong Lo;

        public ResourceGUID(ulong hi, ulong lo)
        {
            Hi = hi;
            Lo = lo;
        }

        public bool IsZero => Hi == 0 && Lo == 0;

        public bool Equals(ResourceGUID other) => Hi == other.Hi && Lo == other.Lo;
        public override bool Equals(object? obj) => obj is ResourceGUID other && Equals(other);
        public override int GetHashCode() => HashCode.Combine(Hi, Lo);
        public override string ToString() => $"{Hi:x16}{Lo:x16}";
    }

    // Breadcrumb reference, mirroring xg::ResourceID: "<file-guid>/<chunk-guid>", or "<file-guid>" for the whole file.
    [StructLayout(LayoutKind.Sequential)]
    public readonly struct ResourceID : IEquatable<ResourceID>
    {
        public readonly ResourceGUID File;
        public readonly ResourceGUID Chunk;

        public ResourceID(ResourceGUID file, ResourceGUID chunk = default)
        {
            File = file;
            Chunk = chunk;
        }

        public static bool TryParse(string? text, out ResourceID id)
        {
            id = default;
            return text is not null && Resources.ParseID(text, out id);
        }

        public static ResourceID Parse(string text) =>
            TryParse(text, out ResourceID id) ? id : throw new FormatException($"Invalid resource ID '{text}'");

        public bool Equals(ResourceID other) => File.Equals(other.File) && Chunk.Equals(other.Chunk);
        public override bool Equals(object? obj) => obj is ResourceID other && Equals(other);
        public override int GetHashCode() => HashCode.Combine(File, Chunk);
        public override string ToString() => Resources.FormatID(this);
    }

    public enum ResourceState
    {
        Invalid = 0,
        Pending = 1,
        Ready = 2,
        Failed = 3,
    }

    // One reference held by this module. A copy is the same reference, so dispose it once.
    public readonly struct ResourceHandle : IDisposable, IEquatable<ResourceHandle>
    {
        public readonly ulong Value;

        internal ResourceHandle(ulong value) => Value = value;

        public bool IsValid => Value != 0;
        public ResourceState State => Resources.GetState(this);

        public void Dispose() => Resources.Release(this);

        public bool Equals(ResourceHandle other) => Value == other.Value;
        public override bool Equals(object? obj) => obj is ResourceHandle other && Equals(other);
        public override int GetHashCode() => Value.GetHashCode();
    }

    // The engine-wide resource cache. Main thread only. A resource can be Pending or Failed after Acquire, so check State.
    public static unsafe partial class Resources
    {
        private const string NativeLib = "xgScriptNative";
        private const int IDTextCapacity = 66;

        public static uint FindType(string name) =>
            ScriptRuntime._enginePtr == 0 ? 0 : XG_ResourceFindType(ScriptRuntime._enginePtr, name);

        public static ResourceHandle Acquire(uint type, in ResourceID id) =>
            new(ScriptRuntime._enginePtr == 0 ? 0 : XG_ResourceAcquire(ScriptRuntime._enginePtr, ScriptRuntime.ModuleID, type, id));

        public static bool AddRef(ResourceHandle handle) =>
            ScriptRuntime._enginePtr != 0 && XG_ResourceAddRef(ScriptRuntime._enginePtr, ScriptRuntime.ModuleID, handle.Value);

        public static bool Release(ResourceHandle handle) =>
            ScriptRuntime._enginePtr != 0 && XG_ResourceRelease(ScriptRuntime._enginePtr, ScriptRuntime.ModuleID, handle.Value);

        public static ResourceState GetState(ResourceHandle handle) =>
            ScriptRuntime._enginePtr == 0 ? ResourceState.Invalid : (ResourceState)XG_ResourceGetState(ScriptRuntime._enginePtr, handle.Value);

        // Test hook: registers a native in-memory type owned by this module.
        public static uint RegisterTestType(string name) =>
            ScriptRuntime._enginePtr == 0 ? 0 : XG_ResourceRegisterTestType(ScriptRuntime._enginePtr, ScriptRuntime.ModuleID, name);

        internal static bool ParseID(string text, out ResourceID id) => XG_ResourceParseID(text, out id);

        internal static string FormatID(in ResourceID id)
        {
            byte* buffer = stackalloc byte[IDTextCapacity];
            int length = XG_ResourceFormatID(id, buffer, IDTextCapacity);
            return Encoding.ASCII.GetString(buffer, length);
        }

        [LibraryImport(NativeLib, EntryPoint = "XG_ResourceParseID", StringMarshalling = StringMarshalling.Utf8)]
        [return: MarshalAs(UnmanagedType.U1)]
        private static partial bool XG_ResourceParseID(string text, out ResourceID id);

        [LibraryImport(NativeLib, EntryPoint = "XG_ResourceFormatID")]
        private static partial int XG_ResourceFormatID(in ResourceID id, byte* buffer, int capacity);

        [LibraryImport(NativeLib, EntryPoint = "XG_ResourceFindType", StringMarshalling = StringMarshalling.Utf8)]
        private static partial uint XG_ResourceFindType(nint engine, string name);

        [LibraryImport(NativeLib, EntryPoint = "XG_ResourceAcquire")]
        private static partial ulong XG_ResourceAcquire(nint engine, uint moduleId, uint type, in ResourceID id);

        [LibraryImport(NativeLib, EntryPoint = "XG_ResourceAddRef")]
        [return: MarshalAs(UnmanagedType.U1)]
        private static partial bool XG_ResourceAddRef(nint engine, uint moduleId, ulong handle);

        [LibraryImport(NativeLib, EntryPoint = "XG_ResourceRelease")]
        [return: MarshalAs(UnmanagedType.U1)]
        private static partial bool XG_ResourceRelease(nint engine, uint moduleId, ulong handle);

        [LibraryImport(NativeLib, EntryPoint = "XG_ResourceGetState")]
        private static partial int XG_ResourceGetState(nint engine, ulong handle);

        [LibraryImport(NativeLib, EntryPoint = "XG_ResourceRegisterTestType", StringMarshalling = StringMarshalling.Utf8)]
        private static partial uint XG_ResourceRegisterTestType(nint engine, uint moduleId, string name);
    }
}
