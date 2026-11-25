// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.

using System.Security.Cryptography;
using System.Text;
using System.Text.Json;
using System.Text.Json.Nodes;
using System.Text.Json.Serialization;

namespace xg.editor
{
    public sealed record AssetDataItem(string Kind, string Path, List<ChunkData> Chunks);

    // A generated file before item GUIDs are applied. Store = false keeps a raw file in memory only. Tags of processed
    // files ("@texture:bc1") are read-only import tags recorded in the asset's manifest.
    public sealed record AssetDataFile(string Extension, uint Kind, List<AssetDataItem> Items, bool Store = true, IReadOnlyList<string>? Tags = null);

    // Reads a source file and returns its raw data. Item order must be stable for the same source.
    public interface IAssetImporter
    {
        string Id { get; }
        int Version { get; }
        // Library/Data subfolder of this importer's files ("Textures").
        string DataFolder { get; }
        // Extension of the asset's main generated file (".xdt"); it also names the manifest ("<guid>.xdt.json").
        string DataExtension { get; }
        bool Accepts(string extension);
        IReadOnlyList<AssetDataFile> Import(string sourcePath, JsonObject settings);

        // Fills the settings of a newly created meta file.
        void WriteDefaultSettings(JsonObject import, JsonObject process) { }

        // The source's hash for the build key, or null to hash the whole file (compositions hash their content without
        // the embedded meta, so tag edits rebuild nothing).
        string? HashSource(string sourcePath) => null;
    }

    // Turns one raw file into one processed file. Output items use the same kinds, paths and order as the input.
    public interface IAssetProcessor
    {
        string Id { get; }
        int Version { get; }
        bool Accepts(string rawExtension);
        AssetDataFile Process(AssetDataFile raw, JsonObject settings);
    }

    // Progress (optional) receives the build's fraction (0..1) and stage name; it may be called from a worker thread.
    // Cancel stops the build between stages: it returns Canceled and keeps the last good output.
    public sealed record AssetBuildRequest(string Guid, string SourcePath, string? SourceHash, JsonObject ImportSettings, JsonObject ProcessSettings,
        IReadOnlyList<AssetItem> StoredItems, Func<IEnumerable<ImportedAssetItem>, AssetItemUpdate?> UpdateItems,
        Action<float, string>? Progress = null, CancellationToken Cancel = default);

    public enum AssetBuildStatus
    {
        NotImportable,
        UpToDate,
        Imported,
        Processed,
        ManifestUpdated,
        Failed,
        PreviouslyFailed,
        Canceled,
    }

    public sealed record AssetBuildResult(AssetBuildStatus Status, string? Error = null);

    public sealed class AssetBuildManifest
    {
        public const int CurrentVersion = 1;

        [JsonPropertyName("version")]
        [JsonPropertyOrder(0)]
        public int Version { get; set; } = CurrentVersion;

        [JsonPropertyName("source")]
        [JsonPropertyOrder(1)]
        public string Source { get; set; } = string.Empty;

        [JsonPropertyName("inputKey")]
        [JsonPropertyOrder(2)]
        public string InputKey { get; set; } = string.Empty;

        [JsonPropertyName("importKey")]
        [JsonPropertyOrder(3)]
        public string ImportKey { get; set; } = string.Empty;

        [JsonPropertyName("rawHash")]
        [JsonPropertyOrder(4)]
        public string RawHash { get; set; } = string.Empty;

        [JsonPropertyName("processKey")]
        [JsonPropertyOrder(5)]
        public string ProcessKey { get; set; } = string.Empty;

        // Stored raw files. Raw data kept only in memory is marked by RawInMemory.
        [JsonPropertyName("rawFiles")]
        [JsonPropertyOrder(6)]
        public List<string> RawFiles { get; set; } = new();

        [JsonPropertyName("rawInMemory")]
        [JsonPropertyOrder(7)]
        [JsonIgnore(Condition = JsonIgnoreCondition.WhenWritingDefault)]
        public bool RawInMemory { get; set; }

        [JsonPropertyName("files")]
        [JsonPropertyOrder(8)]
        public List<string> Files { get; set; } = new();

        // Read-only import tags of the processed data, lower case, in the order the processors gave them.
        [JsonPropertyName("tags")]
        [JsonPropertyOrder(9)]
        public List<string> Tags { get; set; } = new();

