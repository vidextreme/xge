// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.

using System.Text.Json.Serialization;

namespace xg.editor
{
    // One item inside a source asset, such as a submesh, stored in the asset's meta file. Its GUID is the chunk GUID in
    // "<file-guid>/<chunk-guid>" references. It is random, assigned once and kept; the name path finds it again on reimport.
    public sealed class AssetItem
    {
        [JsonPropertyName("kind")]
        [JsonPropertyOrder(0)]
        public string Kind { get; set; } = string.Empty;

        [JsonPropertyName("path")]
        [JsonPropertyOrder(1)]
        public string Path { get; set; } = string.Empty;

        // Position among items with the same kind and path. Only a fallback: duplicate names are reported on import.
        [JsonPropertyName("occurrence")]
        [JsonPropertyOrder(2)]
        [JsonIgnore(Condition = JsonIgnoreCondition.WhenWritingDefault)]
        public int Occurrence { get; set; }

        [JsonPropertyName("guid")]
        [JsonPropertyOrder(3)]
        public string Guid { get; set; } = string.Empty;

        // The last import didn't produce this item. The entry stays so the item gets its GUID back if it returns.
        [JsonPropertyName("missing")]
        [JsonPropertyOrder(4)]
        [JsonIgnore(Condition = JsonIgnoreCondition.WhenWritingDefault)]
        public bool Missing { get; set; }
    }

    // An item produced by an importer, in source order.
    public readonly record struct ImportedAssetItem(string Kind, string Path);

    public readonly record struct AssignedAssetItem(string Kind, string Path, int Occurrence, string Guid);

    public sealed record AssetItemUpdate(List<AssetItem> Items, List<AssignedAssetItem> Assigned, List<string> Warnings, bool Changed);

    // Matches an import result with a meta's stored items. Pure: the caller writes the meta.
    public static class AssetItemTable
    {
        public static bool IsValidChunkGuid(string? guid) => AssetMeta.IsValidGuid(guid) && guid!.Any(c => c != '0');

        public static List<AssetItem> Sorted(IEnumerable<AssetItem> items) =>
            items.OrderBy(i => i.Kind, StringComparer.Ordinal).ThenBy(i => i.Path, StringComparer.Ordinal).ThenBy(i => i.Occurrence).ToList();

        public static AssetItemUpdate Update(IEnumerable<AssetItem> stored, IEnumerable<ImportedAssetItem> imported, Func<string>? newGuid = null)
        {
            newGuid ??= AssetMeta.NewGuid;
            List<AssetItem> before = Sorted(stored);
            List<string> warnings = new();
            Dictionary<(string Kind, string Path, int Occurrence), AssetItem> items = new();
            HashSet<string> guids = new(StringComparer.OrdinalIgnoreCase);

            string UniqueGuid()
            {
                string guid;
                do
                    guid = newGuid().ToLowerInvariant();
                while (!IsValidChunkGuid(guid) || !guids.Add(guid));
                return guid;
            }

            foreach (AssetItem item in before)
            {
                string label = Label(item.Kind, item.Path, item.Occurrence);
                if (!items.TryAdd((item.Kind, item.Path, item.Occurrence), new AssetItem { Kind = item.Kind, Path = item.Path, Occurrence = item.Occurrence, Guid = item.Guid, Missing = true }))
                {
                    warnings.Add($"Dropped a repeated entry for {label}");
                    continue;
                }
                AssetItem copy = items[(item.Kind, item.Path, item.Occurrence)];
                if (IsValidChunkGuid(item.Guid) && guids.Add(item.Guid))
                {
                    copy.Guid = item.Guid.ToLowerInvariant();
                    continue;
                }
                copy.Guid = string.Empty;
                warnings.Add($"{label} had an invalid or repeated GUID '{item.Guid}'; it gets a new one, so references to the old one need relinking");
            }
            foreach (AssetItem item in items.Values.Where(i => i.Guid.Length == 0))
                item.Guid = UniqueGuid();

            List<AssignedAssetItem> assigned = new();
            Dictionary<(string Kind, string Path), int> counts = new();
            foreach (ImportedAssetItem source in imported)
            {
                if (string.IsNullOrEmpty(source.Kind))
                {
                    warnings.Add($"Ignored an imported item without a kind ('{source.Path}')");
                    continue;
                }
                string path = source.Path ?? string.Empty;
                counts.TryGetValue((source.Kind, path), out int occurrence);
                counts[(source.Kind, path)] = occurrence + 1;
                if (!items.TryGetValue((source.Kind, path, occurrence), out AssetItem? item))
                    items[(source.Kind, path, occurrence)] = item = new AssetItem { Kind = source.Kind, Path = path, Occurrence = occurrence, Guid = UniqueGuid() };
                item.Missing = false;
                assigned.Add(new AssignedAssetItem(item.Kind, item.Path, item.Occurrence, item.Guid));
            }
            foreach (((string kind, string path), int count) in counts.Where(c => c.Value > 1).OrderBy(c => c.Key.Kind, StringComparer.Ordinal).ThenBy(c => c.Key.Path, StringComparer.Ordinal))
                warnings.Add($"{count} items share the name {Label(kind, path, 0)}; they are matched by order, so reordering them in the source swaps their GUIDs. Give them unique names");

            List<AssetItem> after = Sorted(items.Values);
            return new AssetItemUpdate(after, assigned, warnings, !Same(before, after));
        }

        private static string Label(string kind, string path, int occurrence) =>
            occurrence == 0 ? $"{kind} '{path}'" : $"{kind} '{path}' #{occurrence}";

        private static bool Same(List<AssetItem> a, List<AssetItem> b)
        {
            if (a.Count != b.Count)
                return false;
            for (int i = 0; i < a.Count; i++)
            {
                if (a[i].Kind != b[i].Kind || a[i].Path != b[i].Path || a[i].Occurrence != b[i].Occurrence || a[i].Guid != b[i].Guid || a[i].Missing != b[i].Missing)
                    return false;
            }
            return true;
        }
    }
}
