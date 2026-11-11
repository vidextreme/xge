// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.
using System.Text.Json;
using System.Text.Json.Serialization;
using xg;

namespace xgHub.CoreCLR
{
    public sealed class HubProjectEntry
    {
        [JsonPropertyName("path")]
        public string Path { get; set; } = string.Empty;

        [JsonPropertyName("lastOpened")]
        public DateTime? LastOpened { get; set; }
    }

    public sealed class HubData
    {
        [JsonPropertyName("projects")]
        public List<HubProjectEntry> Projects { get; set; } = new();

        [JsonPropertyName("newProjectLocation")]
        public string? NewProjectLocation { get; set; }
    }

    // Hub user data: %USERPROFILE%\ExtremeFocusGames\XGE\hub.json. Saved after every change.
    public static class HubStore
    {
        private static readonly JsonSerializerOptions _options = new() { WriteIndented = true };

        public static string Directory { get; } = System.IO.Path.Combine(
            Environment.GetFolderPath(Environment.SpecialFolder.UserProfile), "ExtremeFocusGames", "XGE");

        public static string FilePath { get; } = System.IO.Path.Combine(Directory, "hub.json");

        public static HubData Data { get; private set; } = new();

        public static void Load()
        {
            Data = new HubData();
            if (!File.Exists(FilePath))
                return;
            try
            {
                Data = JsonSerializer.Deserialize<HubData>(File.ReadAllText(FilePath), _options) ?? new HubData();
                Data.Projects.RemoveAll(p => string.IsNullOrWhiteSpace(p.Path));
            }
            catch (Exception e) when (e is IOException or JsonException or UnauthorizedAccessException)
            {
                Debug.LogError($"[Hub] Cannot read {FilePath}: {e.Message}");
            }
        }

        public static void Save()
        {
            try
            {
                System.IO.Directory.CreateDirectory(Directory);
                File.WriteAllText(FilePath, JsonSerializer.Serialize(Data, _options));
            }
            catch (Exception e) when (e is IOException or UnauthorizedAccessException)
            {
                Debug.LogError($"[Hub] Cannot write {FilePath}: {e.Message}");
            }
        }

        public static HubProjectEntry? Find(string path) =>
            Data.Projects.FirstOrDefault(p => SamePath(p.Path, path));

        public static HubProjectEntry Add(string path)
        {
            HubProjectEntry? entry = Find(path);
            if (entry is null)
            {
                entry = new HubProjectEntry { Path = System.IO.Path.GetFullPath(path) };
                Data.Projects.Add(entry);
            }
            Save();
            return entry;
        }

        public static void Remove(string path)
        {
            if (Data.Projects.RemoveAll(p => SamePath(p.Path, path)) > 0)
                Save();
        }

        public static void Touch(string path)
        {
            HubProjectEntry entry = Find(path) ?? Add(path);
            entry.LastOpened = DateTime.UtcNow;
            Save();
        }

        public static bool SamePath(string a, string b)
        {
            try
            {
                return string.Equals(System.IO.Path.GetFullPath(a).TrimEnd('\\', '/'), System.IO.Path.GetFullPath(b).TrimEnd('\\', '/'),
                    StringComparison.OrdinalIgnoreCase);
            }
            catch (Exception e) when (e is ArgumentException or NotSupportedException or PathTooLongException)
            {
                return false;
            }
        }
    }
}
