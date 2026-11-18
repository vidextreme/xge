// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.

using System.Collections.Concurrent;
using System.Diagnostics;
using System.Security.Cryptography;
using System.Text.Json;
using System.Text.Json.Serialization;
using xg.editor;
using xg;
using Debug = xg.Debug;

namespace xgEditor.CoreCLR
{
    // Library/AssetCache.json: what the last scan saw of every asset (size, write time, GUID, tags, the meta's write
    // time and a content hash). Untracked and rebuilt when missing; it lets startup skip parsing unchanged metas and lets
    // the database recognise an asset that was moved outside the editor without its meta.
    internal sealed class AssetCache
    {
        internal sealed class Record
        {
            [JsonPropertyName("path")]
            public string Path { get; set; } = string.Empty;

            [JsonPropertyName("guid")]
            public string Guid { get; set; } = string.Empty;

            [JsonPropertyName("size")]
            public long Size { get; set; }

            [JsonPropertyName("modified")]
            public long Modified { get; set; }

            [JsonPropertyName("metaModified")]
            public long MetaModified { get; set; }

            [JsonPropertyName("hash")]
            [JsonIgnore(Condition = JsonIgnoreCondition.WhenWritingNull)]
            public string? Hash { get; set; }

            [JsonPropertyName("tags")]
            public List<string> Tags { get; set; } = new();
        }

        private sealed class CacheFile
        {
            [JsonPropertyName("version")]
            public int Version { get; set; } = 1;

            [JsonPropertyName("assets")]
            public List<Record> Assets { get; set; } = new();
        }

        private readonly string _path;

        private AssetCache(string path) => _path = path;

        public Dictionary<string, Record> ByPath { get; } = new(StringComparer.OrdinalIgnoreCase);

        public static AssetCache Load(string path)
        {
            AssetCache cache = new(path);
            if (!File.Exists(path))
                return cache;
            try
            {
                CacheFile? file = JSONFile.Read<CacheFile>(path);
                if (file is not null)
                    foreach (Record record in file.Assets)
                        if (record.Path.Length > 0)
                            cache.ByPath[record.Path] = record;
            }
            catch (Exception e) when (e is IOException or JsonException or UnauthorizedAccessException)
            {
                Debug.LogWarning($"[Assets] Ignoring the asset cache {path}: {e.Message}");
            }
            return cache;
        }

        public void Remap(AssetMove move)
        {
            List<Record> moved = ByPath.Values.Where(r => !string.Equals(move.Apply(r.Path), r.Path, StringComparison.Ordinal)).ToList();
            foreach (Record record in moved)
            {
                ByPath.Remove(record.Path);
                record.Path = move.Apply(record.Path);
                ByPath[record.Path] = record;
            }
        }

        public void Save()
        {
            try
            {
                CacheFile file = new() { Assets = ByPath.Values.OrderBy(r => r.Path, StringComparer.Ordinal).ToList() };
                JSONFile.Write(_path, file);
            }
            catch (Exception e) when (e is IOException or UnauthorizedAccessException)
            {
                Debug.LogWarning($"[Assets] Could not save the asset cache {_path}: {e.Message}");
            }
        }
    }

    // The open project's assets: the file index, one meta file per asset file (GUID, tags, importer settings), the tag
    // index, tags and bookmarks. Created by ProjectService.Open; Update runs every frame from the editor module's update.
    // After each scan it reconciles assets with metas: creates missing metas, recovers metas of assets moved outside the
    // editor, gives copies with a duplicate GUID a new one and deletes orphan metas once they stayed orphaned for 2 s.
    public sealed class AssetDatabase
    {
        private const long OrphanGraceMilliseconds = 2000;
        private const long CacheSaveDelayMilliseconds = 2000;
        private const long BuildBudgetMilliseconds = 8;

        private sealed record MetaInfo(string Guid, IReadOnlyList<string> Tags);

