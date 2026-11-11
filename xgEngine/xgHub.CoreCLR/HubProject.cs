// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.
using System.Text.Json;
using xg.imui;

namespace xgHub.CoreCLR
{
    // One project in the hub list: manifest name, availability and the preview image
    // (<Project>/ProjectSettings/project-thumb.png).
    public sealed class HubProject : IDisposable
    {
        public const string ManifestExtension = ".xgproject";

        private DateTime _thumbnailTime;
        private bool _thumbnailDirty = true;

        public HubProject(HubProjectEntry entry)
        {
            Entry = entry;
            Name = System.IO.Path.GetFileName(entry.Path.TrimEnd('\\', '/'));
            Refresh();
        }

        public HubProjectEntry Entry { get; }
        public string Path => Entry.Path;
        public string Name { get; private set; }
        public bool Missing { get; private set; }
        public Texture? Thumbnail { get; private set; }
        public string ThumbnailPath => System.IO.Path.Combine(Path, "ProjectSettings", "project-thumb.png");

        public void Refresh()
        {
            string? manifest = null;
            try
            {
                if (Directory.Exists(Path))
                    manifest = Directory.EnumerateFiles(Path, "*" + ManifestExtension).FirstOrDefault();
            }
            catch (Exception e) when (e is IOException or UnauthorizedAccessException)
            {
            }
            Missing = manifest is null;
            if (manifest is not null)
                Name = ReadName(manifest) ?? System.IO.Path.GetFileNameWithoutExtension(manifest);

            DateTime time = File.Exists(ThumbnailPath) ? File.GetLastWriteTimeUtc(ThumbnailPath) : default;
            if (time != _thumbnailTime)
            {
                _thumbnailTime = time;
                _thumbnailDirty = true;
            }
        }

        public void InvalidateThumbnail() => _thumbnailDirty = true;

        // Called from OnGUI: textures are created on the UI thread while the host runs.
        public void UpdateThumbnail()
        {
            if (!_thumbnailDirty)
                return;
            _thumbnailDirty = false;
            Thumbnail?.Dispose();
            Thumbnail = null;
            if (!File.Exists(ThumbnailPath))
                return;
            byte[]? pixels = xg.ImageFile.LoadRGBA(ThumbnailPath, out int width, out int height);
            if (pixels is null)
            {
                xg.Debug.LogWarning($"[Hub] Cannot load preview {ThumbnailPath}");
                return;
            }
            Texture texture = new(pixels, width, height);
            if (texture.IsValid)
                Thumbnail = texture;
            else
                texture.Dispose();
        }

        public void Dispose()
        {
            Thumbnail?.Dispose();
            Thumbnail = null;
        }

        private static string? ReadName(string manifest)
        {
            try
            {
                using JsonDocument document = JsonDocument.Parse(File.ReadAllText(manifest));
                if (document.RootElement.TryGetProperty("Name", out JsonElement value) && value.ValueKind == JsonValueKind.String)
                    return value.GetString();
            }
            catch (Exception e) when (e is IOException or JsonException or UnauthorizedAccessException)
            {
            }
            return null;
        }
    }
}
