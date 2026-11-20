// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.

using System.Buffers.Binary;
using System.Runtime.InteropServices;
using System.Text;
using System.Text.Json.Nodes;

namespace xg.editor
{
    // Meshes: an FBX source becomes <guid>.xrv (submeshes, one channel per chunk) and <guid>.xrh (the node hierarchy),
    // both kept in Library so reprocessing never reads the FBX again, then <guid>.xdv and <guid>.xdh.
    //   .xrv item "submesh", path "<mesh>/<material>" (an unnamed mesh takes its first node's name): MSHD (mesh index, material index, vertex and index counts,
    //        channel flags, bounds), PVEC float3, NVEC float3, TVEC float4, UVC0 float2, CVEC RGBA8, IVEC uint32
    //   .xrh item "hierarchy": NODE (count, version, then per node parent, mesh, name offset and length, local and
    //        geometry 4x4) and NAME (UTF-8 names)
    //   .xdv item "submesh": VHDR (vertex and index counts, stride, layout version, mesh index, material index, channel
    //        flags, bounds), VTXS (position, normal, tangent, uv0, color interleaved, Stride bytes) and IX16 or IX32
    //   .xdh item "hierarchy": NODE with each node's world matrix appended, and NAME
    // Matrices are row-major for row vectors (v * M); parts and nodes link by mesh index.
    public static class MeshAssets
    {
        public const string SubmeshKind = "submesh";
        public const string HierarchyKind = "hierarchy";
        public const string RawMeshExtension = ".xrv";
        public const string RawHierarchyExtension = ".xrh";
        public const string ProcessedExtension = ".xdv";
        public const string HierarchyExtension = ".xdh";
        public const int Stride = 52;
        public const int LayoutVersion = 1;
        public const int RawNodeSize = 144;
        public const int ProcessedNodeSize = 208;
        public const uint ChannelNormals = 1;
        public const uint ChannelTangents = 2;
        public const uint ChannelUV0 = 4;
        public const uint ChannelColors = 8;
        public const string ImportTagPrefix = "@mesh:";

        public static readonly uint RawMeshKind = ChunkTag.Make("XRV ");
        public static readonly uint RawHierarchyKind = ChunkTag.Make("XRH ");
        public static readonly uint ProcessedKind = ChunkTag.Make("XDV ");
        public static readonly uint HierarchyFileKind = ChunkTag.Make("XDH ");
        public static readonly uint MeshHeaderChunk = ChunkTag.Make("MSHD");
        public static readonly uint PositionsChunk = ChunkTag.Make("PVEC");
        public static readonly uint NormalsChunk = ChunkTag.Make("NVEC");
        public static readonly uint TangentsChunk = ChunkTag.Make("TVEC");
        public static readonly uint UVChunk = ChunkTag.Make("UVC0");
        public static readonly uint ColorsChunk = ChunkTag.Make("CVEC");
        public static readonly uint IndicesChunk = ChunkTag.Make("IVEC");
        public static readonly uint NodesChunk = ChunkTag.Make("NODE");
        public static readonly uint NamesChunk = ChunkTag.Make("NAME");
        public static readonly uint VertexHeaderChunk = ChunkTag.Make("VHDR");
        public static readonly uint VerticesChunk = ChunkTag.Make("VTXS");
        public static readonly uint Indices16Chunk = ChunkTag.Make("IX16");
        public static readonly uint Indices32Chunk = ChunkTag.Make("IX32");

        public static void Register(AssetPipeline pipeline)
        {
            pipeline.Register(new FbxImporter());
            pipeline.Register(new MeshProcessor());
            pipeline.Register(new HierarchyProcessor());
        }

        // ------------------------------------------------------------
        // Raw data (in memory)
        // ------------------------------------------------------------
        public sealed class RawPart
        {
            public string MeshName = string.Empty;
            public string MaterialName = string.Empty;
            public int MeshIndex;
            public int MaterialIndex;
            public uint Channels;
            public float[] Positions = Array.Empty<float>();
            public float[]? Normals;
            public float[]? Tangents;
            public float[]? UV0;
            public byte[]? Colors;
            public uint[] Indices = Array.Empty<uint>();
            public int VertexCount => Positions.Length / 3;
        }

        public sealed class RawNode
        {
            public string Name = string.Empty;
            public int Parent = -1;
            public int Mesh = -1;
            public float[] Local = Identity();
            public float[] Geometry = Identity();
        }

