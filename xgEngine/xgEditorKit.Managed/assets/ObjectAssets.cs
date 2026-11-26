// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.

using System.Buffers.Binary;
using System.Text;
using System.Text.Json;
using System.Text.Json.Nodes;

namespace xg.editor
{
    public enum ObjectNodeKind
    {
        Mesh = 0,
        Proxy = 1,
    }

    public enum ObjectNodeExpose
    {
        None = 0,
        Actor = 1,
        Proxy = 2,
    }

    // One submesh drawn by a Mesh Object: the model's submesh item GUID (empty = every part) and its material GUID.
    public sealed class ObjectPartAsset
    {
        public string Submesh { get; set; } = string.Empty;
        public string Material { get; set; } = string.Empty;

        public ObjectPartAsset Clone() => new() { Submesh = Submesh, Material = Material };
    }

    // One Object of a composition: a node GUID, a name, a kind, a fixed Jointer (parent + local pose), what it exposes
    // and, for Mesh Objects, a model and its drawn submeshes. Components and physics are kept as JSON (not built yet).
    public sealed class ObjectNodeAsset
    {
        public string Id { get; set; } = string.Empty;
        public string Name { get; set; } = string.Empty;
        public ObjectNodeKind Kind { get; set; } = ObjectNodeKind.Proxy;
        public ObjectNodeExpose Expose { get; set; } = ObjectNodeExpose.None;
        public string Parent { get; set; } = string.Empty;
        public string Jointer { get; set; } = "fixed";
        public float[] Position { get; set; } = { 0.0f, 0.0f, 0.0f };
        public float[] Rotation { get; set; } = { 0.0f, 0.0f, 0.0f, 1.0f };
        public float[] Scale { get; set; } = { 1.0f, 1.0f, 1.0f };
        public string Bone { get; set; } = string.Empty;
        public string Model { get; set; } = string.Empty;
        public List<ObjectPartAsset> Parts { get; } = new();
        public JsonArray? Components { get; set; }
        public JsonNode? Physics { get; set; }

        public ObjectNodeAsset Clone()
        {
            ObjectNodeAsset copy = new()
            {
                Id = Id, Name = Name, Kind = Kind, Expose = Expose, Parent = Parent, Jointer = Jointer,
                Position = (float[])Position.Clone(), Rotation = (float[])Rotation.Clone(), Scale = (float[])Scale.Clone(),
                Bone = Bone, Model = Model, Components = Components?.DeepClone().AsArray(), Physics = Physics?.DeepClone(),
            };
            foreach (ObjectPartAsset part in Parts)
                copy.Parts.Add(part.Clone());
            return copy;
        }

        // Local matrix S * R * T (row-major for row vectors, like the engine's transforms).
        public float[] LocalMatrix()
        {
            float x = Rotation[0], y = Rotation[1], z = Rotation[2], w = Rotation[3];
            float length = MathF.Sqrt(x * x + y * y + z * z + w * w);
            if (length > 0.0f)
            {
                x /= length; y /= length; z /= length; w /= length;
            }
            else
            {
                w = 1.0f;
            }
            float[] m = new float[16];
            m[0] = (1 - 2 * (y * y + z * z)) * Scale[0];
            m[1] = 2 * (x * y + w * z) * Scale[0];
            m[2] = 2 * (x * z - w * y) * Scale[0];
            m[4] = 2 * (x * y - w * z) * Scale[1];
            m[5] = (1 - 2 * (x * x + z * z)) * Scale[1];
            m[6] = 2 * (y * z + w * x) * Scale[1];
            m[8] = 2 * (x * z + w * y) * Scale[2];
            m[9] = 2 * (y * z - w * x) * Scale[2];
            m[10] = (1 - 2 * (x * x + y * y)) * Scale[2];
            m[12] = Position[0];
            m[13] = Position[1];
            m[14] = Position[2];
            m[15] = 1.0f;
            return m;
        }
    }

    // An authored .xco composition object: a tree of Objects joined by Jointers. Meta is the embedded asset meta
    // (AssetMetaStore: guid, tags, names), kept as is; comparisons and builds ignore it.
    public sealed class ObjectAsset
    {
        public const int CurrentVersion = 1;
        public const string Extension = ".xco";

        public int Version { get; set; } = CurrentVersion;
        public JsonObject? Meta { get; set; }
        public List<ObjectNodeAsset> Objects { get; } = new();

