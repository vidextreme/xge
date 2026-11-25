// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.

using System.Buffers.Binary;
using System.Text;
using System.Text.Json;
using System.Text.Json.Nodes;

namespace xg.editor
{
    // An authored .xcm material: a shader (asset GUID), param values and texture breadcrumbs, keyed by the names the
    // shader's annotations declare. Names the shader doesn't (or no longer) declare are kept and ignored at runtime.
    // Meta is the embedded asset meta (AssetMetaStore: guid, tags, names), kept as is; comparisons and builds ignore it.
    public sealed class MaterialAsset
    {
        public const int CurrentVersion = 2;
        public const string Extension = ".xcm";

        public int Version { get; set; } = CurrentVersion;
        public JsonObject? Meta { get; set; }
        public string Shader { get; set; } = string.Empty;
        public SortedDictionary<string, float[]> Params { get; } = new(StringComparer.Ordinal);
        public SortedDictionary<string, string> Textures { get; } = new(StringComparer.Ordinal);

        public static bool IsMaterial(string path) => Path.GetExtension(path).Equals(Extension, StringComparison.OrdinalIgnoreCase);

        public MaterialAsset Clone()
        {
            MaterialAsset copy = new() { Version = Version, Shader = Shader, Meta = Meta?.DeepClone().AsObject() };
            foreach (KeyValuePair<string, float[]> pair in Params)
                copy.Params[pair.Key] = (float[])pair.Value.Clone();
            foreach (KeyValuePair<string, string> pair in Textures)
                copy.Textures[pair.Key] = pair.Value;
            return copy;
        }

        // Same content (shader, params, textures); the meta block doesn't count.
        public bool SameAs(MaterialAsset other) => ContentJSON() == other.ContentJSON();

        // The material's own JSON without the meta block (what builds hash).
        public string ContentJSON() => Document(false).ToJsonString(JSONFile.Options) + "\r\n";

        // A name tree entry kept in the meta ("names": GUID -> asset path).
        public IReadOnlyDictionary<string, string> Names()
        {
            Dictionary<string, string> names = new(StringComparer.OrdinalIgnoreCase);
            if (Meta?["names"] is JsonObject table)
                foreach ((string guid, JsonNode? path) in table)
                    if (path is JsonValue value && value.TryGetValue(out string? text))
                        names[guid] = text;
            return names;
        }

        public void SetNames(IReadOnlyDictionary<string, string> names)
        {
            Meta ??= new JsonObject();
            JsonObject table = new();
            foreach ((string guid, string path) in names.OrderBy(p => p.Key, StringComparer.Ordinal))
                table[guid] = path;
            Meta["names"] = table;
        }

        public static MaterialAsset Parse(string json)
        {
            JsonNode? root = JsonNode.Parse(json, documentOptions: new JsonDocumentOptions { CommentHandling = JsonCommentHandling.Skip, AllowTrailingCommas = true });
            if (root is not JsonObject document)
                throw new InvalidDataException("A material is a JSON object");
            MaterialAsset material = new()
            {
                Version = document["version"]?.GetValue<int>() ?? CurrentVersion,
                Shader = document["shader"]?.GetValue<string>() ?? string.Empty,
                Meta = document[AssetMetaStore.EmbeddedKey]?.DeepClone() as JsonObject,
            };
            if (material.Version > CurrentVersion)
                throw new InvalidDataException($"Material version {material.Version} is newer than {CurrentVersion}");
            if (document["params"] is JsonObject parameters)
            {
                foreach (KeyValuePair<string, JsonNode?> pair in parameters)
                {
                    float[] value = new float[4];
                    if (pair.Value is JsonArray array)
                    {
                        for (int i = 0; i < Math.Min(array.Count, 4); i++)
                            value[i] = array[i]?.GetValue<float>() ?? 0.0f;
                    }
                    else if (pair.Value is JsonValue single)
                    {
                        value[0] = single.GetValue<float>();
                    }
                    material.Params[pair.Key] = value;
                }
            }
            if (document["textures"] is JsonObject textures)
            {
                foreach (KeyValuePair<string, JsonNode?> pair in textures)
                {
                    string? id = pair.Value?.GetValue<string>();
                    if (!string.IsNullOrEmpty(id))
                        material.Textures[pair.Key] = id;
                }
            }
            return material;
        }

        public static MaterialAsset Read(string path) => Parse(File.ReadAllText(path));

        public string ToJSON() => Document(true).ToJsonString(JSONFile.Options) + "\r\n";

        private JsonObject Document(bool withMeta)
        {
            JsonObject parameters = new();
            foreach (KeyValuePair<string, float[]> pair in Params)
                parameters[pair.Key] = new JsonArray(pair.Value.Select(v => (JsonNode)JsonValue.Create(MathF.Round(v, 6))!).ToArray());
            JsonObject textures = new();
            foreach (KeyValuePair<string, string> pair in Textures)
                textures[pair.Key] = pair.Value;
            JsonObject document = new() { ["version"] = CurrentVersion };
            if (withMeta && Meta is not null)
                document[AssetMetaStore.EmbeddedKey] = Meta.DeepClone();
            document["shader"] = Shader;
            document["params"] = parameters;
            document["textures"] = textures;
            return document;
        }