        // Builds the raw files from parts and nodes (the FBX importer's output, also used by tests).
        public static List<AssetDataFile> ToRawFiles(IReadOnlyList<RawPart> parts, IReadOnlyList<RawNode> nodes)
        {
            List<AssetDataItem> items = new();
            foreach (RawPart part in parts)
            {
                string mesh = part.MeshName.Length > 0 ? part.MeshName : $"mesh{part.MeshIndex}";
                string material = part.MaterialName.Length > 0 ? part.MaterialName : $"material{part.MaterialIndex}";
                List<ChunkData> chunks = new()
                {
                    new(MeshHeaderChunk, MeshHeader(part)),
                    new(PositionsChunk, Floats(part.Positions)),
                };
                if (part.Normals is not null)
                    chunks.Add(new ChunkData(NormalsChunk, Floats(part.Normals)));
                if (part.Tangents is not null)
                    chunks.Add(new ChunkData(TangentsChunk, Floats(part.Tangents)));
                if (part.UV0 is not null)
                    chunks.Add(new ChunkData(UVChunk, Floats(part.UV0)));
                if (part.Colors is not null)
                    chunks.Add(new ChunkData(ColorsChunk, part.Colors));
                chunks.Add(new ChunkData(IndicesChunk, UInts(part.Indices)));
                items.Add(new AssetDataItem(SubmeshKind, $"{mesh}/{material}", chunks));
            }
            (byte[] nodeData, byte[] names) = NodeTable(nodes, null);
            AssetDataItem hierarchy = new(HierarchyKind, string.Empty, new List<ChunkData> { new(NodesChunk, nodeData), new(NamesChunk, names) });
            return new List<AssetDataFile>
            {
                new(RawMeshExtension, RawMeshKind, items),
                new(RawHierarchyExtension, RawHierarchyKind, new List<AssetDataItem> { hierarchy }),
            };
        }

        public static RawPart ReadRawPart(AssetDataItem item)
        {
            byte[] header = Find(item, MeshHeaderChunk).Data;
            RawPart part = new()
            {
                MeshIndex = BinaryPrimitives.ReadInt32LittleEndian(header.AsSpan(0)),
                MaterialIndex = BinaryPrimitives.ReadInt32LittleEndian(header.AsSpan(4)),
                Channels = BinaryPrimitives.ReadUInt32LittleEndian(header.AsSpan(16)),
                Positions = ToFloats(Find(item, PositionsChunk).Data),
                Normals = TryFind(item, NormalsChunk) is { } normals ? ToFloats(normals.Data) : null,
                Tangents = TryFind(item, TangentsChunk) is { } tangents ? ToFloats(tangents.Data) : null,
                UV0 = TryFind(item, UVChunk) is { } uv ? ToFloats(uv.Data) : null,
                Colors = TryFind(item, ColorsChunk)?.Data,
                Indices = ToUInts(Find(item, IndicesChunk).Data),
            };
            int slash = item.Path.IndexOf('/');
            part.MeshName = slash < 0 ? item.Path : item.Path[..slash];
            part.MaterialName = slash < 0 ? string.Empty : item.Path[(slash + 1)..];
            return part;
        }

        public static List<RawNode> ReadNodes(AssetDataItem item)
        {
            byte[] data = Find(item, NodesChunk).Data;
            byte[] names = Find(item, NamesChunk).Data;
            int count = BinaryPrimitives.ReadInt32LittleEndian(data.AsSpan(0));
            int size = data.Length > 8 ? (data.Length - 8) / Math.Max(count, 1) : RawNodeSize;
            List<RawNode> nodes = new(count);
            for (int i = 0; i < count; i++)
            {
                ReadOnlySpan<byte> node = data.AsSpan(8 + i * size, size);
                int nameOffset = BinaryPrimitives.ReadInt32LittleEndian(node[8..]);
                int nameLength = BinaryPrimitives.ReadInt32LittleEndian(node[12..]);
                nodes.Add(new RawNode
                {
                    Parent = BinaryPrimitives.ReadInt32LittleEndian(node),
                    Mesh = BinaryPrimitives.ReadInt32LittleEndian(node[4..]),
                    Name = Encoding.UTF8.GetString(names, nameOffset, nameLength),
                    Local = ToFloats(node.Slice(16, 64).ToArray()),
                    Geometry = ToFloats(node.Slice(80, 64).ToArray()),
                });
            }
            return nodes;
        }