        public static bool IsObject(string path) => Path.GetExtension(path).Equals(Extension, StringComparison.OrdinalIgnoreCase);

        public ObjectAsset Clone()
        {
            ObjectAsset copy = new() { Version = Version, Meta = Meta?.DeepClone().AsObject() };
            foreach (ObjectNodeAsset node in Objects)
                copy.Objects.Add(node.Clone());
            return copy;
        }

        public ObjectNodeAsset? Find(string id) => Objects.FirstOrDefault(o => o.Id == id);

        public bool SameAs(ObjectAsset other) => ContentJSON() == other.ContentJSON();

        // The object's JSON without the meta block (what builds hash).
        public string ContentJSON() => Document(false).ToJsonString(JSONFile.Options) + "\r\n";

        public string ToJSON() => Document(true).ToJsonString(JSONFile.Options) + "\r\n";

        public bool Write(string path) => JSONFile.WriteText(path, ToJSON());

        public static ObjectAsset Read(string path) => Parse(File.ReadAllText(path));

        public static ObjectAsset Parse(string json)
        {
            JsonNode? root = JsonNode.Parse(json.TrimStart('﻿'), documentOptions: new JsonDocumentOptions { CommentHandling = JsonCommentHandling.Skip, AllowTrailingCommas = true });
            if (root is not JsonObject document)
                throw new InvalidDataException("An object is a JSON object");
            ObjectAsset asset = new()
            {
                Version = document["version"]?.GetValue<int>() ?? CurrentVersion,
                Meta = document[AssetMetaStore.EmbeddedKey]?.DeepClone() as JsonObject,
            };
            if (asset.Version > CurrentVersion)
                throw new InvalidDataException($"Object version {asset.Version} is newer than {CurrentVersion}");
            if (document["objects"] is JsonArray objects)
                foreach (JsonNode? item in objects)
                    if (item is JsonObject node)
                        asset.Objects.Add(ParseNode(node));
            return asset;
        }

        private static ObjectNodeAsset ParseNode(JsonObject json)
        {
            ObjectNodeAsset node = new()
            {
                Id = Text(json["id"]),
                Name = Text(json["name"]),
                Kind = Text(json["kind"]) == "mesh" ? ObjectNodeKind.Mesh : ObjectNodeKind.Proxy,
                Expose = Text(json["expose"]) switch { "actor" => ObjectNodeExpose.Actor, "proxy" => ObjectNodeExpose.Proxy, _ => ObjectNodeExpose.None },
                Components = json["components"]?.DeepClone() as JsonArray,
                Physics = json["physics"]?.DeepClone(),
            };
            if (json["jointer"] is JsonObject jointer)
            {
                node.Parent = Text(jointer["parent"]);
                node.Jointer = Text(jointer["type"]) is { Length: > 0 } type ? type : "fixed";
                node.Position = Floats(jointer["position"], node.Position);
                node.Rotation = Floats(jointer["rotation"], node.Rotation);
                node.Scale = Floats(jointer["scale"], node.Scale);
                node.Bone = Text(jointer["bone"]);
            }
            if (json["mesh"] is JsonObject mesh)
            {
                node.Model = Text(mesh["model"]);
                if (mesh["parts"] is JsonArray parts)
                    foreach (JsonNode? item in parts)
                        if (item is JsonObject part)
                            node.Parts.Add(new ObjectPartAsset { Submesh = Text(part["submesh"]), Material = Text(part["material"]) });
            }
            return node;
        }

        private static string Text(JsonNode? node) =>
            node is JsonValue value && value.TryGetValue(out string? text) ? text : string.Empty;

        private static float[] Floats(JsonNode? node, float[] fallback)
        {
            float[] result = (float[])fallback.Clone();
            if (node is JsonArray array)
                for (int i = 0; i < Math.Min(array.Count, result.Length); i++)
                    result[i] = array[i]?.GetValue<float>() ?? result[i];
            return result;
        }

        private static JsonArray Array(float[] values) =>
            new(values.Select(v => (JsonNode)JsonValue.Create(MathF.Round(v, 6))!).ToArray());

