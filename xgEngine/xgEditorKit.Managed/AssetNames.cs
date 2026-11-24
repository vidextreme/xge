// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.

namespace xg.editor
{
    // How the editor shows asset names: without the file extension, which stays an implementation detail of the file
    // (the asset's kind is shown by its icon and type label instead). Code that needs the real file name uses the path.
    public static class AssetNames
    {
        // The last path segment without its extension; folders (isFolder) and names that would become empty keep it all.
        public static string DisplayName(string path, bool isFolder = false)
        {
            string name = FileName(path);
            if (isFolder)
                return name;
            int dot = name.LastIndexOf('.');
            return dot > 0 ? name[..dot] : name;
        }

        // The folder part of the path plus DisplayName ("Assets/Art/Sword" for "Assets/Art/Sword.png").
        public static string DisplayPath(string path, bool isFolder = false)
        {
            string normalized = path.Replace('\\', '/').TrimEnd('/');
            int slash = normalized.LastIndexOf('/');
            string name = DisplayName(normalized, isFolder);
            return slash < 0 ? name : normalized[..(slash + 1)] + name;
        }

        // The extension with its dot (".png"), or empty for folders and names without one.
        public static string Extension(string path, bool isFolder = false)
        {
            if (isFolder)
                return string.Empty;
            string name = FileName(path);
            int dot = name.LastIndexOf('.');
            return dot > 0 ? name[dot..] : string.Empty;
        }

        private static string FileName(string path)
        {
            string normalized = path.Replace('\\', '/').TrimEnd('/');
            int slash = normalized.LastIndexOf('/');
            return slash < 0 ? normalized : normalized[(slash + 1)..];
        }
    }
}