        private readonly Dictionary<string, MetaInfo> _metas = new(StringComparer.OrdinalIgnoreCase);
        private readonly Dictionary<string, string> _paths = new(StringComparer.OrdinalIgnoreCase);
        private readonly Dictionary<string, HashSet<string>> _tagIndex = new(StringComparer.OrdinalIgnoreCase);
        private readonly Dictionary<string, long> _orphanDeadlines = new(StringComparer.OrdinalIgnoreCase);
        private readonly HashSet<string> _reportedBroken = new(StringComparer.OrdinalIgnoreCase);
        private readonly ConcurrentQueue<(string Path, long Size, long Modified, string Hash)> _hashResults = new();
        private readonly AssetCache _cache;
        private Task? _hashTask;
        private bool _cacheDirty;
        private long _cacheDirtySince;
        private int _syncedIndexVersion = -1;
        private readonly Queue<string> _buildQueue = new();
        private readonly HashSet<string> _queuedBuilds = new(StringComparer.OrdinalIgnoreCase);
        private readonly Dictionary<string, (long Size, long Modified, long MetaModified)> _buildStamps = new(StringComparer.OrdinalIgnoreCase);
        private int _buildsDone;
        private int _buildsFailed;
        private bool _collectLibrary;

        public AssetDatabase(string projectRoot)
        {
            ProjectRoot = projectRoot;
            MigrateMetas(projectRoot);
            Index = new AssetIndex(projectRoot);
            _cache = AssetCache.Load(Path.Combine(projectRoot, "Library", "AssetCache.json"));
            Pipeline = new AssetPipeline(Path.Combine(projectRoot, "Library", "Data"));
            TextureAssets.Register(Pipeline, DecodeImage);
            Sync();
            Tags = new AssetTags(this);
            Bookmarks = Bookmarks.Load(projectRoot, this);
            Tags.Migrate();
            AssetRef.GuidResolver = GuidOf;
            SaveCache();
        }

        private static void MigrateMetas(string projectRoot)
        {
            AssetMetaMigrationResult result = AssetMetaMigration.Run(Path.Combine(projectRoot, AssetIndex.RootFolder));
            if (result.Migrated + result.Unreadable > 0)
                Debug.Log($"[Assets] Migrated {result.Migrated + result.Unreadable} meta file(s) from {AssetMeta.LegacyExtension} to {AssetMeta.Extension}");
            foreach (string problem in result.Problems)
                Debug.LogWarning($"[Assets] Meta migration: {problem}");
        }

        public string ProjectRoot { get; }
        public AssetIndex Index { get; }
        public AssetPipeline Pipeline { get; }
        public AssetTags Tags { get; }
        public Bookmarks Bookmarks { get; }
        // Changes whenever metas, tags or the index change.
        public int Version { get; private set; }

        // A rename or move done by the editor, or a meta recovered after a move outside it.
        public event Action<AssetMove>? Moved;

        public string? GuidOf(string path) =>
            _metas.TryGetValue(AssetIndex.Normalize(path), out MetaInfo? info) ? info.Guid : null;

        public string? PathOf(string guid) => _paths.TryGetValue(guid, out string? path) ? path : null;

        public IReadOnlyList<string> TagsOf(string path) =>
            _metas.TryGetValue(AssetIndex.Normalize(path), out MetaInfo? info) ? info.Tags : Array.Empty<string>();

        public IReadOnlyCollection<string> GuidsWithTag(string tag) =>
            _tagIndex.TryGetValue(tag, out HashSet<string>? guids) ? guids : Array.Empty<string>();

        public IEnumerable<TagCount> TagCounts => _tagIndex.Select(p => new TagCount(p.Key, p.Value.Count));

        public string FullPath(string relPath) => Path.Combine(ProjectRoot, relPath.Replace('/', Path.DirectorySeparatorChar));

        public void Update()
        {
            if (Index.Refresh() || Index.Version != _syncedIndexVersion)
                Sync();
            ProcessOrphans();
            ProcessBuilds(BuildBudgetMilliseconds);
            ApplyHashes();
            if (_cacheDirty && Environment.TickCount64 - _cacheDirtySince >= CacheSaveDelayMilliseconds)
                SaveCache();
        }

        // Runs every queued asset build now instead of spreading them over frames.
        public void BuildAll() => ProcessBuilds(long.MaxValue);

