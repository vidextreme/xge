// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.

using System.Text.Json;
using System.Text.Json.Nodes;

namespace xg.editor
{
    public enum PrimitiveShape
    {
        Cube,
        Sphere,
        Plane,
        Quad,
    }

    // Procedural primitive meshes as raw mesh parts (positions, normals, tangents, uv), shared by the primitive importer
    // and the editor's live preview meshes. Size is the cube's edge, the sphere's diameter, the plane's and quad's side.
    // The plane faces +Y, the quad -Z (towards a camera looking down +Z); front faces wind clockwise.
    public static class PrimitiveMeshes
    {
        public static MeshAssets.RawPart Build(PrimitiveShape shape, float size, int segments = 48, int rings = 24)
        {
            Builder builder = new();
            float half = size * 0.5f;
            switch (shape)
            {
                case PrimitiveShape.Sphere:
                    Sphere(builder, half, Math.Max(segments, 3), Math.Max(rings, 2));
                    break;
                case PrimitiveShape.Plane:
                    builder.Vertex(-half, 0.0f, -half, 0, 1, 0, 1, 0, 0, 0, 1);
                    builder.Vertex(half, 0.0f, -half, 0, 1, 0, 1, 0, 0, 1, 1);
                    builder.Vertex(half, 0.0f, half, 0, 1, 0, 1, 0, 0, 1, 0);
                    builder.Vertex(-half, 0.0f, half, 0, 1, 0, 1, 0, 0, 0, 0);
                    builder.Triangle(0, 2, 1);
                    builder.Triangle(0, 3, 2);
                    break;
                case PrimitiveShape.Quad:
                    builder.Vertex(-half, -half, 0.0f, 0, 0, -1, 1, 0, 0, 0, 1);
                    builder.Vertex(half, -half, 0.0f, 0, 0, -1, 1, 0, 0, 1, 1);
                    builder.Vertex(half, half, 0.0f, 0, 0, -1, 1, 0, 0, 1, 0);
                    builder.Vertex(-half, half, 0.0f, 0, 0, -1, 1, 0, 0, 0, 0);
                    builder.Triangle(0, 3, 2);
                    builder.Triangle(0, 2, 1);
                    break;
                default:
                    Cube(builder, half);
                    break;
            }
            return builder.ToPart(shape.ToString().ToLowerInvariant());
        }

        public static bool TryParse(string? text, out PrimitiveShape shape) => Enum.TryParse(text, true, out shape);

