// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.

using System.Text.Json;
using System.Text.Json.Nodes;
using System.Text.Json.Serialization;

namespace xg.editor
{
    // The meta file next to every asset file ("Sword.png.xmd"; folders have none): the asset's GUID, which runtime
    // packaging and loading use, its user tags, its import and process settings and the table of items inside it.
    // Pretty-printed JSON; keys the editor doesn't know are kept when the file is rewritten.
    public sealed class AssetMeta
    {
        public const int CurrentVersion = 2;
        public const string Extension = ".xmd";
        public const string LegacyExtension = ".xgm";

        [JsonPropertyName("version")]
        [JsonPropertyOrder(0)]
        public int Version { get; set; }

        [JsonPropertyName("guid")]
        [JsonPropertyOrder(1)]
        public string Guid { get; set; } = string.Empty;

        [JsonPropertyName("tags")]
        [JsonPropertyOrder(2)]
        public List<string> Tags { get; set; } = new();

        [JsonPropertyName("importer")]
        [JsonPropertyOrder(3)]
        public JsonObject Importer { get; set; } = new();

        [JsonPropertyName("items")]
        [JsonPropertyOrder(4)]
        public List<AssetItem> Items { get; set; } = new();

        // Settings used to generate the raw data.
        [JsonIgnore]
        public JsonObject ImportSettings => Section("import");

        // Settings used to generate the processed data from the raw data.
        [JsonIgnore]
        public JsonObject ProcessSettings => Section("process");

        [JsonExtensionData]
        public Dictionary<string, JsonElement>? Extra { get; set; }

        public static AssetMeta Create() => new() { Version = CurrentVersion, Guid = NewGuid() };

        public static string NewGuid() => System.Guid.NewGuid().ToString("N");

        public static bool IsValidGuid(string? guid) =>
            guid is { Length: 32 } && guid.All(char.IsAsciiHexDigit);

        public static string PathFor(string assetPath) => assetPath + Extension;

        public static bool IsMetaPath(string path) => path.EndsWith(Extension, StringComparison.OrdinalIgnoreCase);

        public static bool IsLegacyMetaPath(string path) => path.EndsWith(LegacyExtension, StringComparison.OrdinalIgnoreCase);

        public static string AssetPathOf(string metaPath) => metaPath[..^Extension.Length];

        public static AssetMeta Parse(string json)
        {
            AssetMeta meta = JSONFile.Deserialize<AssetMeta>(json) ?? throw new JsonException("Empty meta file");
            meta.Tags ??= new List<string>();
            meta.Importer ??= new JsonObject();
            meta.Items ??= new List<AssetItem>();
            meta.Guid ??= string.Empty;
            meta.Upgrade();
            return meta;
        }

        public static AssetMeta Read(string path) => Parse(File.ReadAllText(path));

        // An embedded meta block (AssetMetaStore): the same fields as a meta file, without version or importer.
        public static AssetMeta FromJSON(JsonObject block) => Parse(block.ToJsonString());

        // The embedded form: guid, tags, items (when there are any) and the extra keys, without version or importer.
        public JsonObject ToEmbeddedJSON()
        {
            JsonObject full = JsonNode.Parse(ToJSON())!.AsObject();
            full.Remove("version");
            full.Remove("importer");
            if (full["items"] is JsonArray { Count: 0 })
                full.Remove("items");
            return full;
        }

        public string ToJSON()
        {
            Upgrade();
            Tags = Tags.Where(t => t.Length > 0).Distinct(StringComparer.OrdinalIgnoreCase).Order(StringComparer.Ordinal).ToList();
            Items = AssetItemTable.Sorted(Items);
            return JSONFile.Serialize(this);
        }

        // Returns true when the file was written (its text changed).
        public bool Write(string path) => JSONFile.WriteText(path, ToJSON());

        private JsonObject Section(string name)
        {
            if (Importer[name] is JsonObject section)
                return section;
            section = new JsonObject();
            Importer[name] = section;
            return section;
        }

        // Version 1 kept import settings directly in "importer".
        private void Upgrade()
        {
            if (Version < 2 && Importer.Count > 0)
            {
                List<KeyValuePair<string, JsonNode?>> settings = Importer.ToList();
                Importer.Clear();
                JsonObject import = new();
                foreach ((string key, JsonNode? value) in settings)
                    import[key] = value;
                Importer["import"] = import;
            }
            _ = Section("import");
            _ = Section("process");
            if (Version < CurrentVersion)
                Version = CurrentVersion;
        }
    }
}