        // Writes pending cache changes now (the editor calls it on shutdown).
        public void Flush()
        {
            ApplyHashes();
            if (_cacheDirty)
                SaveCache();
        }

        public void Refresh()
        {
            Index.Refresh(true);
            Sync();
        }

        // Replaces an asset's tags (normalized, sorted) in its meta file. Returns false when the asset has no meta.
        public bool SetTags(string path, IEnumerable<string> tags)
        {
            path = AssetIndex.Normalize(path);
            if (!_metas.ContainsKey(path))
                return false;
            string metaPath = FullPath(AssetMeta.PathFor(path));
            AssetMeta meta;
            try
            {
                meta = AssetMeta.Read(metaPath);
            }
            catch (Exception e) when (e is IOException or JsonException or UnauthorizedAccessException)
            {
                Debug.LogError($"[Assets] Could not read {metaPath}: {e.Message}");
                return false;
            }
            meta.Tags = NormalizeTags(tags).ToList();
            try
            {
                meta.Write(metaPath);
            }
            catch (Exception e) when (e is IOException or UnauthorizedAccessException)
            {
                Debug.LogError($"[Assets] Could not write {metaPath}: {e.Message}");
                return false;
            }
            MetaInfo info = new(meta.Guid, meta.Tags);
            SetMeta(path, info);
            if (_cache.ByPath.TryGetValue(path, out AssetCache.Record? record))
            {
                record.Tags = meta.Tags;
                record.MetaModified = File.GetLastWriteTimeUtc(metaPath).Ticks;
                MarkCacheDirty();
            }
            Version++;
            return true;
        }

        // Stores an import result in the asset's meta: known items keep their chunk GUIDs, new ones get random GUIDs.
        // Returns null when the asset has no usable meta.
        public AssetItemUpdate? UpdateItems(string path, IEnumerable<ImportedAssetItem> imported)
        {
            path = AssetIndex.Normalize(path);
            if (!_metas.ContainsKey(path))
                return null;
            string metaPath = FullPath(AssetMeta.PathFor(path));
            try
            {
                AssetMeta meta = AssetMeta.Read(metaPath);
                AssetItemUpdate update = AssetItemTable.Update(meta.Items, imported);
                foreach (string warning in update.Warnings)
                    Debug.LogWarning($"[Assets] {path}: {warning}");
                meta.Items = update.Items;
                if (meta.Write(metaPath))
                {
                    if (_cache.ByPath.TryGetValue(path, out AssetCache.Record? record))
                    {
                        record.MetaModified = File.GetLastWriteTimeUtc(metaPath).Ticks;
                        MarkCacheDirty();
                    }
                    Version++;
                }
                return update;
            }
            catch (Exception e) when (e is IOException or JsonException or UnauthorizedAccessException)
            {
                Debug.LogError($"[Assets] Could not update the items of {path}: {e.Message}");
                return null;
            }
        }

        // Renames the tag on every asset that has it. Returns the number of assets changed.
        public int RenameTag(string from, string to)
        {
            int count = 0;
            foreach (string guid in GuidsWithTag(from).ToList())
                if (PathOf(guid) is string path && SetTags(path, TagsOf(path).Select(t => string.Equals(t, from, StringComparison.OrdinalIgnoreCase) ? to : t)))
                    count++;
            return count;
        }

        public int DeleteTag(string tag)
        {
            int count = 0;
            foreach (string guid in GuidsWithTag(tag).ToList())
                if (PathOf(guid) is string path && SetTags(path, TagsOf(path).Where(t => !string.Equals(t, tag, StringComparison.OrdinalIgnoreCase))))
                    count++;
            return count;
        }