        public bool Write(string path) => JSONFile.WriteText(path, ToJSON());

        // The GUIDs this material references (shader, texture files), for the name tree in its meta.
        public IEnumerable<string> ReferencedGuids()
        {
            if (AssetMeta.IsValidGuid(Shader))
                yield return Shader;
            foreach (string id in Textures.Values)
            {
                string file = id.Split('/')[0];
                if (AssetMeta.IsValidGuid(file))
                    yield return file;
            }
        }

        // Replaces a referenced file GUID (name-tree repair).
        public bool ReplaceGuid(string oldGuid, string newGuid)
        {
            bool changed = false;
            if (Shader == oldGuid)
            {
                Shader = newGuid;
                changed = true;
            }
            foreach (string name in Textures.Keys.ToList())
            {
                string[] parts = Textures[name].Split('/');
                if (parts[0] != oldGuid)
                    continue;
                parts[0] = newGuid;
                Textures[name] = string.Join("/", parts);
                changed = true;
            }
            return changed;
        }
    }

    // Materials: a .xcm becomes <guid>.xdm. The JSON stays in memory, so no raw file is stored. One "material" item holds:
    //   MTHD: layout version, param count, texture count (32-bit values), then the shader GUID (16 bytes)
    //   MTPR: per param a 32-byte name and a float4
    //   MTTX: per texture a 32-byte name and a 32-byte resource ID (file GUID, chunk GUID)
    // Params stay keyed by name: the runtime maps them to the shader's slots, so a material never depends on its shader
    // at build time.
    public static class MaterialAssets
    {
        public const string ItemKind = "material";
        public const string RawExtension = ".xrm";
        public const string ProcessedExtension = ".xdm";
        public const int LayoutVersion = 1;
        public const int ParamRecordSize = 48;
        public const int TextureRecordSize = 64;

        public static readonly uint RawKind = ChunkTag.Make("XRM ");
        public static readonly uint ProcessedKind = ChunkTag.Make("XDM ");
        public static readonly uint SourceChunk = ChunkTag.Make("SRCT");
        public static readonly uint HeaderChunk = ChunkTag.Make("MTHD");
        public static readonly uint ParamsChunk = ChunkTag.Make("MTPR");
        public static readonly uint TexturesChunk = ChunkTag.Make("MTTX");

        public static void Register(AssetPipeline pipeline)
        {
            pipeline.Register(new Importer());
            pipeline.Register(new Processor());
        }

        public static List<ChunkData> Pack(MaterialAsset material)
        {
            List<KeyValuePair<string, float[]>> parameters = material.Params.Where(p => ShaderAnnotations.ValidName(p.Key)).ToList();
            List<KeyValuePair<string, string>> textures = material.Textures.Where(t => ShaderAnnotations.ValidName(t.Key)).ToList();
            byte[] header = new byte[12 + 16];
            BinaryPrimitives.WriteInt32LittleEndian(header, LayoutVersion);
            BinaryPrimitives.WriteInt32LittleEndian(header.AsSpan(4), parameters.Count);
            BinaryPrimitives.WriteInt32LittleEndian(header.AsSpan(8), textures.Count);
            WriteGuid(material.Shader, header.AsSpan(12, 16));

            byte[] paramBytes = new byte[parameters.Count * ParamRecordSize];
            for (int i = 0; i < parameters.Count; i++)
            {
                Span<byte> record = paramBytes.AsSpan(i * ParamRecordSize, ParamRecordSize);
                Encoding.UTF8.GetBytes(parameters[i].Key, record[..32]);
                for (int c = 0; c < 4; c++)
                    BinaryPrimitives.WriteSingleLittleEndian(record[(32 + c * 4)..], c < parameters[i].Value.Length ? parameters[i].Value[c] : 0.0f);
            }

            byte[] textureBytes = new byte[textures.Count * TextureRecordSize];
            for (int i = 0; i < textures.Count; i++)
            {
                Span<byte> record = textureBytes.AsSpan(i * TextureRecordSize, TextureRecordSize);
                Encoding.UTF8.GetBytes(textures[i].Key, record[..32]);
                string[] parts = textures[i].Value.Split('/');
                WriteGuid(parts[0], record.Slice(32, 16));
                WriteGuid(parts.Length > 1 ? parts[1] : string.Empty, record.Slice(48, 16));
            }

            return new List<ChunkData>
            {
                new(HeaderChunk, header),
                new(ParamsChunk, paramBytes),
                new(TexturesChunk, textureBytes),
            };
        }