        // Each node's world matrix: local times the parent's world (parents come first; a bad parent counts as root).
        public static float[][] WorldMatrices(IReadOnlyList<RawNode> nodes)
        {
            float[][] world = new float[nodes.Count][];
            for (int i = 0; i < nodes.Count; i++)
            {
                int parent = nodes[i].Parent;
                world[i] = parent >= 0 && parent < i ? Multiply(nodes[i].Local, world[parent]) : (float[])nodes[i].Local.Clone();
            }
            return world;
        }

        public static float[] Multiply(float[] a, float[] b)
        {
            float[] result = new float[16];
            for (int row = 0; row < 4; row++)
                for (int column = 0; column < 4; column++)
                {
                    float sum = 0.0f;
                    for (int k = 0; k < 4; k++)
                        sum += a[row * 4 + k] * b[k * 4 + column];
                    result[row * 4 + column] = sum;
                }
            return result;
        }

        public static float[] Identity() => new float[] { 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1 };

        public static IEnumerable<string> ImportTags(IReadOnlyList<RawPart> parts)
        {
            long triangles = parts.Sum(p => (long)p.Indices.Length / 3);
            yield return $"{ImportTagPrefix}{triangles}tris";
            yield return $"{ImportTagPrefix}{parts.Count}parts";
            if (parts.Any(p => (p.Channels & ChannelNormals) != 0))
                yield return ImportTagPrefix + "normals";
            if (parts.Any(p => (p.Channels & ChannelUV0) != 0))
                yield return ImportTagPrefix + "uv";
            if (parts.Any(p => (p.Channels & ChannelColors) != 0))
                yield return ImportTagPrefix + "colors";
        }

        // ------------------------------------------------------------
        // Processing
        // ------------------------------------------------------------
        public static byte[] Interleave(RawPart part)
        {
            int count = part.VertexCount;
            byte[] data = new byte[count * Stride];
            Span<byte> span = data;
            for (int i = 0; i < count; i++)
            {
                Span<byte> v = span.Slice(i * Stride, Stride);
                for (int k = 0; k < 3; k++)
                    BinaryPrimitives.WriteSingleLittleEndian(v[(k * 4)..], part.Positions[i * 3 + k]);
                for (int k = 0; k < 3; k++)
                    BinaryPrimitives.WriteSingleLittleEndian(v[(12 + k * 4)..], part.Normals is null ? (k == 1 ? 1.0f : 0.0f) : part.Normals[i * 3 + k]);
                for (int k = 0; k < 4; k++)
                    BinaryPrimitives.WriteSingleLittleEndian(v[(24 + k * 4)..], part.Tangents is null ? (k == 0 || k == 3 ? 1.0f : 0.0f) : part.Tangents[i * 4 + k]);
                for (int k = 0; k < 2; k++)
                    BinaryPrimitives.WriteSingleLittleEndian(v[(40 + k * 4)..], part.UV0 is null ? 0.0f : part.UV0[i * 2 + k]);
                for (int k = 0; k < 4; k++)
                    v[48 + k] = part.Colors is null ? (byte)255 : part.Colors[i * 4 + k];
            }
            return data;
        }

        public static ChunkData IndexChunk(uint[] indices)
        {
            if (indices.All(i => i <= ushort.MaxValue))
            {
                byte[] small = new byte[indices.Length * 2];
                for (int i = 0; i < indices.Length; i++)
                    BinaryPrimitives.WriteUInt16LittleEndian(small.AsSpan(i * 2), (ushort)indices[i]);
                return new ChunkData(Indices16Chunk, small);
            }
            return new ChunkData(Indices32Chunk, UInts(indices));
        }

        private static byte[] MeshHeader(RawPart part)
        {
            byte[] header = new byte[44];
            Span<byte> h = header;
            BinaryPrimitives.WriteInt32LittleEndian(h, part.MeshIndex);
            BinaryPrimitives.WriteInt32LittleEndian(h[4..], part.MaterialIndex);
            BinaryPrimitives.WriteInt32LittleEndian(h[8..], part.VertexCount);
            BinaryPrimitives.WriteInt32LittleEndian(h[12..], part.Indices.Length);
            BinaryPrimitives.WriteUInt32LittleEndian(h[16..], part.Channels);
            WriteBounds(h[20..], part.Positions);
            return header;
        }