        // Null when the name is valid for an asset or folder, otherwise the reason.
        public static string? ValidateName(string name)
        {
            if (name.Length == 0)
                return "Enter a name";
            if (name.IndexOfAny(Path.GetInvalidFileNameChars()) >= 0 || name.Contains('/') || name.Contains('\\'))
                return "A name can't contain \\ / : * ? \" < > |";
            if (name.StartsWith('.'))
                return "Names starting with '.' are hidden";
            if (name.EndsWith(' ') || name.EndsWith('.'))
                return "A name can't end with a space or a dot";
            if (AssetMeta.IsMetaPath(name) || AssetMeta.IsLegacyMetaPath(name))
                return $"'{AssetMeta.Extension}' and '{AssetMeta.LegacyExtension}' are reserved for meta files";
            if (name.EndsWith('~') || name.EndsWith(".tmp", StringComparison.OrdinalIgnoreCase))
                return "Names ending with '~' or '.tmp' are treated as temporary files";
            return null;
        }

        // Renames an asset (with its meta) or a folder in place. Returns null on success, otherwise the reason.
        public string? Rename(string path, string newName)
        {
            path = AssetIndex.Normalize(path);
            AssetEntry? entry = Index.Find(path);
            if (entry is null)
                return $"'{path}' no longer exists";
            newName = newName.Trim();
            if (string.Equals(newName, entry.Name, StringComparison.Ordinal))
                return null;
            if (ValidateName(newName) is string error)
                return error;
            string to = entry.Parent.Length > 0 ? entry.Parent + "/" + newName : newName;
            string fromFull = entry.FullPath;
            string toFull = FullPath(to);
            bool caseOnly = string.Equals(newName, entry.Name, StringComparison.OrdinalIgnoreCase);
            if (!caseOnly && (File.Exists(toFull) || Directory.Exists(toFull)))
                return $"'{newName}' already exists";
            try
            {
                if (entry.IsFolder)
                {
                    MovePath(fromFull, toFull, caseOnly, true);
                }
                else
                {
                    MovePath(fromFull, toFull, caseOnly, false);
                    string meta = AssetMeta.PathFor(fromFull);
                    if (File.Exists(meta))
                        MovePath(meta, AssetMeta.PathFor(toFull), caseOnly, false);
                }
            }
            catch (Exception e) when (e is IOException or UnauthorizedAccessException)
            {
                return e.Message;
            }
            AssetMove move = new(path, to, entry.IsFolder);
            _cache.Remap(move);
            Refresh();
            Debug.Log($"[Assets] Renamed {path} -> {to}");
            OnMoved(move);
            return null;
        }

        private static void MovePath(string from, string to, bool caseOnly, bool directory)
        {
            if (caseOnly)
            {
                string temp = to + ".rename~";
                MovePath(from, temp, false, directory);
                MovePath(temp, to, false, directory);
                return;
            }
            if (directory)
                Directory.Move(from, to);
            else
                File.Move(from, to);
        }

        private void OnMoved(AssetMove move)
        {
            Selection.RemapAssets(move.Apply);
            Bookmarks?.Remap(move);
            Moved?.Invoke(move);
        }

        private static IEnumerable<string> NormalizeTags(IEnumerable<string> tags) =>
            tags.Select(AssetTags.NormalizeTag).Where(t => t.Length > 0 && !t.StartsWith('@'))
                .Distinct(StringComparer.OrdinalIgnoreCase).Order(StringComparer.Ordinal);

        private void SetMeta(string path, MetaInfo info)
        {
            if (_metas.TryGetValue(path, out MetaInfo? old))
                foreach (string tag in old.Tags)
                    if (_tagIndex.TryGetValue(tag, out HashSet<string>? guids) && guids.Remove(old.Guid) && guids.Count == 0)
                        _tagIndex.Remove(tag);
            _metas[path] = info;
            _paths[info.Guid] = path;
            foreach (string tag in info.Tags)
            {
                if (!_tagIndex.TryGetValue(tag, out HashSet<string>? guids))
                    _tagIndex[tag] = guids = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
                guids.Add(info.Guid);
            }
        }

