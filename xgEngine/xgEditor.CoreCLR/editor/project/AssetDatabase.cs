// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.

using System.Collections.Concurrent;
using System.Diagnostics;
using System.Security.Cryptography;
using System.Text.Json;
using System.Text.Json.Serialization;
using xg.editor;
using xg;
using xg.imui;
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
        private static readonly bool _logBuildThreads = Environment.GetEnvironmentVariable("XG_ASSET_TEST_THREADS") == "1";
        private readonly int _mainThreadId = Environment.CurrentManagedThreadId;
        private static readonly int _slowStageMilliseconds =
            int.TryParse(Environment.GetEnvironmentVariable("XG_UI_TEST_TASKS_SLOW"), out int slow) ? Math.Max(slow, 0) : 0;
        private const long SettleMilliseconds = 1000;

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
        private sealed class RunningBuild
        {
            public required string Guid;
            public required BackgroundTask Task;
            public required CancellationTokenSource Cancel;
            public AssetBuildResult Result = new(AssetBuildStatus.Failed, "The build didn't run");
            public JobHandle Done;
        }

        private readonly Queue<string> _buildQueue = new();
        private readonly HashSet<string> _queuedBuilds = new(StringComparer.OrdinalIgnoreCase);
        private readonly Dictionary<string, RunningBuild> _runningBuilds = new(StringComparer.OrdinalIgnoreCase);
        private readonly Dictionary<string, BackgroundTask> _buildTasks = new(StringComparer.OrdinalIgnoreCase);
        private readonly HashSet<string> _rebuildAfter = new(StringComparer.OrdinalIgnoreCase);
        private readonly Dictionary<string, (long Size, long Modified, long MetaModified)> _buildStamps = new(StringComparer.OrdinalIgnoreCase);
        // New or changed source files seen after startup, waiting until they stop being written.
        private readonly Dictionary<string, (long Size, long Modified, long Since)> _settling = new(StringComparer.OrdinalIgnoreCase);
        private long _lastSettleCheck;
        private bool _startupScanned;
        private Dictionary<string, IReadOnlyList<string>>? _importTags;
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
            MeshAssets.Register(Pipeline);
            ShaderAssets.Register(Pipeline, new DxcShaderCompiler(ProjectFiles.SdkDirectory));
            MaterialAssets.Register(Pipeline);
            ObjectAssets.Register(Pipeline);
            Sync();
            _startupScanned = true;
            Tags = new AssetTags(this);
            Bookmarks = Bookmarks.Load(projectRoot, this);
            Tags.Migrate();
            AssetRef.GuidResolver = GuidOf;
            SaveCache();
        }

        private static void MigrateMetas(string projectRoot)
        {
            string root = Path.Combine(projectRoot, AssetIndex.RootFolder);
            AssetMetaMigrationResult result = AssetMetaMigration.Run(root);
            if (result.Migrated + result.Unreadable > 0)
                Debug.Log($"[Assets] Migrated {result.Migrated + result.Unreadable} meta file(s) from {AssetMeta.LegacyExtension} to {AssetMeta.Extension}");
            foreach (string problem in result.Problems)
                Debug.LogWarning($"[Assets] Meta migration: {problem}");
            foreach ((string asset, string? problem) in AssetMetaMigration.EmbedSidecars(root))
            {
                if (problem is null)
                    Debug.Log($"[Assets] Moved the meta of {Path.GetRelativePath(projectRoot, asset).Replace('\\', '/')} into the file");
                else
                    Debug.LogWarning($"[Assets] Could not move the meta of {asset} into the file: {problem}");
            }
        }

        public string ProjectRoot { get; }
        public AssetIndex Index { get; }
        public AssetPipeline Pipeline { get; }
        public AssetTags Tags { get; }
        public Bookmarks Bookmarks { get; }
        // Changes whenever metas, tags or the index change.
        public int Version { get; private set; }
        // Changes after every asset build attempt.
        public int BuildVersion { get; private set; }

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

        // Changes whenever an asset's read-only import tags change.
        public int ImportTagVersion { get; private set; }

        // Read-only tags the last build of the asset produced ("@texture:bc1"), sorted.
        public IReadOnlyList<string> ImportTagsOf(string path) =>
            GuidOf(path) is string guid && ImportTagTable.TryGetValue(guid, out IReadOnlyList<string>? tags) ? tags : Array.Empty<string>();

        public IEnumerable<TagCount> ImportTagCounts =>
            ImportTagTable.Where(p => _paths.ContainsKey(p.Key)).SelectMany(p => p.Value)
                .GroupBy(t => t, StringComparer.OrdinalIgnoreCase).Select(g => new TagCount(g.Key, g.Count()));

        private Dictionary<string, IReadOnlyList<string>> ImportTagTable => _importTags ??= Pipeline.ReadAllTags();

        public string FullPath(string relPath) => Path.Combine(ProjectRoot, relPath.Replace('/', Path.DirectorySeparatorChar));

        public void Update()
        {
            if (Index.Refresh() || Index.Version != _syncedIndexVersion)
                Sync();
            ProcessOrphans();
            ProcessSettling();
            ProcessBuilds();
            ApplyHashes();
            if (_cacheDirty && Environment.TickCount64 - _cacheDirtySince >= CacheSaveDelayMilliseconds)
                SaveCache();
        }

        // Builds everything queued now and waits for it (builds still run on job workers).
        public void BuildAll()
        {
            foreach (string path in _settling.Keys)
                QueueBuild(path);
            _settling.Clear();
            ProcessBuilds();
            while (_runningBuilds.Count > 0)
            {
                Jobs.Wait(_runningBuilds.Values.First().Done);
                ProcessBuilds();
            }
        }

        // Asset builds in progress or queued.
        public int PendingBuilds => _runningBuilds.Count + _buildQueue.Count;

        // Stops a queued or running build. A running build stops at its next stage and keeps its last good output.
        public void CancelBuild(string path)
        {
            if (_runningBuilds.TryGetValue(path, out RunningBuild? running))
            {
                running.Cancel.Cancel();
                _rebuildAfter.Remove(path);
                return;
            }
            if (!_queuedBuilds.Remove(path))
                return;
            List<string> rest = _buildQueue.Where(p => !string.Equals(p, path, StringComparison.OrdinalIgnoreCase)).ToList();
            _buildQueue.Clear();
            foreach (string queued in rest)
                _buildQueue.Enqueue(queued);
            if (_buildTasks.Remove(path, out BackgroundTask? task))
                BackgroundTasks.Complete(task);
            Debug.Log($"[Assets] Canceled the queued build of {path}");
        }

        // Writes pending cache changes now (the editor calls it on shutdown), after waiting for running builds.
        public void Flush()
        {
            foreach (RunningBuild running in _runningBuilds.Values.ToList())
                Jobs.Wait(running.Done);
            ApplyHashes();
            if (_cacheDirty)
                SaveCache();
        }

        public void Refresh()
        {
            Index.Refresh(true);
            Sync();
        }

        // Queues a build of an asset whose file the editor just wrote (writing a file doesn't change its folder's time).
        public void RequestBuild(string path)
        {
            path = AssetIndex.Normalize(path);
            Refresh();
            if (_cache.ByPath.TryGetValue(path, out AssetCache.Record? record))
                record.Hash = null;
            _settling.Remove(path);
            if (_metas.ContainsKey(path) && Pipeline.CanImport(path))
                QueueBuild(path);
        }

        // Creates a new asset file with this text in a folder ("Assets/..."), adding " 1", " 2"... to the name when it's
        // taken. Returns the new asset's path, or null with a logged error.
        public string? CreateAsset(string folder, string name, string text)
        {
            folder = AssetIndex.Normalize(folder);
            string stem = Path.GetFileNameWithoutExtension(name);
            string extension = Path.GetExtension(name);
            string candidate = name;
            for (int i = 1; File.Exists(FullPath(folder + "/" + candidate)); i++)
                candidate = $"{stem} {i}{extension}";
            string path = folder + "/" + candidate;
            try
            {
                Directory.CreateDirectory(Path.GetDirectoryName(FullPath(path))!);
                File.WriteAllText(FullPath(path), text);
            }
            catch (Exception e) when (e is IOException or UnauthorizedAccessException)
            {
                Debug.LogError($"[Assets] Could not create {path}: {e.Message}");
                return null;
            }
            Refresh();
            return path;
        }

        // Replaces an asset's tags (normalized, sorted) in its meta file. Returns false when the asset has no meta.
        public bool SetTags(string path, IEnumerable<string> tags)
        {
            path = AssetIndex.Normalize(path);
            if (!_metas.ContainsKey(path))
                return false;
            string metaPath = MetaFileOf(path);
            AssetMeta meta;
            try
            {
                meta = AssetMetaStore.Read(FullPath(path));
            }
            catch (Exception e) when (e is IOException or JsonException or UnauthorizedAccessException)
            {
                Debug.LogError($"[Assets] Could not read {metaPath}: {e.Message}");
                return false;
            }
            meta.Tags = NormalizeTags(tags).ToList();
            try
            {
                AssetMetaStore.Write(FullPath(path), meta);
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

        // Replaces an asset's process settings in its meta file and queues a build. Returns false when that fails.
        public bool SetProcessSettings(string path, System.Text.Json.Nodes.JsonObject settings)
        {
            path = AssetIndex.Normalize(path);
            if (!_metas.ContainsKey(path) || AssetMetaStore.IsEmbedded(path))
                return false;
            string metaPath = MetaFileOf(path);
            try
            {
                AssetMeta meta = AssetMeta.Read(metaPath);
                meta.Importer["process"] = settings.DeepClone();
                meta.Write(metaPath);
                if (_cache.ByPath.TryGetValue(path, out AssetCache.Record? record))
                {
                    record.MetaModified = File.GetLastWriteTimeUtc(metaPath).Ticks;
                    MarkCacheDirty();
                }
            }
            catch (Exception e) when (e is IOException or JsonException or UnauthorizedAccessException)
            {
                Debug.LogError($"[Assets] Could not write {metaPath}: {e.Message}");
                return false;
            }
            Version++;
            if (Pipeline.CanImport(path))
                QueueBuild(path);
            return true;
        }

        // Stores an import result in the asset's meta: known items keep their chunk GUIDs, new ones get random GUIDs.
        // Returns null when the asset has no usable meta.
        public AssetItemUpdate? UpdateItems(string path, IEnumerable<ImportedAssetItem> imported)
        {
            path = AssetIndex.Normalize(path);
            if (!_metas.ContainsKey(path))
                return null;
            string metaPath = MetaFileOf(path);
            try
            {
                AssetMeta meta = AssetMetaStore.Read(FullPath(path));
                AssetItemUpdate update = AssetItemTable.Update(meta.Items, imported);
                foreach (string warning in update.Warnings)
                    Debug.LogWarning($"[Assets] {path}: {warning}");
                meta.Items = update.Items;
                if (AssetMetaStore.Write(FullPath(path), meta))
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

        // Creates a folder under a project folder ("Assets/..."). Returns null on success, otherwise the reason.
        public string? CreateFolder(string parent, string name, out string path)
        {
            parent = AssetIndex.Normalize(parent);
            name = name.Trim();
            path = parent + "/" + name;
            if (ValidateName(name) is string invalid)
                return invalid;
            if (!Index.IsDirectory(parent))
                return $"'{parent}' isn't a folder";
            string full = FullPath(path);
            if (Directory.Exists(full) || File.Exists(full))
                return $"'{name}' already exists";
            try
            {
                Directory.CreateDirectory(full);
            }
            catch (Exception e) when (e is IOException or UnauthorizedAccessException)
            {
                return e.Message;
            }
            Refresh();
            Debug.Log($"[Assets] Created folder {path}");
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
                .Select(e => new AssetFileState(e.RelPath, e.Size, e.ModifiedTicksUtc, AssetMetaStore.IsEmbedded(e.RelPath))).ToList();
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
            foreach (AssetFileState asset in assets.Where(a => a.Embedded))
            {
                if (ReadEmbeddedMeta(asset, broken) is not MetaInfo info)
                    continue;
                infos[asset.Path] = info;
                metas.Add(new AssetMetaState(asset.Path, info.Guid));
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
                                meta = AssetMetaStore.Read(FullPath(action.Path));
                            }
                            catch (JsonException)
                            {
                                meta = new AssetMeta();
                            }
                            string old = meta.Guid;
                            meta.Guid = AssetMeta.NewGuid();
                            AssetMetaStore.Write(FullPath(action.Path), meta);
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
                            Pipeline.WriteDefaultSettings(action.Path, meta);
                            AssetMetaStore.Write(FullPath(action.Path), meta);
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
                string metaFull = MetaFileOf(asset.Path);
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
                bool built = _buildStamps.TryGetValue(record.Path, out var stamp);
                if ((built && stamp == (record.Size, record.Modified, record.MetaModified)) || _queuedBuilds.Contains(record.Path))
                    continue;
                if (_startupScanned && (!built || stamp.Size != record.Size || stamp.Modified != record.Modified))
                {
                    if (!_settling.TryGetValue(record.Path, out var seen) || seen.Size != record.Size || seen.Modified != record.Modified)
                        _settling[record.Path] = (record.Size, record.Modified, Environment.TickCount64);
                    continue;
                }
                QueueBuild(record.Path);
            }
        }

        private void QueueBuild(string path)
        {
            if (_runningBuilds.ContainsKey(path))
            {
                _rebuildAfter.Add(path);
                return;
            }
            if (!_queuedBuilds.Add(path))
                return;
            _buildQueue.Enqueue(path);
            if (!_buildTasks.ContainsKey(path))
            {
                Icon icon = AssetIndex.KindOf(Path.GetExtension(path).TrimStart('.').ToLowerInvariant()) switch
                {
                    AssetKind.Image => Icon.Image,
                    AssetKind.Mesh => Icon.Mesh,
                    AssetKind.Material => Icon.Material,
                    AssetKind.Shader => Icon.Code,
                    AssetKind.Object => Icon.Object,
                    _ => Icon.File,
                };
                _buildTasks[path] = BackgroundTasks.Add("Import " + AssetNames.DisplayName(path), icon, () => CancelBuild(path));
            }
        }

        private void ProcessSettling()
        {
            long now = Environment.TickCount64;
            if (_settling.Count == 0 || now - _lastSettleCheck < SettleMilliseconds)
                return;
            _lastSettleCheck = now;
            bool changed = false;
            foreach ((string path, var seen) in _settling.ToList())
            {
                FileInfo file = new(FullPath(path));
                if (!file.Exists)
                {
                    _settling.Remove(path);
                }
                else if (file.Length != seen.Size || file.LastWriteTimeUtc.Ticks != seen.Modified)
                {
                    changed = true;
                }
                else if (now - seen.Since >= SettleMilliseconds && CanRead(file.FullName))
                {
                    _settling.Remove(path);
                    QueueBuild(path);
                }
            }
            if (changed)
                Refresh();
        }

        private static bool CanRead(string path)
        {
            try
            {
                using FileStream stream = new(path, FileMode.Open, FileAccess.Read, FileShare.Read);
                return true;
            }
            catch (Exception e) when (e is IOException or UnauthorizedAccessException)
            {
                return false;
            }
        }

        private static int MaxParallelBuilds => Math.Clamp(Jobs.WorkerCount - 1, 1, 4);

        private void ProcessBuilds()
        {
            while (_buildQueue.Count > 0 && _runningBuilds.Count < MaxParallelBuilds)
            {
                string path = _buildQueue.Dequeue();
                _queuedBuilds.Remove(path);
                StartBuild(path);
            }
            if (_buildQueue.Count > 0 || _runningBuilds.Count > 0)
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

        // Reads the meta on the main thread, runs Pipeline.Build on a job worker and applies the result in a main-thread
        // continuation. Item GUID updates write the meta, so the worker hands them to the main thread and waits.
        private void StartBuild(string path)
        {
            if (!_buildTasks.Remove(path, out BackgroundTask? task))
                task = BackgroundTasks.Add("Import " + AssetNames.DisplayName(path), Icon.File, () => CancelBuild(path));
            if (!_metas.TryGetValue(path, out MetaInfo? info) || !_cache.ByPath.TryGetValue(path, out AssetCache.Record? record))
            {
                BackgroundTasks.Complete(task);
                return;
            }
            AssetMeta meta;
            try
            {
                meta = AssetMetaStore.Read(FullPath(path));
            }
            catch (Exception e) when (e is IOException or JsonException or UnauthorizedAccessException)
            {
                BackgroundTasks.Complete(task);
                _buildsFailed++;
                Debug.LogError($"[Assets] Could not build {path}: {e.Message}");
                return;
            }

            RunningBuild running = new() { Guid = info.Guid, Task = task, Cancel = new CancellationTokenSource() };
            _runningBuilds[path] = running;
            task.Start();
            AssetBuildRequest request = new(info.Guid, FullPath(path), AssetMetaStore.IsEmbedded(path) ? null : record.Hash, meta.ImportSettings, meta.ProcessSettings, meta.Items,
                items =>
                {
                    List<ImportedAssetItem> imported = items.ToList();
                    AssetItemUpdate? update = null;
                    Jobs.Schedule(() => update = UpdateItems(path, imported), JobQueue.Main).Wait();
                    return update;
                },
                (fraction, stage) =>
                {
                    if (_logBuildThreads && stage == "Hashing")
                        Debug.Log($"[Assets] Building {path} on thread {Environment.CurrentManagedThreadId} (main thread {_mainThreadId})");
                    task.Report(fraction, stage);
                    if (_slowStageMilliseconds > 0)
                        Thread.Sleep(_slowStageMilliseconds);
                },
                running.Cancel.Token);
            JobHandle work = Jobs.Schedule(() => running.Result = Pipeline.Build(request));
            running.Done = Jobs.Schedule(() => FinishBuild(path, running), JobQueue.Main, work);
        }

        private void FinishBuild(string path, RunningBuild running)
        {
            _runningBuilds.Remove(path);
            BackgroundTasks.Complete(running.Task);
            running.Cancel.Dispose();
            AssetBuildResult result = running.Result;
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
                case AssetBuildStatus.Canceled:
                    Debug.Log($"[Assets] Canceled the build of {path}; its last good output is kept");
                    break;
            }
            if (result.Status is AssetBuildStatus.Imported or AssetBuildStatus.Processed or AssetBuildStatus.ManifestUpdated)
                RefreshImportTags(running.Guid);
            if (result.Status is AssetBuildStatus.Imported or AssetBuildStatus.Processed)
                Resources.Invalidate(running.Guid);
            if (result.Status != AssetBuildStatus.Canceled && _cache.ByPath.TryGetValue(path, out AssetCache.Record? record))
                _buildStamps[path] = (record.Size, record.Modified, record.MetaModified);
            BuildVersion++;
            if (_rebuildAfter.Remove(path) && _metas.ContainsKey(path))
                QueueBuild(path);
        }

        private void RefreshImportTags(string guid)
        {
            if (_importTags is null)
            {
                ImportTagVersion++;
                return;
            }
            List<string> tags = Pipeline.ManifestOf(guid)?.Tags ?? new List<string>();
            IReadOnlyList<string> old = _importTags.TryGetValue(guid, out IReadOnlyList<string>? stored) ? stored : Array.Empty<string>();
            if (old.SequenceEqual(tags))
                return;
            if (tags.Count == 0)
                _importTags.Remove(guid);
            else
                _importTags[guid] = tags;
            ImportTagVersion++;
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

        // The file whose write time stands for the asset's meta: its .xmd, or the asset itself when the meta is embedded.
        private string MetaFileOf(string path) => AssetMetaStore.IsEmbedded(path) ? FullPath(path) : FullPath(AssetMeta.PathFor(path));

        // The meta embedded in a composition (null with no block yet, which creates one; unreadable files go to broken).
        private MetaInfo? ReadEmbeddedMeta(AssetFileState asset, HashSet<string> broken)
        {
            if (_cache.ByPath.TryGetValue(asset.Path, out AssetCache.Record? record) && record.MetaModified == asset.ModifiedTicks && AssetMeta.IsValidGuid(record.Guid))
                return new MetaInfo(record.Guid, record.Tags);
            string full = FullPath(asset.Path);
            try
            {
                System.Text.Json.Nodes.JsonNode? root = System.Text.Json.Nodes.JsonNode.Parse(File.ReadAllText(full),
                    documentOptions: new JsonDocumentOptions { CommentHandling = JsonCommentHandling.Skip, AllowTrailingCommas = true });
                if (root is not System.Text.Json.Nodes.JsonObject document)
                    throw new JsonException("not a JSON object");
                _reportedBroken.Remove(asset.Path);
                if (document[AssetMetaStore.EmbeddedKey] is not System.Text.Json.Nodes.JsonObject block)
                    return null;
                AssetMeta meta = AssetMeta.FromJSON(block);
                return new MetaInfo(meta.Guid, NormalizeTags(meta.Tags).ToList());
            }
            catch (Exception e) when (e is IOException or JsonException or UnauthorizedAccessException)
            {
                broken.Add(asset.Path);
                if (_reportedBroken.Add(asset.Path))
                    Debug.LogError($"[Assets] Could not read {asset.Path}: {e.Message}. Fix it; it is left untouched.");
                return null;
            }
        }

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
