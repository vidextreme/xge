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
