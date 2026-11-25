// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.

using System.Text.Json;

namespace xg.editor
{
    public sealed record AssetMetaMigrationResult(int Migrated, int Unreadable, List<string> Problems);

    // Converts ".xgm" meta files under a folder to the current ".xmd" format. The new file is written before the old one
    // is deleted, so an interrupted run can be repeated. A file that can't be parsed moves unchanged and stays reportable.
    public static class AssetMetaMigration
    {
        public static AssetMetaMigrationResult Run(string root)
        {
            int migrated = 0;
            int unreadable = 0;
            List<string> problems = new();
            if (!Directory.Exists(root))
                return new AssetMetaMigrationResult(0, 0, problems);

            EnumerationOptions options = new() { RecurseSubdirectories = true, MatchCasing = MatchCasing.CaseInsensitive, IgnoreInaccessible = true };
            foreach (string legacy in Directory.GetFiles(root, "*" + AssetMeta.LegacyExtension, options))
            {
                string target = legacy[..^AssetMeta.LegacyExtension.Length] + AssetMeta.Extension;
                try
                {
                    if (File.Exists(target))
                    {
                        if (SameGuid(legacy, target))
                        {
                            File.Delete(legacy);
                            migrated++;
                        }
                        else
                        {
                            problems.Add($"{legacy} was left in place: {target} already exists with a different GUID");
                        }
                        continue;
                    }

                    AssetMeta meta;
                    try
                    {
                        meta = AssetMeta.Read(legacy);
                    }
                    catch (JsonException)
                    {
                        File.Move(legacy, target);
                        unreadable++;
                        continue;
                    }
                    meta.Write(target);
                    File.Delete(legacy);
                    migrated++;
                }
                catch (Exception e) when (e is IOException or UnauthorizedAccessException)
                {
                    problems.Add($"{legacy}: {e.Message}");
                }
            }
            return new AssetMetaMigrationResult(migrated, unreadable, problems);
        }

        // Moves the ".xmd" of every composition (AssetMetaStore.IsEmbedded) into the file as its "meta" block and deletes
        // the sidecar; "version" goes to at least 2. The file is written before the sidecar is deleted. Returns each
        // asset handled, with null or the reason it failed (the sidecar is then kept).
        public static List<(string Asset, string? Problem)> EmbedSidecars(string root)
        {
            List<(string, string?)> results = new();
            if (!Directory.Exists(root))
                return results;
            EnumerationOptions options = new() { RecurseSubdirectories = true, MatchCasing = MatchCasing.CaseInsensitive, IgnoreInaccessible = true };
            foreach (string sidecar in Directory.GetFiles(root, "*" + AssetMeta.Extension, options))
            {
                string asset = AssetMeta.AssetPathOf(sidecar);
                if (!AssetMetaStore.IsEmbedded(asset) || !File.Exists(asset))
                    continue;
                try
                {
                    AssetMeta meta = AssetMeta.Read(sidecar);
                    if (System.Text.Json.Nodes.JsonNode.Parse(File.ReadAllText(asset)) is not System.Text.Json.Nodes.JsonObject document)
                        throw new JsonException("not a JSON object");
                    if (document[AssetMetaStore.EmbeddedKey] is System.Text.Json.Nodes.JsonObject existing && AssetMeta.IsValidGuid(AssetMeta.FromJSON(existing).Guid))
                    {
                        File.Delete(sidecar);
                        results.Add((asset, null));
                        continue;
                    }
                    if (document["version"] is System.Text.Json.Nodes.JsonValue version && version.TryGetValue(out int number) && number < 2)
                        document["version"] = 2;
                    System.Text.Json.Nodes.JsonObject updated = AssetMetaStore.WithBlock(document, meta.ToEmbeddedJSON());
                    JSONFile.WriteText(asset, updated.ToJsonString(JSONFile.Options) + "\r\n");
                    File.Delete(sidecar);
                    results.Add((asset, null));
                }
                catch (Exception e) when (e is IOException or JsonException or UnauthorizedAccessException)
                {
                    results.Add((asset, e.Message));
                }
            }
            return results;
        }

        private static bool SameGuid(string a, string b)
        {
            try
            {
                return string.Equals(AssetMeta.Read(a).Guid, AssetMeta.Read(b).Guid, StringComparison.OrdinalIgnoreCase);
            }
            catch (JsonException)
            {
                return false;
            }
        }
    }
}