        private static byte[] VertexHeader(RawPart part)
        {
            byte[] header = new byte[52];
            Span<byte> h = header;
            BinaryPrimitives.WriteInt32LittleEndian(h, part.VertexCount);
            BinaryPrimitives.WriteInt32LittleEndian(h[4..], part.Indices.Length);
            BinaryPrimitives.WriteInt32LittleEndian(h[8..], Stride);
            BinaryPrimitives.WriteInt32LittleEndian(h[12..], LayoutVersion);
            BinaryPrimitives.WriteInt32LittleEndian(h[16..], part.MeshIndex);
            BinaryPrimitives.WriteInt32LittleEndian(h[20..], part.MaterialIndex);
            BinaryPrimitives.WriteUInt32LittleEndian(h[24..], part.Channels);
            WriteBounds(h[28..], part.Positions);
            return header;
        }

        private static void WriteBounds(Span<byte> target, float[] positions)
        {
            float[] min = { 0, 0, 0 };
            float[] max = { 0, 0, 0 };
            for (int i = 0; i < positions.Length; i += 3)
                for (int k = 0; k < 3; k++)
                {
                    float value = positions[i + k];
                    if (i == 0 || value < min[k])
                        min[k] = value;
                    if (i == 0 || value > max[k])
                        max[k] = value;
                }
            for (int k = 0; k < 3; k++)
            {
                BinaryPrimitives.WriteSingleLittleEndian(target[(k * 4)..], min[k]);
                BinaryPrimitives.WriteSingleLittleEndian(target[(12 + k * 4)..], max[k]);
            }
        }

        private static (byte[] Nodes, byte[] Names) NodeTable(IReadOnlyList<RawNode> nodes, float[][]? world)
        {
            int size = world is null ? RawNodeSize : ProcessedNodeSize;
            byte[] data = new byte[8 + nodes.Count * size];
            BinaryPrimitives.WriteInt32LittleEndian(data, nodes.Count);
            BinaryPrimitives.WriteInt32LittleEndian(data.AsSpan(4), LayoutVersion);
            using MemoryStream names = new();
            for (int i = 0; i < nodes.Count; i++)
            {
                Span<byte> n = data.AsSpan(8 + i * size, size);
                byte[] name = Encoding.UTF8.GetBytes(nodes[i].Name);
                BinaryPrimitives.WriteInt32LittleEndian(n, nodes[i].Parent);
                BinaryPrimitives.WriteInt32LittleEndian(n[4..], nodes[i].Mesh);
                BinaryPrimitives.WriteInt32LittleEndian(n[8..], (int)names.Length);
                BinaryPrimitives.WriteInt32LittleEndian(n[12..], name.Length);
                names.Write(name);
                Floats(nodes[i].Local).CopyTo(n[16..]);
                Floats(nodes[i].Geometry).CopyTo(n[80..]);
                if (world is not null)
                    Floats(world[i]).CopyTo(n[144..]);
            }
            return (data, names.ToArray());
        }

        private static ChunkData Find(AssetDataItem item, uint type) =>
            TryFind(item, type) ?? throw new InvalidDataException($"Mesh data has no {ChunkTag.Name(type)} chunk");

        private static ChunkData? TryFind(AssetDataItem item, uint type) => item.Chunks.FirstOrDefault(c => c.Type == type);

        private static byte[] Floats(float[] values)
        {
            byte[] bytes = new byte[values.Length * 4];
            for (int i = 0; i < values.Length; i++)
                BinaryPrimitives.WriteSingleLittleEndian(bytes.AsSpan(i * 4), values[i]);
            return bytes;
        }

        private static byte[] UInts(uint[] values)
        {
            byte[] bytes = new byte[values.Length * 4];
            for (int i = 0; i < values.Length; i++)
                BinaryPrimitives.WriteUInt32LittleEndian(bytes.AsSpan(i * 4), values[i]);
            return bytes;
        }

        private static float[] ToFloats(byte[] bytes)
        {
            float[] values = new float[bytes.Length / 4];
            for (int i = 0; i < values.Length; i++)
                values[i] = BinaryPrimitives.ReadSingleLittleEndian(bytes.AsSpan(i * 4));
            return values;
        }

        private static uint[] ToUInts(byte[] bytes)
        {
            uint[] values = new uint[bytes.Length / 4];
            for (int i = 0; i < values.Length; i++)
                values[i] = BinaryPrimitives.ReadUInt32LittleEndian(bytes.AsSpan(i * 4));
            return values;
        }

        // ------------------------------------------------------------
        // Pipeline steps
        // ------------------------------------------------------------
        private sealed class MeshProcessor : IAssetProcessor
        {
            public string Id => "mesh.interleave";
            public int Version => 1;
            public bool Accepts(string rawExtension) => rawExtension.Equals(RawMeshExtension, StringComparison.OrdinalIgnoreCase);