        private static void Sphere(Builder b, float radius, int segments, int rings)
        {
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
                    b.Vertex(x * radius, y * radius, z * radius, x, y, z, -MathF.Sin(theta), 0.0f, MathF.Cos(theta), u * 2.0f, v);
                }
            }
            int stride = segments + 1;
            for (int r = 0; r < rings; r++)
            {
                for (int s = 0; s < segments; s++)
                {
                    uint a = (uint)(r * stride + s);
                    uint c = a + (uint)stride;
                    b.Triangle(a, a + 1, c);
                    b.Triangle(c, a + 1, c + 1);
                }
            }
        }

        private static void Cube(Builder b, float half)
        {
            (float nx, float ny, float nz, float ux, float uy, float uz, float vx, float vy, float vz)[] faces =
            {
                (1, 0, 0, 0, 0, -1, 0, -1, 0), (-1, 0, 0, 0, 0, 1, 0, -1, 0),
                (0, 1, 0, 1, 0, 0, 0, 0, 1), (0, -1, 0, 1, 0, 0, 0, 0, -1),
                (0, 0, 1, 1, 0, 0, 0, -1, 0), (0, 0, -1, -1, 0, 0, 0, -1, 0),
            };
            foreach (var f in faces)
            {
                uint start = (uint)b.Count;
                for (int corner = 0; corner < 4; corner++)
                {
                    float su = corner is 1 or 2 ? 1.0f : -1.0f;
                    float sv = corner is 2 or 3 ? 1.0f : -1.0f;
                    b.Vertex(
                        (f.nx + f.ux * su + f.vx * sv) * half, (f.ny + f.uy * su + f.vy * sv) * half, (f.nz + f.uz * su + f.vz * sv) * half,
                        f.nx, f.ny, f.nz, f.ux, f.uy, f.uz, (su + 1.0f) * 0.5f, (sv + 1.0f) * 0.5f);
                }
                b.Triangle(start, start + 1, start + 2);
                b.Triangle(start, start + 2, start + 3);
            }
        }

        private sealed class Builder
        {
            private readonly List<float> _positions = new();
            private readonly List<float> _normals = new();
            private readonly List<float> _tangents = new();
            private readonly List<float> _uv = new();
            private readonly List<uint> _indices = new();

            public int Count => _positions.Count / 3;

            public void Vertex(float x, float y, float z, float nx, float ny, float nz, float tx, float ty, float tz, float u, float v)
            {
                _positions.AddRange(new[] { x, y, z });
                _normals.AddRange(new[] { nx, ny, nz });
                _tangents.AddRange(new[] { tx, ty, tz, 1.0f });
                _uv.AddRange(new[] { u, v });
            }

            public void Triangle(uint a, uint b, uint c) => _indices.AddRange(new[] { a, b, c });

            public MeshAssets.RawPart ToPart(string name) => new()
            {
                MeshName = name,
                MaterialName = "default",
                Channels = MeshAssets.ChannelNormals | MeshAssets.ChannelTangents | MeshAssets.ChannelUV0,
                Positions = _positions.ToArray(),
                Normals = _normals.ToArray(),
                Tangents = _tangents.ToArray(),
                UV0 = _uv.ToArray(),
                Indices = _indices.ToArray(),
            };
        }
    }

    // Primitive meshes as assets: a ".xcp" composition { "version": 1, "meta": {...}, "shape": "sphere", "size": 1,
    // "segments": 48, "rings": 24 } (embedded meta, like every .xc* file) builds to the same model data as an FBX (one
    // node, one part), so MeshRenderer and the model resource use it unchanged.
    public static class PrimitiveAssets
    {
        public const string Extension = ".xcp";

        public static void Register(AssetPipeline pipeline) => pipeline.Register(new Importer());

        public static MeshAssets.RawPart Read(string path)
        {
            JsonObject root = JsonNode.Parse(File.ReadAllText(path)) as JsonObject ?? throw new InvalidDataException("A primitive must be a JSON object");
            string? shapeText = root["shape"]?.GetValue<string>();
            if (!PrimitiveMeshes.TryParse(shapeText, out PrimitiveShape shape))
                throw new InvalidDataException($"Unknown primitive shape '{shapeText}' (cube, sphere, plane or quad)");
            float size = root["size"] is JsonValue s && s.TryGetValue(out double sizeValue) && sizeValue > 0.0 ? (float)sizeValue : 1.0f;
            int segments = root["segments"] is JsonValue g && g.TryGetValue(out int segmentValue) ? segmentValue : 48;
            int rings = root["rings"] is JsonValue r && r.TryGetValue(out int ringValue) ? ringValue : 24;
            return PrimitiveMeshes.Build(shape, size, segments, rings);
        }

        private sealed class Importer : IAssetImporter
        {
            public string Id => "mesh.primitive";
            public int Version => 1;
            public string DataFolder => "Models";
            public string DataExtension => MeshAssets.ProcessedExtension;

            public bool Accepts(string extension) => extension.Equals(Extension, StringComparison.OrdinalIgnoreCase);

            public string? HashSource(string sourcePath)
            {
                try
                {
                    JsonObject root = JsonNode.Parse(File.ReadAllText(sourcePath)) as JsonObject ?? new JsonObject();
                    root.Remove(AssetMetaStore.EmbeddedKey);
                    return Convert.ToHexStringLower(System.Security.Cryptography.SHA1.HashData(System.Text.Encoding.UTF8.GetBytes(root.ToJsonString())));
                }
                catch (Exception e) when (e is IOException or JsonException or InvalidDataException)
                {
                    return null;
                }
            }

            public IReadOnlyList<AssetDataFile> Import(string sourcePath, JsonObject settings)
            {
                MeshAssets.RawPart part;
                try
                {
                    part = Read(sourcePath);
                }
                catch (JsonException e)
                {
                    throw new InvalidDataException($"Invalid primitive JSON: {e.Message}");
                }
                MeshAssets.RawNode node = new() { Name = part.MeshName, Mesh = 0 };
                return MeshAssets.ToRawFiles(new[] { part }, new[] { node });
            }
        }
    }
}