        [JsonPropertyName("error")]
        [JsonPropertyOrder(10)]
        [JsonIgnore(Condition = JsonIgnoreCondition.WhenWritingNull)]
        public string? Error { get; set; }

        [JsonPropertyName("errorKey")]
        [JsonPropertyOrder(11)]
        [JsonIgnore(Condition = JsonIgnoreCondition.WhenWritingNull)]
        public string? ErrorKey { get; set; }
    }

    // Builds Library data for source assets. Raw data is keyed by source hash, import settings and importer version;
    // processed data by raw hash, process settings and processor versions. Output goes to <root>/<type folder>/<guid><ext>,
    // is written to temporary files first, and the manifest <guid><main ext>.json is written last, so an interrupted build
    // simply runs again.
    public sealed class AssetPipeline
    {
        private const string ManifestSuffix = ".json";
        private const string TempSuffix = ".build~";
        private const int GuidLength = 32;
        private const int LegacyPrefixLength = 2;

        private readonly List<IAssetImporter> _importers = new();
        private readonly List<IAssetProcessor> _processors = new();

        public AssetPipeline(string root) => Root = root;

        public string Root { get; }

        public void Register(IAssetImporter importer) => _importers.Add(importer);
        public void Register(IAssetProcessor processor) => _processors.Add(processor);

        public bool CanImport(string path) => FindImporter(Path.GetExtension(path)) is not null;

        public void WriteDefaultSettings(string path, AssetMeta meta) =>
            FindImporter(Path.GetExtension(path))?.WriteDefaultSettings(meta.ImportSettings, meta.ProcessSettings);

        // The generated file with this extension, or null when it hasn't been built.
        public string? FindFile(string guid, string extension)
        {
            if (FindManifest(guid) is not { } found)
                return null;
            string? name = found.Manifest.Files.Concat(found.Manifest.RawFiles).FirstOrDefault(f => f.EndsWith(extension, StringComparison.OrdinalIgnoreCase));
            if (name is null)
                return null;
            string path = Path.Combine(found.Folder, name);
            return File.Exists(path) ? path : null;
        }

        public AssetBuildManifest? ManifestOf(string guid) => FindManifest(guid)?.Manifest;

        // The read-only import tags of every built asset, by GUID (assets without tags are left out).
        public Dictionary<string, IReadOnlyList<string>> ReadAllTags()
        {
            Dictionary<string, IReadOnlyList<string>> result = new(StringComparer.OrdinalIgnoreCase);
            foreach (string folder in _importers.Select(i => Path.Combine(Root, i.DataFolder)).Distinct(StringComparer.OrdinalIgnoreCase))
            {
                if (!Directory.Exists(folder))
                    continue;
                foreach (string path in Directory.EnumerateFiles(folder, "*" + ManifestSuffix))
                {
                    string name = Path.GetFileName(path);
                    if (name.Length <= GuidLength || name[GuidLength] != '.' || !AssetMeta.IsValidGuid(name[..GuidLength]))
                        continue;
                    if (LoadManifest(path) is { Error: null, Tags.Count: > 0 } manifest)
                        result[name[..GuidLength]] = manifest.Tags;
                }
            }
            return result;
        }

        public static string? HashFile(string path)
        {
            try
            {
                using FileStream stream = File.OpenRead(path);
                return Convert.ToHexStringLower(SHA1.HashData(stream));
            }
            catch (Exception e) when (e is IOException or UnauthorizedAccessException)
            {
                return null;
            }
        }

