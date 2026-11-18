// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.

using System.Text.Json;
using System.Text.Json.Nodes;
using System.Text.Json.Serialization;

namespace xg.editor
{
    // The meta file next to every asset file ("Sword.png.xgm"; folders have none): the asset's GUID, which runtime
    // packaging and loading use, its user tags and its importer settings. Pretty-printed JSON; keys the editor doesn't
    // know are kept when the file is rewritten.
    public sealed class AssetMeta
    {
        public const int CurrentVersion = 1;
        public const string Extension = ".xgm";

        [JsonPropertyName("version")]
        [JsonPropertyOrder(0)]
        public int Version { get; set; } = CurrentVersion;

        [JsonPropertyName("guid")]
        [JsonPropertyOrder(1)]
        public string Guid { get; set; } = string.Empty;

        [JsonPropertyName("tags")]
        [JsonPropertyOrder(2)]
        public List<string> Tags { get; set; } = new();

        [JsonPropertyName("importer")]
        [JsonPropertyOrder(3)]
        public JsonObject Importer { get; set; } = new();

        [JsonExtensionData]
        public Dictionary<string, JsonElement>? Extra { get; set; }

        public static AssetMeta Create() => new() { Guid = NewGuid() };

        public static string NewGuid() => System.Guid.NewGuid().ToString("N");

        public static bool IsValidGuid(string? guid) =>
            guid is { Length: 32 } && guid.All(char.IsAsciiHexDigit);

        public static string PathFor(string assetPath) => assetPath + Extension;

        public static bool IsMetaPath(string path) => path.EndsWith(Extension, StringComparison.OrdinalIgnoreCase);

        public static string AssetPathOf(string metaPath) => metaPath[..^Extension.Length];

        public static AssetMeta Parse(string json)
        {
            AssetMeta meta = JSONFile.Deserialize<AssetMeta>(json) ?? throw new JsonException("Empty meta file");
            meta.Tags ??= new List<string>();
            meta.Importer ??= new JsonObject();
            meta.Guid ??= string.Empty;
            return meta;
        }

        public static AssetMeta Read(string path) => Parse(File.ReadAllText(path));

        public string ToJSON()
        {
            Tags = Tags.Where(t => t.Length > 0).Distinct(StringComparer.OrdinalIgnoreCase).Order(StringComparer.Ordinal).ToList();
            return JSONFile.Serialize(this);
        }

        // Returns true when the file was written (its text changed).
        public bool Write(string path) => JSONFile.WriteText(path, ToJSON());
    }
}
