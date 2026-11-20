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

    // Mirror of the native xg::MeshRendererData (40 bytes) in the engine's "xg.MeshRenderer" pool.
    [StructLayout(LayoutKind.Sequential)]
    [ComponentDataKey("xg.MeshRenderer")]
    public struct MeshRendererData : IComponentData
    {
        [AssetType("mesh")]
        public AssetReference Model;
        [AssetType("material")]
        public AssetReference Material;
        public MeshRendererFlags Flags;
        [HideInInspector]
        public uint Reserved;
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

        private static partial class Native
        {
            [LibraryImport("xgScriptNative", EntryPoint = "XG_MeshRenderStats")]
            public static partial int Stats(nint engine, out int materialDraws);
        }
    }
}
