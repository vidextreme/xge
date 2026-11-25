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

    public enum ChunkFileStatus
    {
        Valid = 0,
        TooSmall = 1,
        BadMagic = 2,
        UnsupportedVersion = 3,
        Truncated = 4,
        BadItem = 5,
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

    // GPU texture formats (xg::TextureFormat; the editor's TextureHardwareFormat values).
    public enum TextureFormat : uint
    {
        RGBA8 = 1,
        BC1 = 2,
        BC3 = 3,
        BC4 = 4,
        BC5 = 5,
    }

    // Mirrors xg::TextureResourceInfo (xgCore/public/xgRenderer.h): the data of a Ready "texture" resource. TextureId is
    // also a UI texture id. Usage is the editor's TextureUsage (0 color, 1 normal, 2 luminance); Flags bit 0 = sRGB.
    [StructLayout(LayoutKind.Sequential)]
    public struct TextureInfo
    {
        public ulong TextureId;
        public int Width;
        public int Height;
        public int MipCount;
        public TextureFormat Format;
        public uint Usage;
        public uint Flags;
    }

    // Mirrors xg::ModelResourceInfo (xgCore/public/xgRenderer.h) without the parts pointer (read parts with
    // Resources.GetModelPart). Bounds are in model space.
    [StructLayout(LayoutKind.Sequential)]
    public unsafe struct ModelInfo
    {
        public int PartCount;
        public int NodeCount;
        public fixed float BoundsMin[3];
        public fixed float BoundsMax[3];
        private nint _parts;
    }

    // Mirrors xg::ModelPartInfo: a GPU mesh id and its model-space world matrix (row-major, row vectors).
    [StructLayout(LayoutKind.Sequential)]
    public unsafe struct ModelPartInfo
    {
        public ulong Mesh;
        public int MaterialIndex;
        public int TriangleCount;
        public fixed float World[16];
    }

    // The engine-wide resource cache. Main thread only. A resource can be Pending or Failed after Acquire, so check State.
    // Async types (like "texture") load on job workers: Acquire returns Pending and the resource becomes Ready or Failed
    // in a later frame (or call Wait).
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

        // Blocks until a Pending resource is Ready or Failed (runs jobs meanwhile).
        public static ResourceState Wait(ResourceHandle handle) =>
            ScriptRuntime._enginePtr == 0 ? ResourceState.Invalid : (ResourceState)XG_ResourceWait(ScriptRuntime._enginePtr, handle.Value);

        // Adds a folder searched for "<guid>.<extension>" (directly or one folder down), e.g. a project's Library/Data.
        // Returns the source id (0 when the folder is missing); sources go away with this module.
        public static uint AddDirectorySource(string root) =>
            ScriptRuntime._enginePtr == 0 ? 0 : XG_ResourceAddDirectorySource(ScriptRuntime._enginePtr, ScriptRuntime.ModuleID, root);

        public static bool RemoveSource(uint source) =>
            ScriptRuntime._enginePtr != 0 && XG_ResourceRemoveSource(ScriptRuntime._enginePtr, ScriptRuntime.ModuleID, source);

        // The GPU texture of a Ready "texture" resource (TextureType).
        public static bool GetTexture(ResourceHandle handle, out TextureInfo info)
        {
            info = default;
            return ScriptRuntime._enginePtr != 0 && XG_ResourceGetTexture(ScriptRuntime._enginePtr, handle.Value, out info);
        }

        // The engine's texture resource type (registered when a renderer exists): breadcrumb <asset guid>/<texture item guid>.
        public const string TextureType = "texture";

        // A file was rebuilt: its cached resources go stale, so holders (the mesh render system, materials) load the new
        // data. fileGuid is the asset's 32-digit GUID. Returns how many resources went stale.
        public static uint Invalidate(string fileGuid) =>
            ScriptRuntime._enginePtr == 0 ? 0 : XG_ResourceInvalidate(ScriptRuntime._enginePtr, fileGuid);

        // The renderer shader of a Ready "shader" resource (ShaderType), or 0.
        public static ulong GetShader(ResourceHandle handle) =>
            ScriptRuntime._enginePtr == 0 ? 0 : XG_ResourceGetShader(ScriptRuntime._enginePtr, handle.Value);

        // The renderer material of a Ready "material" resource (MaterialType), as the scene draws it; 0 until its shader
        // and textures are loaded.
        public static ulong ResolveMaterial(ResourceHandle handle) =>
            ScriptRuntime._enginePtr == 0 ? 0 : XG_ResourceResolveMaterial(ScriptRuntime._enginePtr, handle.Value);

        public static bool IsStale(ResourceHandle handle) =>
            ScriptRuntime._enginePtr == 0 || XG_ResourceIsStale(ScriptRuntime._enginePtr, handle.Value);

        // The engine's shader and material resource types (registered when a renderer exists), by asset GUID alone.
        public const string ShaderType = "shader";
        public const string MaterialType = "material";

        // The engine's model resource type (registered when a renderer exists): an imported mesh asset by its GUID alone.
        public const string ModelType = "model";

        // The summary of a Ready "model" resource.
        public static bool GetModel(ResourceHandle handle, out ModelInfo info)
        {
            info = default;
            return ScriptRuntime._enginePtr != 0 && XG_ResourceGetModel(ScriptRuntime._enginePtr, handle.Value, out info);
        }

        // The submesh item GUID of a model part (the GUID in the model's .xmd items).
        public static bool GetModelPartItem(ResourceHandle handle, int index, out ResourceGUID item)
        {
            item = default;
            return ScriptRuntime._enginePtr != 0 && XG_ResourceGetModelPartItem(ScriptRuntime._enginePtr, handle.Value, index, out item) != 0;
        }

        // Drawn part index of a Ready "model" resource: its GPU mesh and model-space placement.
        public static bool GetModelPart(ResourceHandle handle, int index, out ModelPartInfo part)
        {
            part = default;
            return ScriptRuntime._enginePtr != 0 && XG_ResourceGetModelPart(ScriptRuntime._enginePtr, handle.Value, index, out part);
        }

        // Test hook: registers a native in-memory type owned by this module.
        public static uint RegisterTestType(string name) =>
            ScriptRuntime._enginePtr == 0 ? 0 : XG_ResourceRegisterTestType(ScriptRuntime._enginePtr, ScriptRuntime.ModuleID, name);

        // Validates a generated .xr* or .xd* file with the native reader the runtime uses.
        public static ChunkFileStatus ValidateChunkFile(ReadOnlySpan<byte> data)
        {
            fixed (byte* bytes = data)
                return (ChunkFileStatus)XG_ChunkFileValidate(bytes, (ulong)data.Length);
        }

        // Finds a chunk inside an item, or at file level with a default item GUID.
        public static bool FindChunk(ReadOnlySpan<byte> data, in ResourceGUID item, uint type, out ulong offset, out ulong size)
        {
            fixed (byte* bytes = data)
                return XG_ChunkFileFind(bytes, (ulong)data.Length, item, type, out offset, out size);
        }

        internal static bool ParseID(string text, out ResourceID id) => XG_ResourceParseID(text, out id);

        internal static string FormatID(in ResourceID id)
        {
            byte* buffer = stackalloc byte[IDTextCapacity];
            int length = XG_ResourceFormatID(id, buffer, IDTextCapacity);
            return Encoding.ASCII.GetString(buffer, length);
        }

        [LibraryImport(NativeLib, EntryPoint = "XG_ChunkFileValidate")]
        private static partial int XG_ChunkFileValidate(byte* data, ulong size);

        [LibraryImport(NativeLib, EntryPoint = "XG_ChunkFileFind")]
        [return: MarshalAs(UnmanagedType.U1)]
        private static partial bool XG_ChunkFileFind(byte* data, ulong size, in ResourceGUID item, uint type, out ulong offset, out ulong chunkSize);

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

        [LibraryImport(NativeLib, EntryPoint = "XG_ResourceInvalidate", StringMarshalling = StringMarshalling.Utf8)]
        private static partial uint XG_ResourceInvalidate(nint engine, string fileGuid);

        [LibraryImport(NativeLib, EntryPoint = "XG_ResourceResolveMaterial")]
        private static partial ulong XG_ResourceResolveMaterial(nint engine, ulong handle);

        [LibraryImport(NativeLib, EntryPoint = "XG_ResourceGetShader")]
        private static partial ulong XG_ResourceGetShader(nint engine, ulong handle);

        [LibraryImport(NativeLib, EntryPoint = "XG_ResourceIsStale")]
        [return: MarshalAs(UnmanagedType.U1)]
        private static partial bool XG_ResourceIsStale(nint engine, ulong handle);

        [LibraryImport(NativeLib, EntryPoint = "XG_ResourceWait")]
        private static partial int XG_ResourceWait(nint engine, ulong handle);

        [LibraryImport(NativeLib, EntryPoint = "XG_ResourceAddDirectorySource", StringMarshalling = StringMarshalling.Utf8)]
        private static partial uint XG_ResourceAddDirectorySource(nint engine, uint moduleId, string root);

        [LibraryImport(NativeLib, EntryPoint = "XG_ResourceRemoveSource")]
        [return: MarshalAs(UnmanagedType.U1)]
        private static partial bool XG_ResourceRemoveSource(nint engine, uint moduleId, uint source);

        [LibraryImport(NativeLib, EntryPoint = "XG_ResourceGetModel")]
        [return: MarshalAs(UnmanagedType.U1)]
        private static partial bool XG_ResourceGetModel(nint engine, ulong handle, out ModelInfo info);

        [LibraryImport(NativeLib, EntryPoint = "XG_ResourceGetModelPartItem")]
        private static partial byte XG_ResourceGetModelPartItem(nint engine, ulong handle, int index, out ResourceGUID item);

        [LibraryImport(NativeLib, EntryPoint = "XG_ResourceGetModelPart")]
        [return: MarshalAs(UnmanagedType.U1)]
        private static partial bool XG_ResourceGetModelPart(nint engine, ulong handle, int index, out ModelPartInfo part);

        [LibraryImport(NativeLib, EntryPoint = "XG_ResourceGetTexture")]
        [return: MarshalAs(UnmanagedType.U1)]
        private static partial bool XG_ResourceGetTexture(nint engine, ulong handle, out TextureInfo info);

        [LibraryImport(NativeLib, EntryPoint = "XG_ResourceRegisterTestType", StringMarshalling = StringMarshalling.Utf8)]
        private static partial uint XG_ResourceRegisterTestType(nint engine, uint moduleId, string name);
    }
}
