// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.

using System.Text.Json;
using System.Text.Json.Nodes;
using System.Text.Json.Serialization;
using xg.editor;
using Debug = xg.Debug;

namespace xgEditor.CoreCLR
{
    public sealed class SavedSearch
    {
        [JsonPropertyName("mode")]
        public string Mode { get; set; } = "dir";

        [JsonPropertyName("query")]
        public string Query { get; set; } = string.Empty;
    }

    // A saved item: the asset's GUID (none for folders) and its last known path, kept for display and as a fallback.
    public sealed class SavedItem
    {
        [JsonPropertyName("guid")]
        [JsonIgnore(Condition = JsonIgnoreCondition.WhenWritingNull)]
        public string? Guid { get; set; }

        [JsonPropertyName("path")]
        public string Path { get; set; } = string.Empty;
    }

    // The project's shared Navigator bookmarks (ProjectSettings/Bookmarks.json): saved searches (a mode and its
    // location) and saved items. Items resolve by GUID first, so they follow renames and moves of their asset.
    public sealed class Bookmarks
    {
        private sealed class BookmarkFile
        {
            [JsonPropertyName("closeOnSelection")]
            public bool CloseOnSelection { get; set; } = true;

            [JsonPropertyName("searches")]
            public List<SavedSearch> Searches { get; set; } = new();

            [JsonPropertyName("items")]
            public List<SavedItem> Items { get; set; } = new();
        }

        private readonly string _path;
        private readonly AssetDatabase _db;
        private BookmarkFile _data = new();
        private int _resolvedVersion = -1;

        private Bookmarks(string path, AssetDatabase db)
        {
            _path = path;
            _db = db;
        }

        public IReadOnlyList<SavedSearch> Searches => _data.Searches;

        public IReadOnlyList<string> Items
        {
            get
            {
                Resolve();
                return _data.Items.ConvertAll(i => i.Path);
            }
        }

        public bool CloseOnSelection
        {
            get => _data.CloseOnSelection;
            set
            {
                if (_data.CloseOnSelection == value)
                    return;
                _data.CloseOnSelection = value;
                Save();
            }
        }

        public static Bookmarks Load(string projectRoot, AssetDatabase db)
        {
            Bookmarks bookmarks = new(Path.Combine(projectRoot, "ProjectSettings", "Bookmarks.json"), db);
            if (!File.Exists(bookmarks._path))
            {
                bookmarks.Save();
                return bookmarks;
            }
            try
            {
                bool migrated = false;
                JsonObject? root = JsonNode.Parse(File.ReadAllText(bookmarks._path), null, new JsonDocumentOptions { CommentHandling = JsonCommentHandling.Skip, AllowTrailingCommas = true }) as JsonObject;
                if (root?["items"] is JsonArray items)
                {
                    for (int i = 0; i < items.Count; i++)
                    {
                        if (items[i] is JsonValue value && value.TryGetValue(out string? path))
                        {
                            items[i] = new JsonObject { ["guid"] = db.GuidOf(path), ["path"] = path };
                            migrated = true;
                        }
                    }
                }
                bookmarks._data = root?.Deserialize<BookmarkFile>(JSONFile.Options) ?? new BookmarkFile();
                bookmarks._data.Items.RemoveAll(i => i.Path.Length == 0 && i.Guid is null);
                if (migrated)
                    bookmarks.Save();
            }
            catch (Exception e) when (e is IOException or JsonException or UnauthorizedAccessException)
            {
                Debug.LogError($"[Navigator] Could not read {bookmarks._path}: {e.Message}");
            }
            bookmarks.Resolve();
            return bookmarks;
        }

        public bool IsBookmarked(string relPath)
        {
            Resolve();
            return _data.Items.Any(i => string.Equals(i.Path, relPath, StringComparison.OrdinalIgnoreCase));
        }

        public void AddSearch(string mode, string query)
        {
            if (query.Length == 0 || _data.Searches.Any(s => s.Mode == mode && s.Query == query))
                return;
            _data.Searches.Add(new SavedSearch { Mode = mode, Query = query });
            Save();
        }

        public void RemoveSearch(SavedSearch search)
        {
            if (_data.Searches.Remove(search))
                Save();
        }

        public void AddItem(string relPath)
        {
            if (relPath.Length == 0 || IsBookmarked(relPath))
                return;
            _data.Items.Add(new SavedItem { Guid = _db.GuidOf(relPath), Path = relPath });
            Save();
        }

        public void RemoveItem(string relPath)
        {
            if (_data.Items.RemoveAll(i => string.Equals(i.Path, relPath, StringComparison.OrdinalIgnoreCase)) > 0)
                Save();
        }

        // Follows a rename or move: folder items and saved folder searches by path (assets follow their GUID).
        internal void Remap(AssetMove move)
        {
            bool changed = false;
            foreach (SavedItem item in _data.Items)
            {
                string path = move.Apply(item.Path);
                if (path != item.Path)
                {
                    item.Path = path;
                    changed = true;
                }
            }
            foreach (SavedSearch search in _data.Searches)
            {
                if (NavigatorState.ParseMode(search.Mode) != NavigatorMode.Directory)
                    continue;
                string query = move.Apply(search.Query);
                if (query != search.Query)
                {
                    search.Query = query;
                    changed = true;
                }
            }
            if (changed)
                Save();
        }

        private void Resolve()
        {
            if (_resolvedVersion == _db.Version)
                return;
            _resolvedVersion = _db.Version;
            bool changed = false;
            foreach (SavedItem item in _data.Items)
            {
                if (item.Guid is not null && _db.PathOf(item.Guid) is string path && path != item.Path)
                {
                    item.Path = path;
                    changed = true;
                }
                else if (item.Guid is null && _db.GuidOf(item.Path) is string guid)
                {
                    item.Guid = guid;
                    changed = true;
                }
            }
            if (changed)
                Save();
        }

        private void Save()
        {
            try
            {
                JSONFile.Write(_path, _data);
            }
            catch (Exception e) when (e is IOException or UnauthorizedAccessException)
            {
                Debug.LogError($"[Navigator] Could not save {_path}: {e.Message}");
            }
        }
    }
}