            public AssetDataFile Process(AssetDataFile raw, JsonObject settings)
            {
                List<RawPart> parts = raw.Items.Select(ReadRawPart).ToList();
                List<AssetDataItem> items = new();
                for (int i = 0; i < parts.Count; i++)
                {
                    RawPart part = parts[i];
                    items.Add(new AssetDataItem(raw.Items[i].Kind, raw.Items[i].Path, new List<ChunkData>
                    {
                        new(VertexHeaderChunk, VertexHeader(part)),
                        new(VerticesChunk, Interleave(part)),
                        IndexChunk(part.Indices),
                    }));
                }
                return new AssetDataFile(ProcessedExtension, ProcessedKind, items, Tags: ImportTags(parts).ToList());
            }
        }

        private sealed class HierarchyProcessor : IAssetProcessor
        {
            public string Id => "mesh.hierarchy";
            public int Version => 1;
            public bool Accepts(string rawExtension) => rawExtension.Equals(RawHierarchyExtension, StringComparison.OrdinalIgnoreCase);

            public AssetDataFile Process(AssetDataFile raw, JsonObject settings)
            {
                List<AssetDataItem> items = new();
                foreach (AssetDataItem item in raw.Items)
                {
                    List<RawNode> nodes = ReadNodes(item);
                    (byte[] data, byte[] names) = NodeTable(nodes, WorldMatrices(nodes));
                    items.Add(new AssetDataItem(item.Kind, item.Path, new List<ChunkData> { new(NodesChunk, data), new(NamesChunk, names) }));
                }
                return new AssetDataFile(HierarchyExtension, HierarchyFileKind, items);
            }
        }

        private sealed class FbxImporter : IAssetImporter
        {
            public string Id => "mesh.fbx";
            public int Version => 3;
            public string DataFolder => "Models";
            public string DataExtension => ProcessedExtension;

            public bool Accepts(string extension) => extension.Equals(".fbx", StringComparison.OrdinalIgnoreCase);

            public void WriteDefaultSettings(JsonObject import, JsonObject process) => import["scale"] = 1.0;

            public IReadOnlyList<AssetDataFile> Import(string sourcePath, JsonObject settings)
            {
                float scale = settings["scale"] is JsonValue value && value.TryGetValue(out double s) && s > 0.0 ? (float)s : 1.0f;
                (List<RawPart> parts, List<RawNode> nodes) = FbxReader.Read(sourcePath, scale);
                return ToRawFiles(parts, nodes);
            }
        }
    }

    // The editor kernel's ufbx exports (xgEditorKernel/EditorMeshExports.cpp).
    public static unsafe partial class FbxReader
    {
        private const string Native = "xgEditorKernel";

        [StructLayout(LayoutKind.Sequential)]
        private struct ImportOptions
        {
            public float Scale;
            public uint Flags;
        }

        [StructLayout(LayoutKind.Sequential)]
        private struct Counts
        {
            public int Nodes;
            public int Meshes;
            public int Materials;
        }

        [StructLayout(LayoutKind.Sequential)]
        private struct Node
        {
            public byte* Name;
            public int Parent;
            public int Mesh;
            public fixed float Local[16];
            public fixed float Geometry[16];
        }

        [StructLayout(LayoutKind.Sequential)]
        private struct PartInfo
        {
            public byte* MeshName;
            public byte* MaterialName;
            public int MaterialIndex;
            public int VertexCount;
            public int IndexCount;
            public uint Channels;
            public fixed float BoundsMin[3];
            public fixed float BoundsMax[3];
        }