        private JsonObject Document(bool withMeta)
        {
            JsonArray objects = new();
            foreach (ObjectNodeAsset node in Objects)
            {
                JsonObject jointer = new()
                {
                    ["parent"] = node.Parent.Length > 0 ? node.Parent : null,
                    ["type"] = node.Jointer,
                    ["position"] = Array(node.Position),
                    ["rotation"] = Array(node.Rotation),
                    ["scale"] = Array(node.Scale),
                };
                if (node.Bone.Length > 0)
                    jointer["bone"] = node.Bone;
                JsonObject json = new()
                {
                    ["id"] = node.Id,
                    ["name"] = node.Name,
                    ["kind"] = node.Kind == ObjectNodeKind.Mesh ? "mesh" : "proxy",
                    ["expose"] = node.Expose switch { ObjectNodeExpose.Actor => "actor", ObjectNodeExpose.Proxy => "proxy", _ => "none" },
                    ["jointer"] = jointer,
                };
                if (node.Kind == ObjectNodeKind.Mesh)
                {
                    JsonArray parts = new();
                    foreach (ObjectPartAsset part in node.Parts)
                        parts.Add(new JsonObject { ["submesh"] = part.Submesh, ["material"] = part.Material });
                    json["mesh"] = new JsonObject { ["model"] = node.Model, ["parts"] = parts };
                }
                if (node.Components is not null)
                    json["components"] = node.Components.DeepClone();
                if (node.Physics is not null)
                    json["physics"] = node.Physics.DeepClone();
                objects.Add(json);
            }
            JsonObject document = new() { ["version"] = CurrentVersion };
            if (withMeta && Meta is not null)
                document[AssetMetaStore.EmbeddedKey] = Meta.DeepClone();
            document["objects"] = objects;
            return document;
        }

        // Gives every authored component an "id" (16 hex digits, unique within its Object) when it has none. Returns
        // whether anything changed.
        public bool EnsureComponentIds()
        {
            bool changed = false;
            foreach (ObjectNodeAsset node in Objects)
            {
                HashSet<string> used = new(StringComparer.OrdinalIgnoreCase);
                foreach (JsonObject entry in node.Components?.OfType<JsonObject>() ?? Enumerable.Empty<JsonObject>())
                {
                    string? id = entry["id"] is JsonValue value && value.TryGetValue(out string? text) ? text : null;
                    if (string.IsNullOrEmpty(id) || !used.Add(id))
                    {
                        id = Guid.NewGuid().ToString("N")[..16];
                        used.Add(id);
                        entry["id"] = id;
                        changed = true;
                    }
                }
            }
            return changed;
        }

        // The GUIDs this object references (models, materials), for the name tree in its meta.
        public IEnumerable<string> ReferencedGuids()
        {
            foreach (ObjectNodeAsset node in Objects)
            {
                if (AssetMeta.IsValidGuid(node.Model))
                    yield return node.Model;
                foreach (ObjectPartAsset part in node.Parts)
                    if (AssetMeta.IsValidGuid(part.Material))
                        yield return part.Material;
            }
        }

        // Replaces a referenced file GUID (name-tree repair).
        public bool ReplaceGuid(string oldGuid, string newGuid)
        {
            bool changed = false;
            foreach (ObjectNodeAsset node in Objects)
            {
                if (node.Model == oldGuid)
                {
                    node.Model = newGuid;
                    changed = true;
                }
                foreach (ObjectPartAsset part in node.Parts)
                    if (part.Material == oldGuid)
                    {
                        part.Material = newGuid;
                        changed = true;
                    }
            }
            return changed;
        }

