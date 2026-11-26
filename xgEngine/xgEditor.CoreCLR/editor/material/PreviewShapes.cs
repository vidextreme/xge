// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.

using xg.editor;

namespace xgEditor.CoreCLR
{
    public enum PreviewShape
    {
        Sphere,
        Cube,
        Plane,
    }

    // The material preview's built-in meshes (xg::shaders::SceneLitVertex, from PrimitiveMeshes), created on first use and
    // shared by every preview; each fits a unit sphere around the origin.
    internal static unsafe class PreviewShapes
    {
        private static readonly ulong[] _meshes = new ulong[3];

        public static ulong Get(PreviewShape shape)
        {
            int index = (int)shape;
            if (_meshes[index] != 0 || SceneViews.HasRenderer() == 0)
                return _meshes[index];
            MeshAssets.RawPart part = shape switch
            {
                PreviewShape.Cube => PrimitiveMeshes.Build(PrimitiveShape.Cube, 1.1f),
                PreviewShape.Plane => PrimitiveMeshes.Build(PrimitiveShape.Plane, 1.8f),
                _ => PrimitiveMeshes.Build(PrimitiveShape.Sphere, 1.8f),
            };
            byte[] vertices = MeshAssets.Interleave(part);
            ushort[] indices = part.Indices.Select(i => (ushort)i).ToArray();
            fixed (byte* v = vertices)
            fixed (ushort* i = indices)
            {
                SceneViews.MeshDesc desc = new()
                {
                    VertexCount = part.VertexCount,
                    Stride = MeshAssets.Stride,
                    Vertices = v,
                    IndexCount = indices.Length,
                    Format = 0,
                    Indices = i,
                };
                _meshes[index] = SceneViews.CreateMesh(&desc);
            }
            return _meshes[index];
        }
    }
}
