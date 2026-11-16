// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.

using System.Text.Json;
using System.Text.Json.Serialization;
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

    // The project's shared Navigator bookmarks (ProjectSettings/Bookmarks.json): saved searches (a mode and its
    // location) and saved items (asset paths).
    public sealed class Bookmarks
    {
        private sealed class BookmarkFile
        {
            [JsonPropertyName("closeOnSelection")]
            public bool CloseOnSelection { get; set; } = true;

            [JsonPropertyName("searches")]
            public List<SavedSearch> Searches { get; set; } = new();

            [JsonPropertyName("items")]
            public List<string> Items { get; set; } = new();
        }

        private static readonly JsonSerializerOptions _options = new() { WriteIndented = true };

        private readonly string _path;
        private BookmarkFile _data = new();

        private Bookmarks(string path) => _path = path;

        public IReadOnlyList<SavedSearch> Searches => _data.Searches;
        public IReadOnlyList<string> Items => _data.Items;

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

        public static Bookmarks Load(string projectRoot)
        {
            Bookmarks bookmarks = new(Path.Combine(projectRoot, "ProjectSettings", "Bookmarks.json"));
            if (!File.Exists(bookmarks._path))
            {
                bookmarks.Save();
                return bookmarks;
            }
            try
            {
                bookmarks._data = JsonSerializer.Deserialize<BookmarkFile>(File.ReadAllText(bookmarks._path), _options) ?? new BookmarkFile();
            }
            catch (Exception e) when (e is IOException or JsonException or UnauthorizedAccessException)
            {
                Debug.LogError($"[Navigator] Could not read {bookmarks._path}: {e.Message}");
            }
            return bookmarks;
        }

        public bool IsBookmarked(string relPath) => _data.Items.Contains(relPath, StringComparer.OrdinalIgnoreCase);

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
            _data.Items.Add(relPath);
            Save();
        }

        public void RemoveItem(string relPath)
        {
            if (_data.Items.RemoveAll(p => string.Equals(p, relPath, StringComparison.OrdinalIgnoreCase)) > 0)
                Save();
        }

        private void Save()
        {
            try
            {
                Directory.CreateDirectory(Path.GetDirectoryName(_path)!);
                File.WriteAllText(_path, JsonSerializer.Serialize(_data, _options));
            }
            catch (Exception e) when (e is IOException or UnauthorizedAccessException)
            {
                Debug.LogError($"[Navigator] Could not save {_path}: {e.Message}");
            }
        }
    }
}