        // The Objects parents first, or the reasons the tree can't be built (missing parent, cycle, duplicate exposed
        // names, bad GUIDs).
        public List<ObjectNodeAsset> Ordered(List<string> errors)
        {
            Dictionary<string, ObjectNodeAsset> byId = new(StringComparer.OrdinalIgnoreCase);
            foreach (ObjectNodeAsset node in Objects)
            {
                if (!AssetMeta.IsValidGuid(node.Id))
                    errors.Add($"Object \"{node.Name}\" has no valid id");
                else if (!byId.TryAdd(node.Id, node))
                    errors.Add($"Object id {node.Id} is used twice");
            }
            foreach (IGrouping<string, ObjectNodeAsset> group in Objects.Where(o => o.Expose != ObjectNodeExpose.None).GroupBy(o => o.Name, StringComparer.Ordinal))
            {
                if (group.Key.Length == 0)
                    errors.Add("An exposed Object has no name");
                else if (group.Count() > 1)
                    errors.Add($"Exposed name \"{group.Key}\" is used by {group.Count()} Objects");
            }
            List<ObjectNodeAsset> ordered = new();
            HashSet<string> placed = new(StringComparer.OrdinalIgnoreCase);
            foreach (ObjectNodeAsset node in Objects)
            {
                if (node.Parent.Length > 0 && !byId.ContainsKey(node.Parent))
                    errors.Add($"Object \"{node.Name}\" names a missing parent");
                else if (node.Parent.Length > 0 && Reaches(node, node.Id, byId))
                    errors.Add($"Object \"{node.Name}\" is its own ancestor");
            }
            if (errors.Count > 0)
                return ordered;
            bool progress = true;
            while (progress && ordered.Count < Objects.Count)
            {
                progress = false;
                foreach (ObjectNodeAsset node in Objects)
                {
                    if (placed.Contains(node.Id) || (node.Parent.Length > 0 && !placed.Contains(node.Parent)))
                        continue;
                    ordered.Add(node);
                    placed.Add(node.Id);
                    progress = true;
                }
            }
            return ordered;
        }

        private static bool Reaches(ObjectNodeAsset start, string target, Dictionary<string, ObjectNodeAsset> byId)
        {
            HashSet<string> seen = new(StringComparer.OrdinalIgnoreCase);
            string parent = start.Parent;
            while (parent.Length > 0 && byId.TryGetValue(parent, out ObjectNodeAsset? node))
            {
                if (string.Equals(parent, target, StringComparison.OrdinalIgnoreCase) || !seen.Add(parent))
                    return true;
                parent = node.Parent;
            }
            return false;
        }

        // A new object from an imported model: a root proxy named after it, then one Mesh Object per mesh of the model
        // (grouped from the submesh items "<mesh>/<material>"), each drawing all of its submeshes, materials empty.
        public static ObjectAsset FromModel(string name, string modelGuid, IEnumerable<AssetItem> items)
        {
            ObjectAsset asset = new();
            ObjectNodeAsset root = new() { Id = AssetMeta.NewGuid(), Name = name, Kind = ObjectNodeKind.Proxy };
            asset.Objects.Add(root);
            foreach (IGrouping<string, AssetItem> mesh in items
                .Where(i => i.Kind == MeshAssets.SubmeshKind && !i.Missing && AssetMeta.IsValidGuid(i.Guid))
                .GroupBy(i => i.Path.Contains('/') ? i.Path[..i.Path.LastIndexOf('/')] : i.Path, StringComparer.Ordinal))
            {
                ObjectNodeAsset node = new()
                {
                    Id = AssetMeta.NewGuid(),
                    Name = mesh.Key.Length > 0 ? mesh.Key : "Mesh",
                    Kind = ObjectNodeKind.Mesh,
                    Parent = root.Id,
                    Model = modelGuid,
                };
                foreach (AssetItem item in mesh)
                    node.Parts.Add(new ObjectPartAsset { Submesh = item.Guid });
                asset.Objects.Add(node);
            }
            return asset;
        }
    }

    // Objects: a .xco becomes <guid>.xdo. The JSON stays in memory, so no raw file is stored. One "object" item holds:
    //   OBHD: layout version, node count, part count, name bytes (32-bit values)
    //   OBND: per node (parents first, 184 bytes) the node GUID, parent index, kind, expose, name offset, name hash
    //         (FNV-1a), the index of the nearest ancestor exposed as an actor (-1 = the driver's actor), the local
    //         pose (position, rotation xyzw, scale), the baked local-to-root matrix and the pose relative to that
    //         ancestor (what a Composition Actor's transform is set to)
    //   OBMS: per drawn part (56 bytes) the node index, model GUID, submesh item GUID and material GUID
    //   OBNM: node names, UTF-8, each zero-terminated
    //   OBCP: authored components of actor nodes: per record the node index and the UTF-8 byte count (32-bit), then
    //         the JSON array (padded to 4 bytes)
    public static class ObjectAssets
    {
        public const string ItemKind = "object";
        public const string RawExtension = ".xro";
        public const string ProcessedExtension = ".xdo";
        public const int LayoutVersion = 2;
        public const int NodeRecordSize = 184;
        public const int PartRecordSize = 56;

