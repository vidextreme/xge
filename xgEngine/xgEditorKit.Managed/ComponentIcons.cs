// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.

using System.Globalization;
using System.Reflection;
using xg.imui;
using xg.math;

namespace xg.editor
{
    // Resolved editor icon of a component type: a built-in icon tinted with Color, or a loaded image.
    public readonly struct ComponentIcon
    {
        public ComponentIcon(Icon icon, Texture? texture, Color32 color)
        {
            Icon = icon;
            Texture = texture;
            Color = color;
        }

        public Icon Icon { get; }
        public Texture? Texture { get; }
        public Color32 Color { get; }

        public void Draw(Painter draw, Vector2 center, float size)
        {
            if (Texture != null && Texture.IsValid)
                draw.Image(Texture, Rect.FromCenterSize(center, new Vector2(size, size)));
            else
                draw.Icon(Icon, center, size, Color);
        }
    }

    // Reads [ComponentIcon] from component types (by name, so any load context works) and caches the result per type.
    // Image paths are tried as given, then relative to the game project folder, then to gameroot/editor.
    public static class ComponentIcons
    {
        private static readonly Dictionary<string, ComponentIcon> _byType = new();
        private static readonly Dictionary<string, ComponentIcon> _byName = new(StringComparer.OrdinalIgnoreCase);
        private static readonly Dictionary<string, Texture?> _textures = new(StringComparer.OrdinalIgnoreCase);

        public static ComponentIcon Get(Type type)
        {
            string key = TypeBridge.KeyOf(type);
            if (_byType.TryGetValue(key, out ComponentIcon cached))
                return cached;
            ComponentIcon icon = Resolve(type);
            _byType[key] = icon;
            return icon;
        }

        // Resolves an icon by name: a built-in Icon name or an image path (same rules as [ComponentIcon]), with an optional
        // "#RRGGBB" tint. Unknown names fall back to fallback.
        public static ComponentIcon Resolve(string? name, string? colorHex, Icon fallback = Icon.Script)
        {
            string key = $"{name}|{colorHex}|{fallback}";
            if (_byName.TryGetValue(key, out ComponentIcon cached))
                return cached;
            Color32 color = Style.Color(StyleColor.Icon);
            if (colorHex is not null && TryParseColor(colorHex, out Color32 parsed))
                color = parsed;
            ComponentIcon icon;
            if (string.IsNullOrEmpty(name))
                icon = new ComponentIcon(fallback, null, color);
            else if (Enum.TryParse(name, true, out Icon builtin) && Enum.IsDefined(builtin))
                icon = new ComponentIcon(builtin, null, color);
            else
            {
                Texture? texture = LoadTexture(name);
                icon = new ComponentIcon(texture != null ? Icon.None : fallback, texture, color);
            }
            _byName[key] = icon;
            return icon;
        }

        // Drops cached icons and textures (e.g. after a project reload).
        public static void Clear()
        {
            _byType.Clear();
            _byName.Clear();
            foreach (Texture? texture in _textures.Values)
                texture?.Dispose();
            _textures.Clear();
        }

        private static ComponentIcon Resolve(Type type)
        {
            Color32 color = Style.Color(StyleColor.Icon);
            CustomAttributeData? data = TypeBridge.GetAttributeData(type, typeof(ComponentIconAttribute));
            if (data is null || data.ConstructorArguments.Count == 0 || data.ConstructorArguments[0].Value is not string name || name.Length == 0)
                return new ComponentIcon(Icon.Script, null, color);

            foreach (CustomAttributeNamedArgument argument in data.NamedArguments)
                if (argument.MemberName == nameof(ComponentIconAttribute.Color) && argument.TypedValue.Value is string hex && TryParseColor(hex, out Color32 parsed))
                    color = parsed;

            if (Enum.TryParse(name, true, out Icon builtin) && Enum.IsDefined(builtin))
                return new ComponentIcon(builtin, null, color);
            Texture? texture = LoadTexture(name);
            return new ComponentIcon(texture != null ? Icon.None : Icon.Script, texture, color);
        }

        private static Texture? LoadTexture(string path)
        {
            if (_textures.TryGetValue(path, out Texture? cached))
                return cached;
            Texture? texture = null;
            foreach (string candidate in Candidates(path))
            {
                if (!File.Exists(candidate))
                    continue;
                byte[]? rgba = ImageFile.LoadRGBA(candidate, out int width, out int height);
                if (rgba is null)
                    continue;
                texture = new Texture(rgba, width, height);
                if (texture.IsValid)
                    break;
                texture.Dispose();
                texture = null;
            }
            if (texture is null)
                Debug.LogWarning($"[Inspector] Component icon not found or unreadable: {path}");
            _textures[path] = texture;
            return texture;
        }

        private static IEnumerable<string> Candidates(string path)
        {
            if (Path.IsPathRooted(path))
            {
                yield return path;
                yield break;
            }
            string project = ScriptRuntime.ProjectPath;
            if (project.Length > 0)
                yield return Path.Combine(project, path);
            yield return Path.GetFullPath(Path.Combine("..", "editor", path));
        }

        private static bool TryParseColor(string text, out Color32 color)
        {
            color = default;
            string hex = text.TrimStart('#');
            if (hex.Length != 6 || !uint.TryParse(hex, NumberStyles.HexNumber, CultureInfo.InvariantCulture, out uint value))
                return false;
            color = new Color32((byte)(value >> 16), (byte)(value >> 8), (byte)value, 255);
            return true;
        }
    }
}