        private void Sync()
        {
            _syncedIndexVersion = Index.Version;
            List<AssetFileState> assets = Index.Entries.Where(e => !e.IsFolder)
                .Select(e => new AssetFileState(e.RelPath, e.Size, e.ModifiedTicksUtc)).ToList();
            List<AssetMetaState> metas = new();
            Dictionary<string, MetaInfo> infos = new(StringComparer.OrdinalIgnoreCase);
            HashSet<string> broken = new(StringComparer.OrdinalIgnoreCase);
            foreach (string metaRel in Index.MetaFiles)
            {
                string assetPath = AssetMeta.AssetPathOf(metaRel);
                if (Index.IsDirectory(assetPath))
                    continue;
                string full = FullPath(metaRel);
                MetaInfo? info = null;
                long metaTicks = File.GetLastWriteTimeUtc(full).Ticks;
                if (_cache.ByPath.TryGetValue(assetPath, out AssetCache.Record? record) && record.MetaModified == metaTicks && AssetMeta.IsValidGuid(record.Guid))
                {
                    info = new MetaInfo(record.Guid, record.Tags);
                }
                else
                {
                    try
                    {
                        AssetMeta meta = AssetMeta.Read(full);
                        info = new MetaInfo(meta.Guid, NormalizeTags(meta.Tags).ToList());
                        _reportedBroken.Remove(assetPath);
                    }
                    catch (Exception e) when (e is IOException or JsonException or UnauthorizedAccessException)
                    {
                        broken.Add(assetPath);
                        if (_reportedBroken.Add(assetPath))
                            Debug.LogError($"[Assets] Could not read {metaRel}: {e.Message}. Fix or delete it; it is left untouched.");
                        continue;
                    }
                }
                infos[assetPath] = info;
                metas.Add(new AssetMetaState(assetPath, info.Guid));
            }
            assets.RemoveAll(a => broken.Contains(a.Path));

            Dictionary<string, AssetCacheState> cacheByGuid = new(StringComparer.OrdinalIgnoreCase);
            foreach (AssetCache.Record record in _cache.ByPath.Values)
                if (AssetMeta.IsValidGuid(record.Guid))
                    cacheByGuid.TryAdd(record.Guid, new AssetCacheState(record.Path, record.Size, record.Modified, record.Hash));

            List<AssetMove> moves = new();
            int created = 0;
            long now = Environment.TickCount64;
            foreach (AssetReconcileAction action in AssetReconciler.Reconcile(assets, metas, cacheByGuid, HashFile))
            {
                string metaFull = FullPath(AssetMeta.PathFor(action.Path));
                try
                {
                    switch (action.Kind)
                    {
                        case AssetReconcileKind.Regenerate:
                        {
                            AssetMeta meta;
                            try
                            {
                                meta = AssetMeta.Read(metaFull);
                            }
                            catch (JsonException)
                            {
                                meta = new AssetMeta();
                            }
                            string old = meta.Guid;
                            meta.Guid = AssetMeta.NewGuid();
                            meta.Write(metaFull);
                            infos[action.Path] = new MetaInfo(meta.Guid, NormalizeTags(meta.Tags).ToList());
                            Debug.LogWarning($"[Assets] {action.Path} had a duplicate or invalid GUID '{old}' (copied with its meta?); it now has {meta.Guid}");
                            break;
                        }
                        case AssetReconcileKind.Recover:
                        {
                            File.Move(FullPath(AssetMeta.PathFor(action.From!)), metaFull);
                            if (infos.Remove(action.From!, out MetaInfo? info))
                                infos[action.Path] = info;
                            _orphanDeadlines.Remove(action.From!);
                            AssetMove move = new(action.From!, action.Path, false);
                            _cache.Remap(move);
                            moves.Add(move);
                            Debug.Log($"[Assets] Recovered the meta of {action.Path} (moved from {action.From} outside the editor)");
                            break;
                        }
                        case AssetReconcileKind.Create:
                        {
                            AssetMeta meta = AssetMeta.Create();
                            meta.Write(metaFull);
                            infos[action.Path] = new MetaInfo(meta.Guid, Array.Empty<string>());
                            created++;
                            break;
                        }
                        case AssetReconcileKind.Delete:
                            infos.Remove(action.Path);
                            _orphanDeadlines.TryAdd(action.Path, now + OrphanGraceMilliseconds);
                            break;
                    }
                }
                catch (Exception e) when (e is IOException or UnauthorizedAccessException)
                {
                    Debug.LogError($"[Assets] Could not update the meta of {action.Path}: {e.Message}");
                }
            }
            if (created > 0)
                Debug.Log($"[Assets] Created {created} meta file(s)");

            HashSet<string> live = new(assets.Select(a => a.Path), StringComparer.OrdinalIgnoreCase);
            foreach (string path in _orphanDeadlines.Keys.Where(live.Contains).ToList())
                _orphanDeadlines.Remove(path);

            _metas.Clear();
            _paths.Clear();
            _tagIndex.Clear();
            foreach ((string path, MetaInfo info) in infos)
                if (live.Contains(path))
                    SetMeta(path, info);

            Dictionary<string, AssetCache.Record> records = new(StringComparer.OrdinalIgnoreCase);
            foreach (AssetFileState asset in assets)
            {
                if (!_metas.TryGetValue(asset.Path, out MetaInfo? info))
                    continue;
                _cache.ByPath.TryGetValue(asset.Path, out AssetCache.Record? old);
                string metaFull = FullPath(AssetMeta.PathFor(asset.Path));
                records[asset.Path] = new AssetCache.Record
                {
                    Path = asset.Path,
                    Guid = info.Guid,
                    Size = asset.Size,
                    Modified = asset.ModifiedTicks,
                    MetaModified = File.Exists(metaFull) ? File.GetLastWriteTimeUtc(metaFull).Ticks : 0,
                    Hash = old is not null && old.Size == asset.Size && old.Modified == asset.ModifiedTicks ? old.Hash : null,
                    Tags = info.Tags.ToList(),
                };
            }
            foreach (string pending in _orphanDeadlines.Keys)
                if (_cache.ByPath.TryGetValue(pending, out AssetCache.Record? old))
                    records[pending] = old;
            _cache.ByPath.Clear();
            foreach ((string path, AssetCache.Record record) in records)
                _cache.ByPath[path] = record;
            MarkCacheDirty();
            StartHashing();
            QueueBuilds();

            Version++;
            foreach (AssetMove move in moves)
                OnMoved(move);
        }