        // Reads packed chunks back (tests and tools).
        public static MaterialAsset Unpack(IReadOnlyList<ChunkData> chunks)
        {
            byte[] header = chunks.First(c => c.Type == HeaderChunk).Data;
            int paramCount = BinaryPrimitives.ReadInt32LittleEndian(header.AsSpan(4));
            int textureCount = BinaryPrimitives.ReadInt32LittleEndian(header.AsSpan(8));
            MaterialAsset material = new() { Shader = ReadGuid(header.AsSpan(12, 16)) };
            byte[] paramBytes = chunks.First(c => c.Type == ParamsChunk).Data;
            for (int i = 0; i < paramCount; i++)
            {
                ReadOnlySpan<byte> record = paramBytes.AsSpan(i * ParamRecordSize, ParamRecordSize);
                float[] value = new float[4];
                for (int c = 0; c < 4; c++)
                    value[c] = BinaryPrimitives.ReadSingleLittleEndian(record[(32 + c * 4)..]);
                material.Params[ReadName(record[..32])] = value;
            }
            byte[] textureBytes = chunks.First(c => c.Type == TexturesChunk).Data;
            for (int i = 0; i < textureCount; i++)
            {
                ReadOnlySpan<byte> record = textureBytes.AsSpan(i * TextureRecordSize, TextureRecordSize);
                string file = ReadGuid(record.Slice(32, 16));
                string chunk = ReadGuid(record.Slice(48, 16));
                material.Textures[ReadName(record[..32])] = chunk.Length == 0 ? file : file + "/" + chunk;
            }
            return material;
        }

        private static string ReadName(ReadOnlySpan<byte> bytes)
        {
            int end = bytes.IndexOf((byte)0);
            return Encoding.UTF8.GetString(end < 0 ? bytes : bytes[..end]);
        }

        // GUID text (32 hex) as the engine's ResourceGUID bytes: Hi then Lo, each little-endian.
        public static void WriteGuid(string text, Span<byte> destination)
        {
            destination.Clear();
            if (!AssetMeta.IsValidGuid(text))
                return;
            ulong hi = Convert.ToUInt64(text[..16], 16);
            ulong lo = Convert.ToUInt64(text[16..32], 16);
            BinaryPrimitives.WriteUInt64LittleEndian(destination, hi);
            BinaryPrimitives.WriteUInt64LittleEndian(destination[8..], lo);
        }

        public static string ReadGuid(ReadOnlySpan<byte> source)
        {
            ulong hi = BinaryPrimitives.ReadUInt64LittleEndian(source);
            ulong lo = BinaryPrimitives.ReadUInt64LittleEndian(source[8..]);
            return hi == 0 && lo == 0 ? string.Empty : $"{hi:x16}{lo:x16}";
        }

        private sealed class Importer : IAssetImporter
        {
            public string Id => "material.xcm";
            public int Version => 1;
            public string DataFolder => "Materials";
            public string DataExtension => ProcessedExtension;

            public bool Accepts(string extension) => extension.Equals(MaterialAsset.Extension, StringComparison.OrdinalIgnoreCase);

            public string? HashSource(string sourcePath)
            {
                try
                {
                    MaterialAsset material = MaterialAsset.Parse(File.ReadAllText(sourcePath));
                    return Convert.ToHexStringLower(System.Security.Cryptography.SHA1.HashData(Encoding.UTF8.GetBytes(material.ContentJSON())));
                }
                catch (Exception e) when (e is IOException or JsonException or InvalidDataException)
                {
                    return null;
                }
            }

            public IReadOnlyList<AssetDataFile> Import(string sourcePath, JsonObject settings)
            {
                byte[] text = File.ReadAllBytes(sourcePath);
                MaterialAsset.Parse(Encoding.UTF8.GetString(text).TrimStart('﻿'));
                AssetDataItem item = new(ItemKind, string.Empty, new List<ChunkData> { new(SourceChunk, text) });
                return new[] { new AssetDataFile(RawExtension, RawKind, new List<AssetDataItem> { item }, Store: false) };
            }
        }

        private sealed class Processor : IAssetProcessor
        {
            public string Id => "material.pack";
            public int Version => 1;

            public bool Accepts(string rawExtension) => rawExtension.Equals(RawExtension, StringComparison.OrdinalIgnoreCase);

            public AssetDataFile Process(AssetDataFile raw, JsonObject settings)
            {
                List<AssetDataItem> items = new();
                List<string> tags = new();
                foreach (AssetDataItem item in raw.Items)
                {
                    MaterialAsset material = MaterialAsset.Parse(Encoding.UTF8.GetString(item.Chunks.First(c => c.Type == SourceChunk).Data).TrimStart('﻿'));
                    items.Add(new AssetDataItem(item.Kind, item.Path, Pack(material)));
                    tags.Add($"@material:{material.Textures.Count}textures");
                    if (string.IsNullOrEmpty(material.Shader))
                        tags.Add("@material:noshader");
                }
                return new AssetDataFile(ProcessedExtension, ProcessedKind, items, Tags: tags);
            }
        }
    }
}
