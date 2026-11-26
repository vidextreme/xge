// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.

using System.Text;
using System.Text.Json;
using System.Text.Json.Serialization;
using xg.editor;
using xg.math;
using Debug = xg.Debug;

namespace xgEditor.CoreCLR
{
    public readonly record struct TagCount(string Tag, int Count);

    public readonly record struct TagChip(string Tag, bool Or, bool ReadOnly = false)
    {
        public bool IsAttribute => Tag.StartsWith('@');
        public override string ToString() => Or ? "+" + Tag : Tag;
    }

    // A tag filter: plain chips must all match (AND); "+" chips match on their own (OR):
    // "enemy forest +boss" = (enemy and forest) or boss.
    public sealed class TagQuery
    {
        public List<TagChip> Chips { get; } = new();

        public bool IsEmpty => Chips.Count == 0;

        public static TagQuery Parse(string text)
        {
            TagQuery query = new();
            foreach (string part in text.Split(' ', StringSplitOptions.RemoveEmptyEntries | StringSplitOptions.TrimEntries))
            {
                bool or = part.StartsWith('+');
                string tag = AssetTags.NormalizeTag(or ? part[1..] : part);
                if (tag.Length > 0)
                    query.Chips.Add(new TagChip(tag, or));
            }
            return query;
        }

        public override string ToString() => string.Join(' ', Chips);

        // Paths matching a query of user tags only, from the tag index: (all plain chips) or (any "+" chip). Null when the
        // query has attribute chips (@type, @ext), which need Matches per entry.
        public HashSet<string>? MatchingPaths(AssetTags tags)
        {
            if (Chips.Count == 0 || Chips.Any(c => c.IsAttribute))
                return null;
            HashSet<string>? all = null;
            HashSet<string> result = new(StringComparer.OrdinalIgnoreCase);
            foreach (TagChip chip in Chips)
            {
                if (chip.Or)
                    result.UnionWith(tags.PathsWithTag(chip.Tag));
                else if (all is null)
                    all = new HashSet<string>(tags.PathsWithTag(chip.Tag), StringComparer.OrdinalIgnoreCase);
                else
                    all.IntersectWith(tags.PathsWithTag(chip.Tag));
            }
            if (all is not null)
                result.UnionWith(all);
            return result;
        }

        public bool Matches(AssetEntry entry, AssetTags tags)
        {
            if (Chips.Count == 0)
                return true;
            bool anyAnd = false;
            bool allAnd = true;
            bool anyOr = false;
            foreach (TagChip chip in Chips)
            {
                bool has = tags.HasTag(entry, chip.Tag);
                if (chip.Or)
                    anyOr |= has;
                else
                {
                    anyAnd = true;
                    allAnd &= has;
                }
            }
            return (anyAnd && allAnd) || anyOr;
        }

        // "Match tree and @type:mesh" / "or acorncity" lines for the hover tooltip.
        public string Describe()
        {
            StringBuilder text = new();
            List<string> ands = Chips.Where(c => !c.Or).Select(c => c.Tag).ToList();
            List<string> ors = Chips.Where(c => c.Or).Select(c => c.Tag).ToList();
            if (ands.Count > 0)
                text.Append("Match ").Append(string.Join(" and ", ands));
            foreach (string tag in ors)
            {
                if (text.Length > 0)
                    text.Append('\n').Append("or ");
                else
                    text.Append("Match ");
                text.Append(tag);
            }
            return text.ToString();
        }
    }

    // User tags of the project's assets, stored in each asset's meta file and indexed by the asset database, plus
    // built-in attribute tags computed from the index and never stored: @type:<kind> and @ext:<extension>. Optional
    // per-tag settings (description, colour) live in ProjectSettings/Tags.json.
    public sealed class AssetTags
    {
        private sealed class TagInfo
        {
            [JsonPropertyName("description")]
            [JsonIgnore(Condition = JsonIgnoreCondition.WhenWritingDefault)]
            public string Description { get; set; } = string.Empty;

            [JsonPropertyName("color")]
            [JsonIgnore(Condition = JsonIgnoreCondition.WhenWritingNull)]
            public string? Color { get; set; }
        }

        private sealed class SettingsFile
        {
            [JsonPropertyName("tags")]
            public SortedDictionary<string, TagInfo> Tags { get; set; } = new(StringComparer.Ordinal);
        }

        private sealed class LegacyTagFile
        {
            [JsonPropertyName("tags")]
            public Dictionary<string, List<string>> Tags { get; set; } = new();
        }

        private readonly AssetDatabase _db;
        private readonly string _settingsPath;
        private readonly SettingsFile _settings = new();
        private List<TagCount>? _userTags;
        private int _userVersion = -1;
        private List<TagCount>? _attributeTags;
        private long _attributeVersion = -1;

