// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.

using xg.editor;
using xg.imui;

namespace xgEditor.CoreCLR
{
    // The asset pickers' view of the open project (AssetBrowser.Current while a project is open).
    public sealed class ProjectAssetBrowser : IAssetBrowser
    {
        private readonly AssetDatabase _assets;
        private readonly Dictionary<string, (int Version, List<AssetChoice> List)> _lists = new(StringComparer.OrdinalIgnoreCase);

        public ProjectAssetBrowser(AssetDatabase assets) => _assets = assets;

        public string? PathOf(string guid) => _assets.PathOf(guid);

        public IReadOnlyList<AssetChoice> List(string kind)
        {
            if (_lists.TryGetValue(kind, out var cached) && cached.Version == _assets.Version)
                return cached.List;
            bool any = !AssetTags.TryParseType(kind, out AssetKind wanted);
            List<AssetChoice> list = new();
            foreach (AssetEntry entry in _assets.Index.Entries)
            {
                if (entry.IsFolder || (!any && entry.Kind != wanted))
                    continue;
                string? guid = _assets.GuidOf(entry.RelPath);
                if (guid is not null)
                    list.Add(new AssetChoice(guid, entry.RelPath, entry.Name));
            }
            list.Sort((a, b) => string.Compare(a.Path, b.Path, StringComparison.OrdinalIgnoreCase));
            _lists[kind] = (_assets.Version, list);
            return list;
        }

        public string? SelectedGuid(string kind)
        {
            for (int i = Selection.Objects.Length - 1; i >= 0; i--)
            {
                if (Selection.Objects[i] is not AssetRef asset)
                    continue;
                if (AssetTags.TryParseType(kind, out AssetKind wanted) && AssetIndex.KindOf(Path.GetExtension(asset.Path).TrimStart('.')) != wanted)
                    continue;
                return _assets.GuidOf(asset.Path);
            }
            return null;
        }

        public Icon IconOf(string kind) => AssetTags.TryParseType(kind, out AssetKind wanted) ? AssetIndex.IconOf(wanted) : Icon.File;
    }
}
