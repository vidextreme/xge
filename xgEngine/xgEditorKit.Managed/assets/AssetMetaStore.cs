// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.

using System.Text.Json;
using System.Text.Json.Nodes;

namespace xg.editor
{
    // Where an asset's meta lives. Imported sources (".png", ".fbx", ".hlsl") keep it in the ".xmd" next to them;
    // compositions the editor authors (".xc*" JSON files) embed it as their top-level "meta" object (guid, tags,
    // optional items, names and any other keys), so they're a single file.
    public static class AssetMetaStore
    {
        public const string EmbeddedKey = "meta";

        public static bool IsEmbedded(string path) =>
            Path.GetExtension(path) is { Length: > 3 } extension && extension.StartsWith(".xc", StringComparison.OrdinalIgnoreCase);

        // The asset's meta (full path of the asset file). Throws IOException/JsonException when it can't be read.
        public static AssetMeta Read(string assetFull)
        {
            if (!IsEmbedded(assetFull))
                return AssetMeta.Read(AssetMeta.PathFor(assetFull));
            JsonObject root = ReadRoot(assetFull);
            if (root[EmbeddedKey] is not JsonObject block)
                throw new JsonException($"{Path.GetFileName(assetFull)} has no \"{EmbeddedKey}\" object");
            return AssetMeta.FromJSON(block);
        }

        // True when the asset has a readable meta (an existing .xmd, or an embedded block with a GUID).
        public static bool TryRead(string assetFull, out AssetMeta? meta)
        {
            meta = null;
            try
            {
                if (!IsEmbedded(assetFull) && !File.Exists(AssetMeta.PathFor(assetFull)))
                    return false;
                meta = Read(assetFull);
                return true;
            }
            catch (Exception e) when (e is IOException or JsonException or UnauthorizedAccessException)
            {
                return false;
            }
        }

        // Writes the meta; returns true when a file changed. An embedded block replaces "meta" in place (it's added
        // after "version" when missing) and keeps every other key and its order.
        public static bool Write(string assetFull, AssetMeta meta)
        {
            if (!IsEmbedded(assetFull))
                return meta.Write(AssetMeta.PathFor(assetFull));
            JsonObject root = ReadRoot(assetFull);
            return JSONFile.WriteText(assetFull, WithBlock(root, meta.ToEmbeddedJSON()).ToJsonString(JSONFile.Options) + "\r\n");
        }

        // The file's JSON with this meta block (pure; tests and migration).
        public static JsonObject WithBlock(JsonObject root, JsonObject block)
        {
            JsonObject result = new();
            bool placed = false;
            foreach ((string key, JsonNode? value) in root.ToList())
            {
                root.Remove(key);
                if (key == EmbeddedKey)
                {
                    result[EmbeddedKey] = block;
                    placed = true;
                    continue;
                }
                result[key] = value;
                if (key == "version" && !placed)
                {
                    result[EmbeddedKey] = block;
                    placed = true;
                }
            }
            if (!placed)
            {
                JsonObject front = new() { [EmbeddedKey] = block };
                foreach ((string key, JsonNode? value) in result.ToList())
                {
                    result.Remove(key);
                    front[key] = value;
                }
                return front;
            }
            return result;
        }

        private static JsonObject ReadRoot(string assetFull)
        {
            JsonNode? node = JsonNode.Parse(File.ReadAllText(assetFull), documentOptions: new JsonDocumentOptions { CommentHandling = JsonCommentHandling.Skip, AllowTrailingCommas = true });
            return node as JsonObject ?? throw new JsonException($"{Path.GetFileName(assetFull)} isn't a JSON object");
        }
    }
}