        internal AssetTags(AssetDatabase db)
        {
            _db = db;
            _settingsPath = Path.Combine(db.ProjectRoot, "ProjectSettings", "Tags.json");
            if (!File.Exists(_settingsPath))
                return;
            try
            {
                SettingsFile? file = JSONFile.Read<SettingsFile>(_settingsPath);
                if (file is not null)
                    foreach (KeyValuePair<string, TagInfo> pair in file.Tags)
                        _settings.Tags[NormalizeTag(pair.Key)] = pair.Value;
            }
            catch (Exception e) when (e is IOException or JsonException or UnauthorizedAccessException)
            {
                Debug.LogError($"[Navigator] Could not read {_settingsPath}: {e.Message}");
            }
        }

        public string SettingsPath => _settingsPath;

        public static string NormalizeTag(string tag) => tag.Trim().ToLowerInvariant().Replace(' ', '_');

        public IReadOnlyList<string> Get(string relPath) => _db.TagsOf(relPath);

        // Read-only tags from the asset's last build ("@texture:bc1").
        public IReadOnlyList<string> ImportTags(string relPath) => _db.ImportTagsOf(relPath);

        public bool IsUntagged(string relPath) => Get(relPath).Count == 0;

        public void Add(string relPath, string tag)
        {
            tag = NormalizeTag(tag);
            if (tag.Length == 0 || tag.StartsWith('@'))
                return;
            IReadOnlyList<string> tags = Get(relPath);
            if (tags.Contains(tag, StringComparer.OrdinalIgnoreCase))
                return;
            _db.SetTags(relPath, tags.Append(tag));
        }

        public void Remove(string relPath, string tag)
        {
            IReadOnlyList<string> tags = Get(relPath);
            if (tags.Contains(tag, StringComparer.OrdinalIgnoreCase))
                _db.SetTags(relPath, tags.Where(t => !string.Equals(t, tag, StringComparison.OrdinalIgnoreCase)));
        }

        public bool HasTag(AssetEntry entry, string tag)
        {
            if (tag.StartsWith('@'))
            {
                int colon = tag.IndexOf(':');
                if (colon < 0)
                    return false;
                string group = tag[1..colon];
                string value = tag[(colon + 1)..];
                return group switch
                {
                    "type" => !entry.IsFolder && string.Equals(TypeName(entry.Kind), value, StringComparison.OrdinalIgnoreCase),
                    "ext" => string.Equals(entry.Extension, value, StringComparison.OrdinalIgnoreCase),
                    _ => !entry.IsFolder && ImportTags(entry.RelPath).Contains(tag, StringComparer.OrdinalIgnoreCase),
                };
            }
            IReadOnlyList<string> tags = Get(entry.RelPath);
            for (int i = 0; i < tags.Count; i++)
                if (string.Equals(tags[i], tag, StringComparison.OrdinalIgnoreCase))
                    return true;
            return false;
        }

        // Paths of the assets with this user tag (from the tag index).
        public IEnumerable<string> PathsWithTag(string tag)
        {
            foreach (string guid in _db.GuidsWithTag(tag))
                if (_db.PathOf(guid) is string path)
                    yield return path;
        }

        public static string TypeName(AssetKind kind) => kind.ToString().ToLowerInvariant();

        public static bool TryParseType(string value, out AssetKind kind)
        {
            if (string.Equals(value, "object", StringComparison.OrdinalIgnoreCase))
            {
                kind = AssetKind.Archetype;
                return true;
            }
            foreach (AssetKind candidate in Enum.GetValues<AssetKind>())
            {
                if (candidate != AssetKind.Folder && string.Equals(TypeName(candidate), value, StringComparison.OrdinalIgnoreCase))
                {
                    kind = candidate;
                    return true;
                }
            }
            kind = AssetKind.Other;
            return false;
        }

        // User tags with the number of assets carrying each, by count then name.
        public IReadOnlyList<TagCount> UserTags()
        {
            if (_userTags is not null && _userVersion == _db.Version)
                return _userTags;
            _userTags = _db.TagCounts.OrderByDescending(t => t.Count).ThenBy(t => t.Tag, StringComparer.OrdinalIgnoreCase).ToList();
            _userVersion = _db.Version;
            return _userTags;
        }

