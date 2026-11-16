// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.

using System.Text;
using System.Text.Json;
using System.Text.Json.Serialization;
using Debug = xg.Debug;

namespace xgEditor.CoreCLR
{
    public readonly record struct TagCount(string Tag, int Count);

    public readonly record struct TagChip(string Tag, bool Or)
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

    // User tags of the project's assets, kept in ProjectSettings/AssetTags.json (path -> tags), plus built-in attribute
    // tags computed from the index and never stored: @type:<kind> and @ext:<extension>.
    public sealed class AssetTags
    {
        private sealed class TagFile
        {
            [JsonPropertyName("tags")]
            public Dictionary<string, List<string>> Tags { get; set; } = new();
        }

        private static readonly JsonSerializerOptions _options = new() { WriteIndented = true };
        private static readonly string[] _fakeTags = { "prototype", "wip", "hero", "enemy", "environment", "ui", "props", "night", "boss", "forest" };

        private readonly Dictionary<string, List<string>> _tags = new(StringComparer.OrdinalIgnoreCase);
        private readonly string _path;
        private List<TagCount>? _userTags;
        private List<TagCount>? _attributeTags;
        private int _attributeVersion = -1;

        private AssetTags(string path) => _path = path;

        public string FilePath => _path;

        public static AssetTags Load(string projectRoot, AssetIndex index)
        {
            AssetTags tags = new(Path.Combine(projectRoot, "ProjectSettings", "AssetTags.json"));
            if (File.Exists(tags._path))
            {
                try
                {
                    TagFile? file = JsonSerializer.Deserialize<TagFile>(File.ReadAllText(tags._path), _options);
                    if (file is not null)
                        foreach (KeyValuePair<string, List<string>> pair in file.Tags)
                            tags._tags[AssetIndex.Normalize(pair.Key)] = pair.Value.Select(NormalizeTag).Where(t => t.Length > 0 && !t.StartsWith('@')).Distinct().ToList();
                }
                catch (Exception e) when (e is IOException or JsonException or UnauthorizedAccessException)
                {
                    Debug.LogError($"[Navigator] Could not read {tags._path}: {e.Message}");
                }
            }
            else
            {
                tags.Seed(index);
                tags.Save();
            }
            return tags;
        }

        public static string NormalizeTag(string tag) => tag.Trim().ToLowerInvariant().Replace(' ', '_');

        public IReadOnlyList<string> Get(string relPath) =>
            _tags.TryGetValue(relPath, out List<string>? list) ? list : Array.Empty<string>();

        public bool IsUntagged(string relPath) => Get(relPath).Count == 0;

        public void Add(string relPath, string tag)
        {
            tag = NormalizeTag(tag);
            if (tag.Length == 0 || tag.StartsWith('@'))
                return;
            if (!_tags.TryGetValue(relPath, out List<string>? list))
                _tags[relPath] = list = new List<string>();
            if (list.Contains(tag))
                return;
            list.Add(tag);
            _userTags = null;
            Save();
        }

        public void Remove(string relPath, string tag)
        {
            if (!_tags.TryGetValue(relPath, out List<string>? list) || !list.Remove(tag))
                return;
            if (list.Count == 0)
                _tags.Remove(relPath);
            _userTags = null;
            Save();
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
                    _ => false,
                };
            }
            IReadOnlyList<string> tags = Get(entry.RelPath);
            for (int i = 0; i < tags.Count; i++)
                if (string.Equals(tags[i], tag, StringComparison.OrdinalIgnoreCase))
                    return true;
            return false;
        }

        public static string TypeName(AssetKind kind) => kind.ToString().ToLowerInvariant();

        public static bool TryParseType(string value, out AssetKind kind)
        {
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
            if (_userTags is not null)
                return _userTags;
            Dictionary<string, int> counts = new(StringComparer.OrdinalIgnoreCase);
            foreach (List<string> list in _tags.Values)
                foreach (string tag in list)
                    counts[tag] = counts.TryGetValue(tag, out int n) ? n + 1 : 1;
            _userTags = counts.Select(p => new TagCount(p.Key, p.Value))
                .OrderByDescending(t => t.Count).ThenBy(t => t.Tag, StringComparer.OrdinalIgnoreCase).ToList();
            return _userTags;
        }

        // @type and @ext tags of the files in the index, by group then value.
        public IReadOnlyList<TagCount> AttributeTags(AssetIndex index)
        {
            if (_attributeTags is not null && _attributeVersion == index.Version)
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
            _attributeTags = counts.Select(p => new TagCount(p.Key, p.Value))
                .OrderBy(t => t.Tag.StartsWith("@type:") ? 0 : 1).ThenBy(t => t.Tag, StringComparer.OrdinalIgnoreCase).ToList();
            _attributeVersion = index.Version;
            return _attributeTags;
        }

        private void Seed(AssetIndex index)
        {
            foreach (AssetEntry entry in index.Entries)
            {
                if (entry.IsFolder)
                    continue;
                List<string> list = new();
                string[] folders = entry.Parent.Split('/');
                for (int i = 1; i < folders.Length; i++)
                    AddUnique(list, NormalizeTag(folders[i]));
                uint hash = Fnv(entry.RelPath);
                AddUnique(list, _fakeTags[hash % (uint)_fakeTags.Length]);
                if ((hash >> 8) % 3 == 0)
                    AddUnique(list, _fakeTags[(hash >> 12) % (uint)_fakeTags.Length]);
                _tags[entry.RelPath] = list;
            }
        }

        private static void AddUnique(List<string> list, string tag)
        {
            if (tag.Length > 0 && !list.Contains(tag))
                list.Add(tag);
        }

        private static uint Fnv(string text)
        {
            uint hash = 2166136261;
            foreach (char c in text.ToLowerInvariant())
                hash = (hash ^ c) * 16777619;
            return hash;
        }

        private void Save()
        {
            try
            {
                Directory.CreateDirectory(Path.GetDirectoryName(_path)!);
                TagFile file = new() { Tags = _tags.OrderBy(p => p.Key, StringComparer.OrdinalIgnoreCase).ToDictionary(p => p.Key, p => p.Value) };
                File.WriteAllText(_path, JsonSerializer.Serialize(file, _options));
            }
            catch (Exception e) when (e is IOException or UnauthorizedAccessException)
            {
                Debug.LogError($"[Navigator] Could not save {_path}: {e.Message}");
            }
        }
    }
}