        public AssetBuildResult Build(AssetBuildRequest request)
        {
            IAssetImporter? importer = FindImporter(Path.GetExtension(request.SourcePath));
            if (importer is null)
                return new AssetBuildResult(AssetBuildStatus.NotImportable);

            string guid = request.Guid.ToLowerInvariant();
            string folder = Path.Combine(Root, importer.DataFolder);
            string manifestPath = Path.Combine(folder, guid + importer.DataExtension + ManifestSuffix);
            string sourceName = Path.GetFileName(request.SourcePath);
            AssetBuildManifest? old = LoadManifest(manifestPath);
            string? inputKey = null;
            void Stage(float fraction, string stage)
            {
                request.Cancel.ThrowIfCancellationRequested();
                request.Progress?.Invoke(fraction, stage);
            }
            try
            {
                Stage(0.02f, "Hashing");
                string sourceHash = request.SourceHash ?? importer.HashSource(request.SourcePath) ?? HashFile(request.SourcePath) ?? throw new IOException($"Could not read {request.SourcePath}");
                string importSettings = Canonical(request.ImportSettings);
                string processSettings = Canonical(request.ProcessSettings);
                string processors = string.Join(",", _processors.Select(p => $"{p.Id}@{p.Version}").Order(StringComparer.Ordinal));
                string importKey = Key("import", ChunkFile.Version.ToString(), sourceHash, importer.Id, importer.Version.ToString(), importSettings);
                inputKey = Key("input", importKey, processSettings, processors);

                if (old?.ErrorKey == inputKey)
                    return new AssetBuildResult(AssetBuildStatus.PreviouslyFailed, old.Error);
                if (old is { Error: null } && old.InputKey == inputKey && AllExist(folder, old.Files.Concat(old.RawFiles)))
                {
                    if (old.Source != sourceName)
                    {
                        old.Source = sourceName;
                        SaveManifest(manifestPath, old);
                    }
                    return new AssetBuildResult(AssetBuildStatus.UpToDate);
                }

                Dictionary<(string Kind, string Path, int Occurrence), string> guids;
                List<AssetDataFile>? rawFiles = LoadStoredRaw(folder, old, importKey, request.StoredItems);
                bool imported = rawFiles is null;
                if (rawFiles is null)
                {
                    Stage(0.1f, "Importing");
                    rawFiles = importer.Import(request.SourcePath, request.ImportSettings).ToList();
                    Stage(0.55f, "Assigning items");
                    guids = AssignItems(rawFiles, request);
                }
                else
                {
                    guids = request.StoredItems.Where(i => !i.Missing).ToDictionary(i => (i.Kind, i.Path, i.Occurrence), i => i.Guid);
                }

                Dictionary<string, byte[]> output = new(StringComparer.OrdinalIgnoreCase);
                List<string> rawNames = new();
                using IncrementalHash rawHasher = IncrementalHash.CreateHash(HashAlgorithmName.SHA256);
                foreach (AssetDataFile raw in rawFiles)
                {
                    byte[] bytes = ToChunkFile(raw, guid, guids).ToBytes();
                    rawHasher.AppendData(Encoding.UTF8.GetBytes(raw.Extension));
                    rawHasher.AppendData(bytes);
                    if (!raw.Store)
                        continue;
                    string name = guid + raw.Extension;
                    rawNames.Add(name);
                    output[name] = bytes;
                }
                string rawHash = Convert.ToHexStringLower(rawHasher.GetHashAndReset());
                string processKey = Key("process", rawHash, processSettings, processors);

                List<string> processedNames;
                List<string> tags;
                bool reuseProcessed = old is not null && old.ProcessKey == processKey && AllExist(folder, old.Files);
                if (reuseProcessed)
                {
                    processedNames = old!.Files;
                    tags = old.Tags ?? new List<string>();
                }
                else
                {
                    processedNames = new List<string>();
                    List<string> collected = new();
                    for (int r = 0; r < rawFiles.Count; r++)
                    {
                        AssetDataFile raw = rawFiles[r];
                        Stage(0.6f + 0.3f * r / rawFiles.Count, $"Processing {raw.Extension}");
                        IAssetProcessor processor = _processors.FirstOrDefault(p => p.Accepts(raw.Extension))
                            ?? throw new InvalidDataException($"No processor accepts {raw.Extension}");
                        AssetDataFile processed = processor.Process(raw, request.ProcessSettings);
                        string name = guid + processed.Extension;
                        processedNames.Add(name);
                        output[name] = ToChunkFile(processed, guid, guids).ToBytes();
                        if (processed.Tags is not null)
                            collected.AddRange(processed.Tags.Where(t => t.Length > 0).Select(t => t.ToLowerInvariant()));
                    }
                    tags = collected.Distinct(StringComparer.Ordinal).ToList();
                }

                Stage(0.9f, "Writing");
                Commit(folder, guid, manifestPath, output, rawNames.Concat(processedNames));
                SaveManifest(manifestPath, new AssetBuildManifest
                {
                    Source = sourceName,
                    InputKey = inputKey,
                    ImportKey = importKey,
                    RawHash = rawHash,
                    ProcessKey = processKey,
                    RawFiles = rawNames,
                    RawInMemory = rawFiles.Any(f => !f.Store),
                    Files = processedNames,
                    Tags = tags,
                });
                if (!reuseProcessed)
                    return new AssetBuildResult(imported ? AssetBuildStatus.Imported : AssetBuildStatus.Processed);
                return new AssetBuildResult(AssetBuildStatus.ManifestUpdated);
            }
            catch (OperationCanceledException) when (request.Cancel.IsCancellationRequested)
            {
                return new AssetBuildResult(AssetBuildStatus.Canceled);
            }
            catch (Exception e)
            {
                AssetBuildManifest failed = old ?? new AssetBuildManifest { Source = sourceName };
                failed.Error = e.Message;
                failed.ErrorKey = inputKey;
                try
                {
                    if (inputKey is not null)
                        SaveManifest(manifestPath, failed);
                }
                catch (Exception saveError) when (saveError is IOException or UnauthorizedAccessException)
                {
                    return new AssetBuildResult(AssetBuildStatus.Failed, $"{e.Message} (the failure could not be recorded: {saveError.Message})");
                }
                return new AssetBuildResult(AssetBuildStatus.Failed, e.Message);
            }
        }

