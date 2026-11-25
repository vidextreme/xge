// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.

namespace xg.editor
{
    // An asset file on disk (project-relative path, size in bytes, last write time in UTC ticks). Embedded: the asset
    // carries its meta inside (AssetMetaStore), so an orphan meta file is never moved next to it.
    public readonly record struct AssetFileState(string Path, long Size, long ModifiedTicks, bool Embedded = false);

    // A meta on disk (a meta file, or the block embedded in the asset), by the path of its asset, and its GUID.
    public readonly record struct AssetMetaState(string AssetPath, string Guid);

    // What the asset cache remembered about an asset at the last scan (the hash is null until computed).
    public readonly record struct AssetCacheState(string Path, long Size, long ModifiedTicks, string? Hash);

    public enum AssetReconcileKind
    {
        // The asset has no meta: create one with a new GUID.
        Create,
        // The asset has no meta, but an orphan meta matches it (moved outside the editor): move From's meta next to it.
        Recover,
        // The asset's GUID is used by another asset too (copied with its meta) or isn't valid: give it a new one.
        Regenerate,
        // The meta's asset is gone and nothing matched it: delete the meta.
        Delete,
    }

    public readonly record struct AssetReconcileAction(AssetReconcileKind Kind, string Path, string? From = null, string? Guid = null);

    // A rename or move of an asset or a folder (project-relative paths).
    public readonly record struct AssetMove(string From, string To, bool IsFolder)
    {
        // The path after the move: From itself, or for a folder anything inside it.
        public string Apply(string path)
        {
            if (string.Equals(path, From, StringComparison.OrdinalIgnoreCase))
                return To;
            if (IsFolder && path.Length > From.Length && path[From.Length] == '/' && path.StartsWith(From, StringComparison.OrdinalIgnoreCase))
                return To + path[From.Length..];
            return path;
        }
    }

    // Matches asset files with meta files after a scan. Pure: the caller does the file work. A meta-less asset takes an
    // orphan meta only when the pair is unique both ways and, when the cache knows the orphan's asset, has its size plus
    // the same file name or the same content hash; without cache data the same file name is enough.
    public static class AssetReconciler
    {
        public static List<AssetReconcileAction> Reconcile(IReadOnlyList<AssetFileState> assets, IReadOnlyList<AssetMetaState> metas,
            IReadOnlyDictionary<string, AssetCacheState> cacheByGuid, Func<string, string?> hashOf)
        {
            StringComparer paths = StringComparer.OrdinalIgnoreCase;
            Dictionary<string, AssetFileState> assetByPath = new(paths);
            foreach (AssetFileState asset in assets)
                assetByPath[asset.Path] = asset;
            Dictionary<string, AssetMetaState> metaByAsset = new(paths);
            foreach (AssetMetaState meta in metas)
                metaByAsset[meta.AssetPath] = meta;

            List<AssetReconcileAction> regenerate = new();
            HashSet<string> liveGuids = new(StringComparer.OrdinalIgnoreCase);
            foreach (IGrouping<string, AssetMetaState> group in metaByAsset.Values.Where(m => assetByPath.ContainsKey(m.AssetPath))
                .GroupBy(m => m.Guid, StringComparer.OrdinalIgnoreCase).OrderBy(g => g.Key, StringComparer.Ordinal))
            {
                List<AssetMetaState> owners = group.OrderBy(m => m.AssetPath, StringComparer.Ordinal).ToList();
                if (!AssetMeta.IsValidGuid(group.Key))
                {
                    foreach (AssetMetaState meta in owners)
                        regenerate.Add(new AssetReconcileAction(AssetReconcileKind.Regenerate, meta.AssetPath, null, group.Key));
                    continue;
                }
                liveGuids.Add(group.Key);
                if (owners.Count == 1)
                    continue;
                AssetMetaState keeper = owners[0];
                if (cacheByGuid.TryGetValue(group.Key, out AssetCacheState cached))
                    foreach (AssetMetaState meta in owners)
                        if (paths.Equals(meta.AssetPath, cached.Path))
                            keeper = meta;
                foreach (AssetMetaState meta in owners)
                    if (!paths.Equals(meta.AssetPath, keeper.AssetPath))
                        regenerate.Add(new AssetReconcileAction(AssetReconcileKind.Regenerate, meta.AssetPath, null, group.Key));
            }

            List<AssetMetaState> orphans = metaByAsset.Values.Where(m => !assetByPath.ContainsKey(m.AssetPath))
                .OrderBy(m => m.AssetPath, StringComparer.Ordinal).ToList();
            List<AssetFileState> missing = assets.Where(a => !metaByAsset.ContainsKey(a.Path))
                .OrderBy(a => a.Path, StringComparer.Ordinal).ToList();

            Dictionary<string, string?> hashes = new(paths);
            string? HashOf(string path)
            {
                if (!hashes.TryGetValue(path, out string? hash))
                    hashes[path] = hash = hashOf(path);
                return hash;
            }

            Dictionary<int, List<int>> byAsset = new();
            Dictionary<int, List<int>> byOrphan = new();
            for (int o = 0; o < orphans.Count; o++)
            {
                AssetMetaState orphan = orphans[o];
                if (!AssetMeta.IsValidGuid(orphan.Guid) || liveGuids.Contains(orphan.Guid))
                    continue;
                bool known = cacheByGuid.TryGetValue(orphan.Guid, out AssetCacheState cached);
                string orphanName = FileName(orphan.AssetPath);
                for (int a = 0; a < missing.Count; a++)
                {
                    AssetFileState asset = missing[a];
                    if (asset.Embedded)
                        continue;
                    bool sameName = paths.Equals(FileName(asset.Path), orphanName);
                    bool match = known
                        ? cached.Size == asset.Size && (sameName || (cached.Hash is not null && string.Equals(HashOf(asset.Path), cached.Hash, StringComparison.OrdinalIgnoreCase)))
                        : sameName;
                    if (!match)
                        continue;
                    Add(byAsset, a, o);
                    Add(byOrphan, o, a);
                }
            }

            List<AssetReconcileAction> recover = new();
            HashSet<int> recoveredAssets = new();
            HashSet<int> recoveredOrphans = new();
            foreach ((int a, List<int> candidates) in byAsset.OrderBy(p => p.Key))
            {
                if (candidates.Count != 1 || byOrphan[candidates[0]].Count != 1)
                    continue;
                int o = candidates[0];
                recover.Add(new AssetReconcileAction(AssetReconcileKind.Recover, missing[a].Path, orphans[o].AssetPath, orphans[o].Guid));
                recoveredAssets.Add(a);
                recoveredOrphans.Add(o);
            }

            List<AssetReconcileAction> actions = new(regenerate);
            actions.AddRange(recover);
            for (int a = 0; a < missing.Count; a++)
                if (!recoveredAssets.Contains(a))
                    actions.Add(new AssetReconcileAction(AssetReconcileKind.Create, missing[a].Path));
            for (int o = 0; o < orphans.Count; o++)
                if (!recoveredOrphans.Contains(o))
                    actions.Add(new AssetReconcileAction(AssetReconcileKind.Delete, orphans[o].AssetPath, null, orphans[o].Guid));
            return actions;
        }

        private static string FileName(string path)
        {
            int slash = path.LastIndexOf('/');
            return slash < 0 ? path : path[(slash + 1)..];
        }

        private static void Add(Dictionary<int, List<int>> map, int key, int value)
        {
            if (!map.TryGetValue(key, out List<int>? list))
                map[key] = list = new List<int>();
            list.Add(value);
        }
    }
}
