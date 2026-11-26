// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.

namespace xg
{
    // A reference to an asset by its GUID (the asset's .xmd GUID; zero = none). Blittable, so it can live in component
    // data and survives hot reload and Play/Stop. The editor draws it as an asset picker.
    [StructLayout(LayoutKind.Sequential)]
    public struct AssetReference : IEquatable<AssetReference>
    {
        public ResourceGUID Guid;

        public AssetReference(ResourceGUID guid) => Guid = guid;

        public bool IsNone => Guid.IsZero;

        // From 32 hex digits (an .xmd GUID); empty or invalid text gives None.
        public static AssetReference FromText(string? text) =>
            ResourceID.TryParse(text, out ResourceID id) ? new AssetReference(id.File) : default;

        public bool Equals(AssetReference other) => Guid.Equals(other.Guid);
        public override bool Equals(object? obj) => obj is AssetReference other && Equals(other);
        public override int GetHashCode() => Guid.GetHashCode();
        public override string ToString() => IsNone ? string.Empty : Guid.ToString();
    }

    [Flags]
    public enum MeshRendererFlags : uint
    {
        None = 0,
        Hidden = 1u << 0,
    }

    [Flags]
    public enum MeshRendererSubmeshFlags : uint
    {
        None = 0,
        // The submesh isn't drawn.
        Hidden = 1u << 0,
    }

    // Mirror of the native xg::MeshRendererSubmesh (48 bytes): one submesh's material (None = the default material) and
    // visibility, keyed by the submesh item GUID of the model's meta.
    [StructLayout(LayoutKind.Sequential)]
    public struct MeshRendererSubmesh
    {
        public ResourceGUID Submesh;
        public AssetReference Material;
        public MeshRendererSubmeshFlags Flags;
        public uint Reserved0;
        public uint Reserved1;
        public uint Reserved2;

        public readonly bool Hidden => (Flags & MeshRendererSubmeshFlags.Hidden) != 0;
    }

    // Mirror of the native xg::MeshRendererData (424 bytes) in the engine's "xg.MeshRenderer" pool. Submesh0..7
    // (the first SubmeshCount are used) set a material per submesh or hide it; other submeshes draw with Material.
    [StructLayout(LayoutKind.Sequential)]
    [ComponentDataKey("xg.MeshRenderer")]
    public struct MeshRendererData : IComponentData
    {
        public const int MaxSubmeshes = 8;

        [AssetType("mesh")]
        public AssetReference Model;
        [AssetType("material")]
        public AssetReference Material;
        public MeshRendererFlags Flags;
        public uint SubmeshCount;
        public MeshRendererSubmesh Submesh0;
        public MeshRendererSubmesh Submesh1;
        public MeshRendererSubmesh Submesh2;
        public MeshRendererSubmesh Submesh3;
        public MeshRendererSubmesh Submesh4;
        public MeshRendererSubmesh Submesh5;
        public MeshRendererSubmesh Submesh6;
        public MeshRendererSubmesh Submesh7;

        public MeshRendererSubmesh GetSubmesh(int index) => Entry(ref this, index);

        public void SetSubmesh(int index, MeshRendererSubmesh value) => Entry(ref this, index) = value;

        // The entry index of a submesh item GUID among the first SubmeshCount, or -1.
        public readonly int FindSubmesh(ResourceGUID submesh)
        {
            MeshRendererData copy = this;
            for (int i = 0; i < Math.Min((int)SubmeshCount, MaxSubmeshes); i++)
                if (Entry(ref copy, i).Submesh.Equals(submesh))
                    return i;
            return -1;
        }

        // The submesh's entry, adding one (drawn, with Material) when it has none; -1 when all entries are used.
        public int EnsureSubmesh(ResourceGUID submesh)
        {
            int index = FindSubmesh(submesh);
            if (index >= 0 || SubmeshCount >= MaxSubmeshes)
                return index;
            index = (int)SubmeshCount++;
            Entry(ref this, index) = new MeshRendererSubmesh { Submesh = submesh, Material = Material };
            return index;
        }

        public void ClearSubmeshes()
        {
            for (int i = 0; i < MaxSubmeshes; i++)
                Entry(ref this, i) = default;
            SubmeshCount = 0;
        }

