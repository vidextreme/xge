// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.

using xg.imui;

namespace xgEditor.CoreCLR
{
    // The icons the icon picker offers: the built-in Icon glyphs (without window and play controls) and the project's
    // images (PNG/BMP) found under any folder named Icons (any case, any depth) in Assets/. The project list is rebuilt
    // when the asset index changes.
    public static class IconLibrary
    {
        private static readonly HashSet<Icon> _excluded = new()
        {
            Icon.None, Icon.WinMinimize, Icon.WinMaximize, Icon.WinRestore, Icon.WinClose, Icon.Kebab,
            Icon.Play, Icon.Pause, Icon.Step, Icon.PlayWindow,
        };
        private static readonly HashSet<string> _extensions = new(StringComparer.OrdinalIgnoreCase) { "png", "bmp" };

        private static IReadOnlyList<Icon>? _builtIn;
        private static readonly List<AssetEntry> _project = new();
        private static AssetIndex? _index;
        private static int _version = -1;

        public const string FolderName = "Icons";

        public static IReadOnlyList<Icon> BuiltIn => _builtIn ??= Enum.GetValues<Icon>().Where(i => !_excluded.Contains(i)).ToArray();

        public static IReadOnlyList<AssetEntry> Project
        {
            get
            {
                AssetIndex? index = ProjectService.Assets?.Index;
                if (index is null)
                {
                    _project.Clear();
                    _index = null;
                    return _project;
                }
                if (index != _index || index.Version != _version)
                {
                    _index = index;
                    _version = index.Version;
                    _project.Clear();
                    _project.AddRange(index.Entries.Where(IsProjectIcon).OrderBy(e => e.RelPath, StringComparer.OrdinalIgnoreCase));
                }
                return _project;
            }
        }

        // An image inside a folder named Icons below Assets/.
        public static bool IsProjectIcon(AssetEntry entry)
        {
            if (entry.IsFolder || !_extensions.Contains(entry.Extension))
                return false;
            string[] folders = entry.Parent.Split('/');
            for (int i = 1; i < folders.Length; i++)
                if (string.Equals(folders[i], FolderName, StringComparison.OrdinalIgnoreCase))
                    return true;
            return false;
        }
    }
}
