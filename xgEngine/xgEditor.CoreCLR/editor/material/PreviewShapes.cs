// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.

using System.Runtime.InteropServices;

namespace xgEditor.CoreCLR
{
    public enum PreviewShape
    {
        Sphere,
        Cube,
        Plane,
    }

    // The material preview's built-in meshes (xg::shaders::SceneLitVertex), created on first use and shared by every
    // preview; each fits a unit sphere around the origin.
    internal static unsafe class PreviewShapes
    {
        [StructLayout(LayoutKind.Sequential, Pack = 1)]
        private struct Vertex
        {
            public float X, Y, Z;
            public float NX, NY, NZ;
            public float TX, TY, TZ, TW;
            public float U, V;
            public uint Color;
        }

        private static readonly ulong[] _meshes = new ulong[3];

        public static ulong Get(PreviewShape shape)
        {
            int index = (int)shape;
            if (_meshes[index] != 0 || SceneViews.HasRenderer() == 0)
                return _meshes[index];
            (List<Vertex> vertices, List<ushort> indices) = shape switch
            {
                PreviewShape.Cube => Cube(),
                PreviewShape.Plane => Plane(),
                _ => Sphere(48, 24),
            };
            Vertex[] vertexArray = vertices.ToArray();
            ushort[] indexArray = indices.ToArray();
            fixed (Vertex* v = vertexArray)
            fixed (ushort* i = indexArray)
            {
                SceneViews.MeshDesc desc = new()
                {
                    VertexCount = vertexArray.Length,
                    Stride = sizeof(Vertex),
                    Vertices = v,
                    IndexCount = indexArray.Length,
                    Format = 0,
                    Indices = i,
                };
                _meshes[index] = SceneViews.CreateMesh(&desc);
            }
            return _meshes[index];
        }

        private static Vertex Make(float x, float y, float z, float nx, float ny, float nz, float tx, float ty, float tz, float u, float v) =>
            new() { X = x, Y = y, Z = z, NX = nx, NY = ny, NZ = nz, TX = tx, TY = ty, TZ = tz, TW = 1.0f, U = u, V = v, Color = 0xFFFFFFFF };

        private static (List<Vertex>, List<ushort>) Sphere(int segments, int rings)
        {
            List<Vertex> vertices = new();
            List<ushort> indices = new();
            for (int r = 0; r <= rings; r++)
            {
                float v = (float)r / rings;
                float phi = v * MathF.PI;
                for (int s = 0; s <= segments; s++)
                {
                    float u = (float)s / segments;
                    float theta = u * MathF.Tau;
                    float x = MathF.Sin(phi) * MathF.Cos(theta);
                    float y = MathF.Cos(phi);
                    float z = MathF.Sin(phi) * MathF.Sin(theta);
                    vertices.Add(Make(x * 0.9f, y * 0.9f, z * 0.9f, x, y, z, -MathF.Sin(theta), 0.0f, MathF.Cos(theta), u * 2.0f, v));
                }
            }
            int stride = segments + 1;
            for (int r = 0; r < rings; r++)
            {
                for (int s = 0; s < segments; s++)
                {
                    ushort a = (ushort)(r * stride + s);
                    ushort b = (ushort)(a + stride);
                    indices.AddRange(new[] { a, (ushort)(a + 1), b, b, (ushort)(a + 1), (ushort)(b + 1) });
                }
            }
            return (vertices, indices);
        }

        private static (List<Vertex>, List<ushort>) Cube()
        {
            List<Vertex> vertices = new();
            List<ushort> indices = new();
            (float nx, float ny, float nz, float ux, float uy, float uz, float vx, float vy, float vz)[] faces =
            {
                (1, 0, 0, 0, 0, -1, 0, -1, 0), (-1, 0, 0, 0, 0, 1, 0, -1, 0),
                (0, 1, 0, 1, 0, 0, 0, 0, 1), (0, -1, 0, 1, 0, 0, 0, 0, -1),
                (0, 0, 1, 1, 0, 0, 0, -1, 0), (0, 0, -1, -1, 0, 0, 0, -1, 0),
            };
            const float h = 0.55f;
            foreach (var f in faces)
            {
                ushort start = (ushort)vertices.Count;
                for (int corner = 0; corner < 4; corner++)
                {
                    float su = corner is 1 or 2 ? 1.0f : -1.0f;
                    float sv = corner is 2 or 3 ? 1.0f : -1.0f;
                    vertices.Add(Make(
                        (f.nx + f.ux * su + f.vx * sv) * h, (f.ny + f.uy * su + f.vy * sv) * h, (f.nz + f.uz * su + f.vz * sv) * h,
                        f.nx, f.ny, f.nz, f.ux, f.uy, f.uz, (su + 1.0f) * 0.5f, (sv + 1.0f) * 0.5f));
                }
                indices.AddRange(new[] { start, (ushort)(start + 1), (ushort)(start + 2), start, (ushort)(start + 2), (ushort)(start + 3) });
            }
            return (vertices, indices);
        }

        private static (List<Vertex>, List<ushort>) Plane()
        {
            const float h = 0.9f;
            List<Vertex> vertices = new()
            {
                Make(-h, 0.0f, -h, 0, 1, 0, 1, 0, 0, 0, 1),
                Make(h, 0.0f, -h, 0, 1, 0, 1, 0, 0, 1, 1),
                Make(h, 0.0f, h, 0, 1, 0, 1, 0, 0, 1, 0),
                Make(-h, 0.0f, h, 0, 1, 0, 1, 0, 0, 0, 0),
            };
            return (vertices, new List<ushort> { 0, 2, 1, 0, 3, 2 });
        }
    }
}