        public static readonly uint RawKind = ChunkTag.Make("XRO ");
        public static readonly uint ProcessedKind = ChunkTag.Make("XDO ");
        public static readonly uint SourceChunk = ChunkTag.Make("SRCT");
        public static readonly uint HeaderChunk = ChunkTag.Make("OBHD");
        public static readonly uint NodesChunk = ChunkTag.Make("OBND");
        public static readonly uint PartsChunk = ChunkTag.Make("OBMS");
        public static readonly uint NamesChunk = ChunkTag.Make("OBNM");
        public static readonly uint ComponentsChunk = ChunkTag.Make("OBCP");

        public static void Register(AssetPipeline pipeline)
        {
            pipeline.Register(new Importer());
            pipeline.Register(new Processor());
        }

        public static uint HashName(string name)
        {
            uint hash = 2166136261u;
            foreach (byte b in Encoding.UTF8.GetBytes(name))
            {
                hash ^= b;
                hash *= 16777619u;
            }
            return hash;
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

        public static float[] Invert(float[] m)
        {
            float[] inv = new float[16];
            inv[0] = m[5] * m[10] * m[15] - m[5] * m[11] * m[14] - m[9] * m[6] * m[15] + m[9] * m[7] * m[14] + m[13] * m[6] * m[11] - m[13] * m[7] * m[10];
            inv[4] = -m[4] * m[10] * m[15] + m[4] * m[11] * m[14] + m[8] * m[6] * m[15] - m[8] * m[7] * m[14] - m[12] * m[6] * m[11] + m[12] * m[7] * m[10];
            inv[8] = m[4] * m[9] * m[15] - m[4] * m[11] * m[13] - m[8] * m[5] * m[15] + m[8] * m[7] * m[13] + m[12] * m[5] * m[11] - m[12] * m[7] * m[9];
            inv[12] = -m[4] * m[9] * m[14] + m[4] * m[10] * m[13] + m[8] * m[5] * m[14] - m[8] * m[6] * m[13] - m[12] * m[5] * m[10] + m[12] * m[6] * m[9];
            inv[1] = -m[1] * m[10] * m[15] + m[1] * m[11] * m[14] + m[9] * m[2] * m[15] - m[9] * m[3] * m[14] - m[13] * m[2] * m[11] + m[13] * m[3] * m[10];
            inv[5] = m[0] * m[10] * m[15] - m[0] * m[11] * m[14] - m[8] * m[2] * m[15] + m[8] * m[3] * m[14] + m[12] * m[2] * m[11] - m[12] * m[3] * m[10];
            inv[9] = -m[0] * m[9] * m[15] + m[0] * m[11] * m[13] + m[8] * m[1] * m[15] - m[8] * m[3] * m[13] - m[12] * m[1] * m[11] + m[12] * m[3] * m[9];
            inv[13] = m[0] * m[9] * m[14] - m[0] * m[10] * m[13] - m[8] * m[1] * m[14] + m[8] * m[2] * m[13] + m[12] * m[1] * m[10] - m[12] * m[2] * m[9];
            inv[2] = m[1] * m[6] * m[15] - m[1] * m[7] * m[14] - m[5] * m[2] * m[15] + m[5] * m[3] * m[14] + m[13] * m[2] * m[7] - m[13] * m[3] * m[6];
            inv[6] = -m[0] * m[6] * m[15] + m[0] * m[7] * m[14] + m[4] * m[2] * m[15] - m[4] * m[3] * m[14] - m[12] * m[2] * m[7] + m[12] * m[3] * m[6];
            inv[10] = m[0] * m[5] * m[15] - m[0] * m[7] * m[13] - m[4] * m[1] * m[15] + m[4] * m[3] * m[13] + m[12] * m[1] * m[7] - m[12] * m[3] * m[5];
            inv[14] = -m[0] * m[5] * m[14] + m[0] * m[6] * m[13] + m[4] * m[1] * m[14] - m[4] * m[2] * m[13] - m[12] * m[1] * m[6] + m[12] * m[2] * m[5];
            inv[3] = -m[1] * m[6] * m[11] + m[1] * m[7] * m[10] + m[5] * m[2] * m[11] - m[5] * m[3] * m[10] - m[9] * m[2] * m[7] + m[9] * m[3] * m[6];
            inv[7] = m[0] * m[6] * m[11] - m[0] * m[7] * m[10] - m[4] * m[2] * m[11] + m[4] * m[3] * m[10] + m[8] * m[2] * m[7] - m[8] * m[3] * m[6];
            inv[11] = -m[0] * m[5] * m[11] + m[0] * m[7] * m[9] + m[4] * m[1] * m[11] - m[4] * m[3] * m[9] - m[8] * m[1] * m[7] + m[8] * m[3] * m[5];
            inv[15] = m[0] * m[5] * m[10] - m[0] * m[6] * m[9] - m[4] * m[1] * m[10] + m[4] * m[2] * m[9] + m[8] * m[1] * m[6] - m[8] * m[2] * m[5];
            float det = m[0] * inv[0] + m[1] * inv[4] + m[2] * inv[8] + m[3] * inv[12];
            if (MathF.Abs(det) < 1e-12f)
                return new float[] { 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1 };
            for (int i = 0; i < 16; i++)
                inv[i] /= det;
            return inv;
        }

        // Throws InvalidDataException with every problem when the tree can't be built.
        public static List<ChunkData> Pack(ObjectAsset asset)
        {
            List<string> errors = new();
            List<ObjectNodeAsset> ordered = asset.Ordered(errors);
            if (errors.Count > 0)
                throw new InvalidDataException(string.Join("; ", errors));
            Dictionary<string, int> index = new(StringComparer.OrdinalIgnoreCase);
            for (int i = 0; i < ordered.Count; i++)
                index[ordered[i].Id] = i;

            MemoryStream names = new();
            MemoryStream components = new();
            byte[] nodeBytes = new byte[ordered.Count * NodeRecordSize];
            float[][] baked = new float[ordered.Count][];
            int[] parents = new int[ordered.Count];
            for (int i = 0; i < ordered.Count; i++)
            {
                ObjectNodeAsset node = ordered[i];
                int parent = node.Parent.Length > 0 ? index[node.Parent] : -1;
                parents[i] = parent;
                float[] local = node.LocalMatrix();
                baked[i] = parent >= 0 ? Multiply(local, baked[parent]) : local;
                int actorParent = parent;
                while (actorParent >= 0 && ordered[actorParent].Expose != ObjectNodeExpose.Actor)
                    actorParent = parents[actorParent];
                TransformData actorPose = ObjectDriverComponent.Decompose(actorParent >= 0 ? Multiply(baked[i], Invert(baked[actorParent])) : baked[i]);
                Span<byte> record = nodeBytes.AsSpan(i * NodeRecordSize, NodeRecordSize);
                MaterialAssets.WriteGuid(node.Id, record[..16]);
                BinaryPrimitives.WriteInt32LittleEndian(record[16..], parent);
                BinaryPrimitives.WriteInt32LittleEndian(record[20..], (int)node.Kind);
                BinaryPrimitives.WriteInt32LittleEndian(record[24..], (int)node.Expose);
                BinaryPrimitives.WriteInt32LittleEndian(record[28..], (int)names.Length);
                BinaryPrimitives.WriteUInt32LittleEndian(record[32..], HashName(node.Name));
                BinaryPrimitives.WriteInt32LittleEndian(record[36..], actorParent);
                float[] pose = node.Position.Concat(node.Rotation).Concat(node.Scale).ToArray();
                for (int c = 0; c < 10; c++)
                    BinaryPrimitives.WriteSingleLittleEndian(record[(40 + c * 4)..], pose[c]);
                for (int c = 0; c < 16; c++)
                    BinaryPrimitives.WriteSingleLittleEndian(record[(80 + c * 4)..], baked[i][c]);
                float[] relative =
                {
                    actorPose.PositionX, actorPose.PositionY, actorPose.PositionZ,
                    actorPose.RotationX, actorPose.RotationY, actorPose.RotationZ, actorPose.RotationW,
                    actorPose.ScaleX, actorPose.ScaleY, actorPose.ScaleZ,
                };
                for (int c = 0; c < 10; c++)
                    BinaryPrimitives.WriteSingleLittleEndian(record[(144 + c * 4)..], relative[c]);
                byte[] name = Encoding.UTF8.GetBytes(node.Name);
                names.Write(name);
                names.WriteByte(0);
                if (node.Expose == ObjectNodeExpose.Actor && node.Components is { Count: > 0 } authored)
                {
                    byte[] json = Encoding.UTF8.GetBytes(authored.ToJsonString());
                    Span<byte> head = stackalloc byte[8];
                    BinaryPrimitives.WriteInt32LittleEndian(head, i);
                    BinaryPrimitives.WriteInt32LittleEndian(head[4..], json.Length);
                    components.Write(head);
                    components.Write(json);
                    while (components.Length % 4 != 0)
                        components.WriteByte(0);
                }
            }

            List<(int Node, string Model, ObjectPartAsset Part)> parts = new();
            for (int i = 0; i < ordered.Count; i++)
                if (ordered[i].Kind == ObjectNodeKind.Mesh && AssetMeta.IsValidGuid(ordered[i].Model))
                    foreach (ObjectPartAsset part in ordered[i].Parts)
                        parts.Add((i, ordered[i].Model, part));
            byte[] partBytes = new byte[parts.Count * PartRecordSize];
            for (int i = 0; i < parts.Count; i++)
            {
                Span<byte> record = partBytes.AsSpan(i * PartRecordSize, PartRecordSize);
                BinaryPrimitives.WriteInt32LittleEndian(record, parts[i].Node);
                MaterialAssets.WriteGuid(parts[i].Model, record.Slice(8, 16));
                MaterialAssets.WriteGuid(parts[i].Part.Submesh, record.Slice(24, 16));
                MaterialAssets.WriteGuid(parts[i].Part.Material, record.Slice(40, 16));
            }

            byte[] header = new byte[16];
            BinaryPrimitives.WriteInt32LittleEndian(header, LayoutVersion);
            BinaryPrimitives.WriteInt32LittleEndian(header.AsSpan(4), ordered.Count);
            BinaryPrimitives.WriteInt32LittleEndian(header.AsSpan(8), parts.Count);
            BinaryPrimitives.WriteInt32LittleEndian(header.AsSpan(12), (int)names.Length);
            return new List<ChunkData>
            {
                new(HeaderChunk, header),
                new(NodesChunk, nodeBytes),
                new(PartsChunk, partBytes),
                new(NamesChunk, names.ToArray()),
                new(ComponentsChunk, components.ToArray()),
            };
        }

        public readonly record struct PackedNode(string Id, int Parent, ObjectNodeKind Kind, ObjectNodeExpose Expose, string Name, uint NameHash, float[] Baked,
            int ActorParent, float[] ActorPose);
        public readonly record struct PackedPart(int Node, string Model, string Submesh, string Material);

        // The authored components chunk back as (node index, JSON array) pairs (tests and tools).
        public static List<(int Node, string JSON)> UnpackComponents(IReadOnlyList<ChunkData> chunks)
        {
            List<(int, string)> result = new();
            byte[] data = chunks.First(c => c.Type == ComponentsChunk).Data;
            int cursor = 0;
            while (cursor + 8 <= data.Length)
            {
                int node = BinaryPrimitives.ReadInt32LittleEndian(data.AsSpan(cursor));
                int length = BinaryPrimitives.ReadInt32LittleEndian(data.AsSpan(cursor + 4));
                result.Add((node, Encoding.UTF8.GetString(data, cursor + 8, length)));
                cursor += 8 + ((length + 3) & ~3);
            }
            return result;
        }

        // Reads packed chunks back (tests and tools).
        public static (List<PackedNode> Nodes, List<PackedPart> Parts) Unpack(IReadOnlyList<ChunkData> chunks)
        {
            byte[] header = chunks.First(c => c.Type == HeaderChunk).Data;
            int nodeCount = BinaryPrimitives.ReadInt32LittleEndian(header.AsSpan(4));
            int partCount = BinaryPrimitives.ReadInt32LittleEndian(header.AsSpan(8));
            byte[] nodeBytes = chunks.First(c => c.Type == NodesChunk).Data;
            byte[] partBytes = chunks.First(c => c.Type == PartsChunk).Data;
            byte[] names = chunks.First(c => c.Type == NamesChunk).Data;
            List<PackedNode> nodes = new();
            for (int i = 0; i < nodeCount; i++)
            {
                ReadOnlySpan<byte> record = nodeBytes.AsSpan(i * NodeRecordSize, NodeRecordSize);
                int nameOffset = BinaryPrimitives.ReadInt32LittleEndian(record[28..]);
                int end = System.Array.IndexOf(names, (byte)0, nameOffset);
                float[] baked = new float[16];
                for (int c = 0; c < 16; c++)
                    baked[c] = BinaryPrimitives.ReadSingleLittleEndian(record[(80 + c * 4)..]);
                float[] actorPose = new float[10];
                for (int c = 0; c < 10; c++)
                    actorPose[c] = BinaryPrimitives.ReadSingleLittleEndian(record[(144 + c * 4)..]);
                nodes.Add(new PackedNode(
                    MaterialAssets.ReadGuid(record[..16]),
                    BinaryPrimitives.ReadInt32LittleEndian(record[16..]),
                    (ObjectNodeKind)BinaryPrimitives.ReadInt32LittleEndian(record[20..]),
                    (ObjectNodeExpose)BinaryPrimitives.ReadInt32LittleEndian(record[24..]),
                    Encoding.UTF8.GetString(names, nameOffset, (end < 0 ? names.Length : end) - nameOffset),
                    BinaryPrimitives.ReadUInt32LittleEndian(record[32..]),
                    baked,
                    BinaryPrimitives.ReadInt32LittleEndian(record[36..]),
                    actorPose));
            }
            List<PackedPart> parts = new();
            for (int i = 0; i < partCount; i++)
            {
                ReadOnlySpan<byte> record = partBytes.AsSpan(i * PartRecordSize, PartRecordSize);
                parts.Add(new PackedPart(BinaryPrimitives.ReadInt32LittleEndian(record),
                    MaterialAssets.ReadGuid(record.Slice(8, 16)), MaterialAssets.ReadGuid(record.Slice(24, 16)), MaterialAssets.ReadGuid(record.Slice(40, 16))));
            }
            return (nodes, parts);
        }

        private sealed class Importer : IAssetImporter
        {
            public string Id => "object.xco";
            public int Version => 1;
            public string DataFolder => "Objects";
            public string DataExtension => ProcessedExtension;

            public bool Accepts(string extension) => extension.Equals(ObjectAsset.Extension, StringComparison.OrdinalIgnoreCase);

            public string? HashSource(string sourcePath)
            {
                try
                {
                    ObjectAsset asset = ObjectAsset.Parse(File.ReadAllText(sourcePath));
                    return Convert.ToHexStringLower(System.Security.Cryptography.SHA1.HashData(Encoding.UTF8.GetBytes(asset.ContentJSON())));
                }
                catch (Exception e) when (e is IOException or JsonException or InvalidDataException)
                {
                    return null;
                }
            }

            public IReadOnlyList<AssetDataFile> Import(string sourcePath, JsonObject settings)
            {
                byte[] text = File.ReadAllBytes(sourcePath);
                ObjectAsset.Parse(Encoding.UTF8.GetString(text));
                AssetDataItem item = new(ItemKind, string.Empty, new List<ChunkData> { new(SourceChunk, text) });
                return new[] { new AssetDataFile(RawExtension, RawKind, new List<AssetDataItem> { item }, Store: false) };
            }
        }

        private sealed class Processor : IAssetProcessor
        {
            public string Id => "object.pack";
            public int Version => 1;

            public bool Accepts(string rawExtension) => rawExtension.Equals(RawExtension, StringComparison.OrdinalIgnoreCase);

            public AssetDataFile Process(AssetDataFile raw, JsonObject settings)
            {
                List<AssetDataItem> items = new();
                List<string> tags = new();
                foreach (AssetDataItem item in raw.Items)
                {
                    ObjectAsset asset = ObjectAsset.Parse(Encoding.UTF8.GetString(item.Chunks.First(c => c.Type == SourceChunk).Data));
                    items.Add(new AssetDataItem(item.Kind, item.Path, Pack(asset)));
                    tags.Add($"@object:{asset.Objects.Count}nodes");
                    tags.Add($"@object:{asset.Objects.Count(o => o.Kind == ObjectNodeKind.Mesh)}meshes");
                    int actors = asset.Objects.Count(o => o.Expose == ObjectNodeExpose.Actor);
                    if (actors > 0)
                        tags.Add($"@object:{actors}actors");
                }
                return new AssetDataFile(ProcessedExtension, ProcessedKind, items, Tags: tags);
            }
        }
    }
}