        // Deletes generated files whose GUID isn't in the set, leftover temporary files and folders of the old
        // <prefix>/<guid>/ layout. Returns how many assets lost their data.
        public int RemoveOrphans(IReadOnlySet<string> liveGuids)
        {
            if (!Directory.Exists(Root))
                return 0;
            int removed = 0;
            foreach (string folder in Directory.GetDirectories(Root))
            {
                string folderName = Path.GetFileName(folder);
                if (folderName.Length == LegacyPrefixLength && folderName.All(char.IsAsciiHexDigit))
                {
                    Directory.Delete(folder, true);
                    continue;
                }
                HashSet<string> orphans = new(StringComparer.OrdinalIgnoreCase);
                foreach (string file in Directory.GetFiles(folder))
                {
                    string name = Path.GetFileName(file);
                    if (name.Length <= GuidLength || name[GuidLength] != '.')
                        continue;
                    string guid = name[..GuidLength];
                    bool temporary = name.EndsWith(TempSuffix, StringComparison.Ordinal);
                    if (!AssetMeta.IsValidGuid(guid) || (!temporary && liveGuids.Contains(guid)))
                        continue;
                    File.Delete(file);
                    if (!temporary)
                        orphans.Add(guid);
                }
                removed += orphans.Count;
                if (!Directory.EnumerateFileSystemEntries(folder).Any())
                    Directory.Delete(folder);
            }
            return removed;
        }

        private IAssetImporter? FindImporter(string extension) => _importers.FirstOrDefault(i => i.Accepts(extension));

        private (string Folder, AssetBuildManifest Manifest)? FindManifest(string guid)
        {
            string pattern = guid.ToLowerInvariant() + ".*" + ManifestSuffix;
            foreach (string folder in _importers.Select(i => Path.Combine(Root, i.DataFolder)).Distinct(StringComparer.OrdinalIgnoreCase))
            {
                if (!Directory.Exists(folder))
                    continue;
                foreach (string path in Directory.EnumerateFiles(folder, pattern))
                    if (LoadManifest(path) is AssetBuildManifest manifest)
                        return (folder, manifest);
            }
            return null;
        }

        private static Dictionary<(string, string, int), string> AssignItems(List<AssetDataFile> rawFiles, AssetBuildRequest request)
        {
            List<AssetDataItem> items = rawFiles.SelectMany(f => f.Items).ToList();
            if (items.Any(i => string.IsNullOrEmpty(i.Kind)))
                throw new InvalidDataException("The importer produced an item without a kind");
            AssetItemUpdate update = request.UpdateItems(items.Select(i => new ImportedAssetItem(i.Kind, i.Path)))
                ?? throw new IOException("The asset's meta file could not be updated");
            return update.Assigned.ToDictionary(a => (a.Kind, a.Path, a.Occurrence), a => a.Guid);
        }