        private static DecodedImage? DecodeImage(string path)
        {
            byte[]? pixels = ImageFile.LoadRGBA(path, out int width, out int height);
            return pixels is null ? null : new DecodedImage(width, height, pixels);
        }

        private void QueueBuilds()
        {
            _collectLibrary = true;
            foreach (AssetCache.Record record in _cache.ByPath.Values.OrderBy(r => r.Path, StringComparer.Ordinal))
            {
                if (!_metas.ContainsKey(record.Path) || !Pipeline.CanImport(record.Path))
                    continue;
                if (_buildStamps.TryGetValue(record.Path, out var stamp) && stamp == (record.Size, record.Modified, record.MetaModified))
                    continue;
                if (_queuedBuilds.Add(record.Path))
                    _buildQueue.Enqueue(record.Path);
            }
        }

        private void ProcessBuilds(long budgetMilliseconds)
        {
            long start = Stopwatch.GetTimestamp();
            while (_buildQueue.Count > 0)
            {
                string path = _buildQueue.Dequeue();
                _queuedBuilds.Remove(path);
                BuildAsset(path);
                if (Stopwatch.GetElapsedTime(start).TotalMilliseconds >= budgetMilliseconds)
                    break;
            }
            if (_buildQueue.Count > 0)
                return;
            if (_buildsDone + _buildsFailed > 0)
            {
                Debug.Log($"[Assets] Built {_buildsDone} asset(s), {_buildsFailed} failed");
                _buildsDone = 0;
                _buildsFailed = 0;
            }
            if (_collectLibrary && _orphanDeadlines.Count == 0)
            {
                _collectLibrary = false;
                CollectLibrary();
            }
        }

