// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.

using xg;
using xg.imui;
using xg.math;

namespace xgEditor.CoreCLR
{
    // Makes the pixels of an asset's thumbnail. Register one per kind with AssetThumbnails.Register; kinds without a
    // generator show their generic kind icon.
    public interface IThumbnailGenerator
    {
        bool Supports(AssetEntry entry);

        // RGBA8 pixels, or null when the asset can't be previewed.
        byte[]? Generate(AssetEntry entry, out int width, out int height);
    }

    // The editor's asset thumbnails, shared by every view (Navigator tiles, the Rename dialog, ...): cached by path and
    // write time, generated a few per frame, drawn fitted into a rect, with the kind icon until a thumbnail exists.
    public static class AssetThumbnails
    {
        private const int LoadsPerFrame = 4;

        private static readonly List<IThumbnailGenerator> _generators = new() { new ImageThumbnails() };
        private static readonly Dictionary<string, (Texture? Texture, DateTime Modified)> _cache = new(StringComparer.OrdinalIgnoreCase);
        private static int _loads;

        public static void Register(IThumbnailGenerator generator) => _generators.Insert(0, generator);

        // Call once per editor frame: resets the per-frame generation budget.
        public static void NewFrame() => _loads = 0;

        // Drops every cached thumbnail (assets changed on disk, project reloaded).
        public static void Clear()
        {
            foreach ((Texture? texture, _) in _cache.Values)
                texture?.Dispose();
            _cache.Clear();
        }

        // The thumbnail texture, or null when there is none (yet).
        public static Texture? Get(AssetEntry entry)
        {
            IThumbnailGenerator? generator = _generators.FirstOrDefault(g => g.Supports(entry));
            if (generator is null)
                return null;
            if (_cache.TryGetValue(entry.RelPath, out var cached) && cached.Modified == entry.Modified)
                return cached.Texture;
            if (_loads >= LoadsPerFrame)
                return null;
            _loads++;
            cached.Texture?.Dispose();
            Texture? texture = null;
            byte[]? pixels = generator.Generate(entry, out int width, out int height);
            if (pixels is not null && width > 0 && height > 0)
                texture = new Texture(pixels, width, height);
            _cache[entry.RelPath] = (texture, entry.Modified);
            return texture;
        }

        // Draws the thumbnail fitted into rect, or the kind icon (iconSize) when there is none.
        public static void Draw(Painter draw, Rect rect, AssetEntry entry, float iconSize)
        {
            if (Get(entry) is Texture texture)
            {
                float scale = Math.Min(rect.Width / texture.Width, rect.Height / texture.Height);
                draw.Image(texture, Rect.FromCenterSize(rect.Center, new Vector2(texture.Width * scale, texture.Height * scale)));
                return;
            }
            draw.Icon(AssetIndex.IconOf(entry.Kind), rect.Center, iconSize, AssetIndex.IconColorOf(entry.Kind));
        }

        // PNG and BMP files preview their own pixels.
        private sealed class ImageThumbnails : IThumbnailGenerator
        {
            private const int MaxSide = 2048;

            public bool Supports(AssetEntry entry) => entry.Kind == AssetKind.Image && entry.Extension is "png" or "bmp";

            public byte[]? Generate(AssetEntry entry, out int width, out int height)
            {
                byte[]? pixels = ImageFile.LoadRGBA(entry.FullPath, out width, out height);
                return width <= MaxSide && height <= MaxSide ? pixels : null;
            }
        }
    }
}