        private static ref MeshRendererSubmesh Entry(ref MeshRendererData data, int index)
        {
            if ((uint)index >= MaxSubmeshes)
                throw new ArgumentOutOfRangeException(nameof(index));
            return ref System.Runtime.CompilerServices.Unsafe.Add(ref data.Submesh0, index);
        }
    }

    // Draws an imported model with a material in every render view (edit mode too). The engine's mesh render system
    // reads the data, so a MeshRenderer works the same from any module.
    [ComponentIcon("Mesh", Color = "#4FA3D9")]
    [RequireComponent(typeof(Transform))]
    public sealed partial class MeshRenderer : ComponentDataSet<MeshRendererData>
    {
        public AssetReference Model
        {
            get => Data1.Model;
            set => Data1.Model = value;
        }

        public AssetReference Material
        {
            get => Data1.Material;
            set => Data1.Material = value;
        }

        // A submesh's material (None = the default material) and visibility; submeshes without an entry use Material.
        public AssetReference SubmeshMaterial(ResourceGUID submesh)
        {
            int index = Data1.FindSubmesh(submesh);
            return index >= 0 ? Data1.GetSubmesh(index).Material : Data1.Material;
        }

        public bool SetSubmeshMaterial(ResourceGUID submesh, AssetReference material)
        {
            int index = Data1.EnsureSubmesh(submesh);
            if (index < 0)
                return false;
            MeshRendererSubmesh entry = Data1.GetSubmesh(index);
            entry.Material = material;
            Data1.SetSubmesh(index, entry);
            return true;
        }

        public bool SetSubmeshVisible(ResourceGUID submesh, bool visible)
        {
            int index = Data1.EnsureSubmesh(submesh);
            if (index < 0)
                return false;
            MeshRendererSubmesh entry = Data1.GetSubmesh(index);
            entry.Flags = visible ? entry.Flags & ~MeshRendererSubmeshFlags.Hidden : entry.Flags | MeshRendererSubmeshFlags.Hidden;
            Data1.SetSubmesh(index, entry);
            return true;
        }

        public bool Hidden
        {
            get => (Data1.Flags & MeshRendererFlags.Hidden) != 0;
            set => Data1.Flags = value ? Data1.Flags | MeshRendererFlags.Hidden : Data1.Flags & ~MeshRendererFlags.Hidden;
        }

        // Draws handed to the renderer last frame by the mesh render system, and how many had their material resolved.
        public static int DrawCount(out int materialDraws)
        {
            materialDraws = 0;
            return ScriptRuntime._enginePtr == 0 ? 0 : Native.Stats(ScriptRuntime._enginePtr, out materialDraws);
        }

        // Replaces the actors hidden in the editor: their mesh and object draws are skipped by the Scene view only.
        public static unsafe void SetEditorHidden(ReadOnlySpan<ulong> actors)
        {
            if (ScriptRuntime._enginePtr == 0)
                return;
            fixed (ulong* p = actors)
                Native.SetEditorHidden(ScriptRuntime._enginePtr, p, (uint)actors.Length);
        }

        // Editor preview of an unsaved material: the actor's parts drawing this material draw rendererMaterial instead
        // (0 removes the override).
        public static unsafe void SetEditorMaterialOverride(ActorID actor, ResourceGUID material, ulong rendererMaterial)
        {
            if (ScriptRuntime._enginePtr != 0)
                Native.SetEditorOverride(ScriptRuntime._enginePtr, actor, &material, rendererMaterial);
        }

        private static partial class Native
        {
            [LibraryImport("xgScriptNative", EntryPoint = "XG_MeshRenderSetEditorOverride")]
            public static unsafe partial void SetEditorOverride(nint engine, ulong actor, ResourceGUID* material, ulong rendererMaterial);

            [LibraryImport("xgScriptNative", EntryPoint = "XG_MeshRenderStats")]
            public static partial int Stats(nint engine, out int materialDraws);

            [LibraryImport("xgScriptNative", EntryPoint = "XG_MeshRenderSetEditorHidden")]
            public static unsafe partial void SetEditorHidden(nint engine, ulong* actors, uint count);
        }
    }
}