        private static ChunkFile ToChunkFile(AssetDataFile source, string guid, Dictionary<(string, string, int), string> guids)
        {
            ChunkFile file = new() { Kind = source.Kind, FileGuid = guid };
            Dictionary<(string, string), int> counts = new();
            foreach (AssetDataItem item in source.Items)
            {
                counts.TryGetValue((item.Kind, item.Path), out int occurrence);
                counts[(item.Kind, item.Path)] = occurrence + 1;
                if (!guids.TryGetValue((item.Kind, item.Path, occurrence), out string? chunkGuid))
                    throw new InvalidDataException($"Item {item.Kind} '{item.Path}' has no GUID");
                file.Items.Add(new ChunkItem(chunkGuid, item.Chunks));
            }
            return file;
        }

        private static List<AssetDataFile>? LoadStoredRaw(string folder, AssetBuildManifest? manifest, string importKey, IReadOnlyList<AssetItem> storedItems)
        {
            if (manifest is null || manifest.ImportKey != importKey || manifest.RawInMemory || manifest.RawFiles.Count == 0 || !AllExist(folder, manifest.RawFiles))
                return null;
            Dictionary<string, AssetItem> byGuid = storedItems.Where(i => !i.Missing).ToDictionary(i => i.Guid, StringComparer.OrdinalIgnoreCase);
            List<AssetDataFile> files = new();
            try
            {
                foreach (string name in manifest.RawFiles)
                {
                    ChunkFile chunkFile = ChunkFile.Parse(File.ReadAllBytes(Path.Combine(folder, name)));
                    List<AssetDataItem> items = new();
                    foreach (ChunkItem item in chunkFile.Items)
                    {
                        if (!byGuid.TryGetValue(item.Guid, out AssetItem? stored))
                            return null;
                        items.Add(new AssetDataItem(stored.Kind, stored.Path, item.Chunks));
                    }
                    files.Add(new AssetDataFile(Path.GetExtension(name), chunkFile.Kind, items));
                }
            }
            catch (InvalidDataException)
            {
                return null;
            }
            return files;
        }

        private static void Commit(string folder, string guid, string manifestPath, Dictionary<string, byte[]> output, IEnumerable<string> keep)
        {
            Directory.CreateDirectory(folder);
            foreach ((string name, byte[] bytes) in output)
                File.WriteAllBytes(Path.Combine(folder, name + TempSuffix), bytes);

            File.Delete(manifestPath);
            foreach (string name in output.Keys)
                File.Move(Path.Combine(folder, name + TempSuffix), Path.Combine(folder, name), true);

            HashSet<string> kept = new(keep, StringComparer.OrdinalIgnoreCase);
            foreach (string file in Directory.GetFiles(folder, guid + ".*"))
                if (!kept.Contains(Path.GetFileName(file)))
                    File.Delete(file);
        }

        private static bool AllExist(string folder, IEnumerable<string> names) =>
            names.All(name => File.Exists(Path.Combine(folder, name)));

        private static AssetBuildManifest? LoadManifest(string path)
        {
            if (!File.Exists(path))
                return null;
            try
            {
                AssetBuildManifest? manifest = JSONFile.Read<AssetBuildManifest>(path);
                return manifest?.Version == AssetBuildManifest.CurrentVersion ? manifest : null;
            }
            catch (Exception e) when (e is IOException or JsonException or UnauthorizedAccessException)
            {
                return null;
            }
        }

        private static void SaveManifest(string path, AssetBuildManifest manifest) => JSONFile.Write(path, manifest);

        private static string Key(params string[] parts)
        {
            using IncrementalHash hash = IncrementalHash.CreateHash(HashAlgorithmName.SHA256);
            foreach (string part in parts)
            {
                byte[] bytes = Encoding.UTF8.GetBytes(part);
                hash.AppendData(BitConverter.GetBytes(bytes.Length));
                hash.AppendData(bytes);
            }
            return Convert.ToHexStringLower(hash.GetHashAndReset());
        }

        private static string Canonical(JsonNode? node)
        {
            switch (node)
            {
                case JsonObject value:
                    return "{" + string.Join(",", value.OrderBy(p => p.Key, StringComparer.Ordinal).Select(p => JsonSerializer.Serialize(p.Key) + ":" + Canonical(p.Value))) + "}";
                case JsonArray value:
                    return "[" + string.Join(",", value.Select(Canonical)) + "]";
                case null:
                    return "null";
                default:
                    return node.ToJsonString();
            }
        }
    }
}