        // @type and @ext tags of the files in the index, then the import tags (@texture:...), by group then value.
        public IReadOnlyList<TagCount> AttributeTags(AssetIndex index)
        {
            long version = ((long)index.Version << 32) | (uint)_db.ImportTagVersion;
            if (_attributeTags is not null && _attributeVersion == version)
                return _attributeTags;
            Dictionary<string, int> counts = new(StringComparer.OrdinalIgnoreCase);
            foreach (AssetEntry entry in index.Entries)
            {
                if (entry.IsFolder)
                    continue;
                string type = "@type:" + TypeName(entry.Kind);
                counts[type] = counts.TryGetValue(type, out int n) ? n + 1 : 1;
                if (entry.Extension.Length > 0)
                {
                    string ext = "@ext:" + entry.Extension;
                    counts[ext] = counts.TryGetValue(ext, out int m) ? m + 1 : 1;
                }
            }
            foreach (TagCount tag in _db.ImportTagCounts)
                counts[tag.Tag] = tag.Count;
            _attributeTags = counts.Select(p => new TagCount(p.Key, p.Value))
                .OrderBy(t => t.Tag.StartsWith("@type:") ? 0 : t.Tag.StartsWith("@ext:") ? 1 : 2).ThenBy(t => t.Tag, StringComparer.OrdinalIgnoreCase).ToList();
            _attributeVersion = version;
            return _attributeTags;
        }

        public string Description(string tag) => _settings.Tags.TryGetValue(tag, out TagInfo? info) ? info.Description : string.Empty;

        public void SetDescription(string tag, string description)
        {
            description = description.Trim();
            if (Description(tag) == description)
                return;
            if (!_settings.Tags.TryGetValue(tag, out TagInfo? info))
                _settings.Tags[tag] = info = new TagInfo();
            info.Description = description;
            if (info.Description.Length == 0 && info.Color is null)
                _settings.Tags.Remove(tag);
            SaveSettings();
        }

        // The tag's colour from Tags.json ("#RRGGBB"), if any.
        public Color32? Color(string tag)
        {
            if (!_settings.Tags.TryGetValue(tag, out TagInfo? info) || info.Color is not { Length: 7 } hex || hex[0] != '#'
                || !uint.TryParse(hex.AsSpan(1), System.Globalization.NumberStyles.HexNumber, null, out uint rgb))
                return null;
            return new Color32((byte)(rgb >> 16), (byte)(rgb >> 8), (byte)rgb, 255);
        }

        // Renames a user tag on every asset (and its settings). Returns the number of assets changed.
        public int Rename(string from, string to)
        {
            to = NormalizeTag(to);
            if (to.Length == 0 || to.StartsWith('@') || string.Equals(from, to, StringComparison.Ordinal))
                return 0;
            int count = _db.RenameTag(from, to);
            if (_settings.Tags.Remove(from, out TagInfo? info))
            {
                _settings.Tags.TryAdd(to, info);
                SaveSettings();
            }
            Debug.Log($"[Assets] Renamed tag '{from}' to '{to}' on {count} asset(s)");
            return count;
        }

        // Removes a user tag from every asset (and its settings). Returns the number of assets changed.
        public int Delete(string tag)
        {
            int count = _db.DeleteTag(tag);
            if (_settings.Tags.Remove(tag))
                SaveSettings();
            Debug.Log($"[Assets] Deleted tag '{tag}' from {count} asset(s)");
            return count;
        }

        // Moves the tags of the old ProjectSettings/AssetTags.json (path -> tags) into the meta files once, then renames
        // the old file to AssetTags.json.migrated.
        internal void Migrate()
        {
            string legacy = Path.Combine(_db.ProjectRoot, "ProjectSettings", "AssetTags.json");
            if (!File.Exists(legacy))
                return;
            try
            {
                LegacyTagFile? file = JSONFile.Read<LegacyTagFile>(legacy);
                int moved = 0;
                int missing = 0;
                if (file is not null)
                {
                    foreach (KeyValuePair<string, List<string>> pair in file.Tags)
                    {
                        string path = AssetIndex.Normalize(pair.Key);
                        if (_db.GuidOf(path) is null)
                        {
                            missing++;
                            continue;
                        }
                        if (_db.SetTags(path, Get(path).Concat(pair.Value)))
                            moved++;
                    }
                }
                File.Move(legacy, legacy + ".migrated", true);
                Debug.Log($"[Assets] Moved the tags of {moved} asset(s) from AssetTags.json into their meta files"
                    + (missing > 0 ? $" ({missing} path(s) no longer exist)" : string.Empty) + "; the old file is AssetTags.json.migrated");
            }
            catch (Exception e) when (e is IOException or JsonException or UnauthorizedAccessException)
            {
                Debug.LogError($"[Assets] Could not migrate {legacy}: {e.Message}");
            }
        }

        private void SaveSettings()
        {
            try
            {
                JSONFile.Write(_settingsPath, _settings);
            }
            catch (Exception e) when (e is IOException or UnauthorizedAccessException)
            {
                Debug.LogError($"[Navigator] Could not save {_settingsPath}: {e.Message}");
            }
        }
    }
}