        public static (List<MeshAssets.RawPart> Parts, List<MeshAssets.RawNode> Nodes) Read(string path, float scale)
        {
            byte* error = stackalloc byte[512];
            ImportOptions options = new() { Scale = scale };
            nint handle = XG_EditorFbxOpen(path, &options, error, 512);
            if (handle == 0)
                throw new InvalidDataException($"Could not read {Path.GetFileName(path)}: {Marshal.PtrToStringUTF8((nint)error)?.Trim()}");
            try
            {
                Counts counts;
                XG_EditorFbxGetCounts(handle, &counts);
                List<MeshAssets.RawNode> nodes = new(counts.Nodes);
                for (int i = 0; i < counts.Nodes; i++)
                {
                    Node node;
                    if (!XG_EditorFbxGetNode(handle, i, &node))
                        continue;
                    nodes.Add(new MeshAssets.RawNode
                    {
                        Name = Marshal.PtrToStringUTF8((nint)node.Name) ?? string.Empty,
                        Parent = node.Parent,
                        Mesh = node.Mesh,
                        Local = new ReadOnlySpan<float>(node.Local, 16).ToArray(),
                        Geometry = new ReadOnlySpan<float>(node.Geometry, 16).ToArray(),
                    });
                }

                List<MeshAssets.RawPart> parts = new();
                for (int mesh = 0; mesh < counts.Meshes; mesh++)
                {
                    int partCount = XG_EditorFbxGetPartCount(handle, mesh);
                    for (int p = 0; p < partCount; p++)
                    {
                        PartInfo info;
                        if (!XG_EditorFbxGetPart(handle, mesh, p, &info) || info.VertexCount == 0 || info.IndexCount == 0)
                            continue;
                        string meshName = Marshal.PtrToStringUTF8((nint)info.MeshName) ?? string.Empty;
                        if (meshName.Length == 0)
                            meshName = nodes.FirstOrDefault(n => n.Mesh == mesh && n.Name.Length > 0)?.Name ?? string.Empty;
                        MeshAssets.RawPart part = new()
                        {
                            MeshName = meshName,
                            MaterialName = Marshal.PtrToStringUTF8((nint)info.MaterialName) ?? string.Empty,
                            MeshIndex = mesh,
                            MaterialIndex = info.MaterialIndex,
                            Channels = info.Channels,
                            Positions = new float[info.VertexCount * 3],
                            Normals = (info.Channels & MeshAssets.ChannelNormals) != 0 ? new float[info.VertexCount * 3] : null,
                            Tangents = (info.Channels & MeshAssets.ChannelTangents) != 0 ? new float[info.VertexCount * 4] : null,
                            UV0 = (info.Channels & MeshAssets.ChannelUV0) != 0 ? new float[info.VertexCount * 2] : null,
                            Colors = (info.Channels & MeshAssets.ChannelColors) != 0 ? new byte[info.VertexCount * 4] : null,
                            Indices = new uint[info.IndexCount],
                        };
                        fixed (float* positions = part.Positions)
                        fixed (float* normals = part.Normals)
                        fixed (float* tangents = part.Tangents)
                        fixed (float* uv = part.UV0)
                        fixed (byte* colors = part.Colors)
                        fixed (uint* indices = part.Indices)
                        {
                            if (!XG_EditorFbxReadPart(handle, mesh, p, positions, normals, tangents, uv, colors, indices))
                                throw new InvalidDataException($"Could not read mesh {part.MeshName} of {Path.GetFileName(path)}");
                        }
                        parts.Add(part);
                    }
                }
                if (parts.Count == 0)
                    throw new InvalidDataException($"{Path.GetFileName(path)} has no mesh");
                return (parts, nodes);
            }
            finally
            {
                XG_EditorFbxClose(handle);
            }
        }

        [LibraryImport(Native, EntryPoint = "XG_EditorFbxOpen", StringMarshalling = StringMarshalling.Utf8)]
        private static partial nint XG_EditorFbxOpen(string path, ImportOptions* options, byte* error, int capacity);

        [LibraryImport(Native, EntryPoint = "XG_EditorFbxClose")]
        private static partial void XG_EditorFbxClose(nint handle);

        [LibraryImport(Native, EntryPoint = "XG_EditorFbxGetCounts")]
        [return: MarshalAs(UnmanagedType.U1)]
        private static partial bool XG_EditorFbxGetCounts(nint handle, Counts* counts);

        [LibraryImport(Native, EntryPoint = "XG_EditorFbxGetNode")]
        [return: MarshalAs(UnmanagedType.U1)]
        private static partial bool XG_EditorFbxGetNode(nint handle, int index, Node* node);

        [LibraryImport(Native, EntryPoint = "XG_EditorFbxGetPartCount")]
        private static partial int XG_EditorFbxGetPartCount(nint handle, int mesh);

        [LibraryImport(Native, EntryPoint = "XG_EditorFbxGetPart")]
        [return: MarshalAs(UnmanagedType.U1)]
        private static partial bool XG_EditorFbxGetPart(nint handle, int mesh, int part, PartInfo* info);

        [LibraryImport(Native, EntryPoint = "XG_EditorFbxReadPart")]
        [return: MarshalAs(UnmanagedType.U1)]
        private static partial bool XG_EditorFbxReadPart(nint handle, int mesh, int part, float* positions, float* normals, float* tangents,
            float* uv0, byte* colors, uint* indices);
    }
}