        private void BuildAsset(string path)
        {
            if (!_metas.TryGetValue(path, out MetaInfo? info) || !_cache.ByPath.TryGetValue(path, out AssetCache.Record? record))
                return;
            string metaPath = FullPath(AssetMeta.PathFor(path));
            AssetBuildResult result;
            try
            {
                AssetMeta meta = AssetMeta.Read(metaPath);
                result = Pipeline.Build(new AssetBuildRequest(info.Guid, FullPath(path), record.Hash, meta.ImportSettings, meta.ProcessSettings, meta.Items,
                    items => UpdateItems(path, items)));
            }
            catch (Exception e) when (e is IOException or JsonException or UnauthorizedAccessException)
            {
                result = new AssetBuildResult(AssetBuildStatus.Failed, e.Message);
            }

            switch (result.Status)
            {
                case AssetBuildStatus.Imported:
                case AssetBuildStatus.Processed:
                    _buildsDone++;
                    break;
                case AssetBuildStatus.Failed:
                    _buildsFailed++;
                    Debug.LogError($"[Assets] Could not build {path}: {result.Error}");
                    break;
            }
            if (_cache.ByPath.TryGetValue(path, out record))
                _buildStamps[path] = (record.Size, record.Modified, record.MetaModified);
        }

        private void CollectLibrary()
        {
            HashSet<string> live = new(StringComparer.OrdinalIgnoreCase);
            foreach ((string path, MetaInfo info) in _metas)
                if (Pipeline.CanImport(path))
                    live.Add(info.Guid);
            try
            {
                int removed = Pipeline.RemoveOrphans(live);
                if (removed > 0)
                    Debug.Log($"[Assets] Removed generated data of {removed} deleted asset(s)");
            }
            catch (Exception e) when (e is IOException or UnauthorizedAccessException)
            {
                Debug.LogWarning($"[Assets] Could not clean the Library: {e.Message}");
            }
        }

        private void ProcessOrphans()
        {
            if (_orphanDeadlines.Count == 0)
                return;
            long now = Environment.TickCount64;
            foreach ((string path, long deadline) in _orphanDeadlines.ToList())
            {
                if (deadline > now)
                    continue;
                _orphanDeadlines.Remove(path);
                string assetFull = FullPath(path);
                if (File.Exists(assetFull) || Directory.Exists(assetFull))
                    continue;
                string metaFull = FullPath(AssetMeta.PathFor(path));
                try
                {
                    if (File.Exists(metaFull))
                    {
                        File.Delete(metaFull);
                        Debug.Log($"[Assets] Deleted the meta of {path} (its asset is gone)");
                    }
                }
                catch (Exception e) when (e is IOException or UnauthorizedAccessException)
                {
                    Debug.LogError($"[Assets] Could not delete {metaFull}: {e.Message}");
                }
                _cache.ByPath.Remove(path);
                MarkCacheDirty();
            }
        }

        private string? HashFile(string relPath) => HashFullPath(FullPath(relPath));

        private static string? HashFullPath(string fullPath)
        {
            try
            {
                using FileStream stream = File.OpenRead(fullPath);
                return Convert.ToHexStringLower(SHA1.HashData(stream));
            }
            catch (Exception e) when (e is IOException or UnauthorizedAccessException)
            {
                return null;
            }
        }

        private void StartHashing()
        {
            if (_hashTask is { IsCompleted: false })
                return;
            List<(string Path, string Full, long Size, long Modified)> work = _cache.ByPath.Values
                .Where(r => r.Hash is null && _metas.ContainsKey(r.Path))
                .Select(r => (r.Path, FullPath(r.Path), r.Size, r.Modified)).ToList();
            if (work.Count == 0)
                return;
            _hashTask = Task.Run(() =>
            {
                foreach ((string path, string full, long size, long modified) in work)
                    if (HashFullPath(full) is string hash)
                        _hashResults.Enqueue((path, size, modified, hash));
            });
        }

        private void ApplyHashes()
        {
            while (_hashResults.TryDequeue(out var result))
            {
                if (_cache.ByPath.TryGetValue(result.Path, out AssetCache.Record? record) && record.Hash is null
                    && record.Size == result.Size && record.Modified == result.Modified)
                {
                    record.Hash = result.Hash;
                    MarkCacheDirty();
                }
            }
        }

        private void MarkCacheDirty()
        {
            if (!_cacheDirty)
                _cacheDirtySince = Environment.TickCount64;
            _cacheDirty = true;
        }

        private void SaveCache()
        {
            _cache.Save();
            _cacheDirty = false;
        }
    }
}
