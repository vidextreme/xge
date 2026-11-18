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

    // A generated file before item GUIDs are applied. Store = false keeps a raw file in memory only.
    public sealed record AssetDataFile(string Extension, uint Kind, List<AssetDataItem> Items, bool Store = true);

    // Reads a source file and returns its raw data. Item order must be stable for the same source.
    public interface IAssetImporter
    {
        string Id { get; }
        int Version { get; }
        bool Accepts(string extension);
        IReadOnlyList<AssetDataFile> Import(string sourcePath, JsonObject settings);
    }

    // Turns one raw file into one processed file. Output items use the same kinds, paths and order as the input.
    public interface IAssetProcessor
    {
        string Id { get; }
        int Version { get; }
        bool Accepts(string rawExtension);
        AssetDataFile Process(AssetDataFile raw, JsonObject settings);
    }

    public sealed record AssetBuildRequest(string Guid, string SourcePath, string? SourceHash, JsonObject ImportSettings, JsonObject ProcessSettings,
        IReadOnlyList<AssetItem> StoredItems, Func<IEnumerable<ImportedAssetItem>, AssetItemUpdate?> UpdateItems);

    public enum AssetBuildStatus
    {
        NotImportable,
        UpToDate,
        Imported,
        Processed,
        ManifestUpdated,
        Failed,
        PreviouslyFailed,
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

        [JsonPropertyName("error")]
        [JsonPropertyOrder(9)]
        [JsonIgnore(Condition = JsonIgnoreCondition.WhenWritingNull)]
        public string? Error { get; set; }

        [JsonPropertyName("errorKey")]
        [JsonPropertyOrder(10)]
        [JsonIgnore(Condition = JsonIgnoreCondition.WhenWritingNull)]
        public string? ErrorKey { get; set; }
    }

    // Builds Library data for source assets. Raw data is keyed by source hash, import settings and importer version;
    // processed data by raw hash, process settings and processor versions. Output goes to <root>/<prefix>/<guid>/, is
    // written to a temporary folder first, and manifest.json is written last, so an interrupted build simply runs again.
    public sealed class AssetPipeline
    {
        private const string ManifestName = "manifest.json";
        private const string TempSuffix = ".build~";

        private readonly List<IAssetImporter> _importers = new();
        private readonly List<IAssetProcessor> _processors = new();

        public AssetPipeline(string root) => Root = root;

        public string Root { get; }

        public void Register(IAssetImporter importer) => _importers.Add(importer);
        public void Register(IAssetProcessor processor) => _processors.Add(processor);

        public bool CanImport(string path) => FindImporter(Path.GetExtension(path)) is not null;

        public string DirectoryOf(string guid)
        {
            string lower = guid.ToLowerInvariant();
            return Path.Combine(Root, lower[..2], lower);
        }

        // The generated file with this extension, or null when it hasn't been built.
        public string? FindFile(string guid, string extension)
        {
            string directory = DirectoryOf(guid);
            AssetBuildManifest? manifest = LoadManifest(directory);
            string? name = manifest?.Files.Concat(manifest.RawFiles).FirstOrDefault(f => f.EndsWith(extension, StringComparison.OrdinalIgnoreCase));
            if (name is null)
                return null;
            string path = Path.Combine(directory, name);
            return File.Exists(path) ? path : null;
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
            string directory = DirectoryOf(guid);
            AssetBuildManifest? old = LoadManifest(directory);
            string? inputKey = null;
            try
            {
                string sourceHash = request.SourceHash ?? HashFile(request.SourcePath) ?? throw new IOException($"Could not read {request.SourcePath}");
                string importSettings = Canonical(request.ImportSettings);
                string processSettings = Canonical(request.ProcessSettings);
                string processors = string.Join(",", _processors.Select(p => $"{p.Id}@{p.Version}").Order(StringComparer.Ordinal));
                string importKey = Key("import", ChunkFile.Version.ToString(), sourceHash, importer.Id, importer.Version.ToString(), importSettings);
                inputKey = Key("input", importKey, processSettings, processors);

                if (old?.ErrorKey == inputKey)
                    return new AssetBuildResult(AssetBuildStatus.PreviouslyFailed, old.Error);
                if (old is { Error: null } && old.InputKey == inputKey && AllExist(directory, old.Files.Concat(old.RawFiles)))
                    return new AssetBuildResult(AssetBuildStatus.UpToDate);

                string sourceName = Path.GetFileName(request.SourcePath);
                Dictionary<(string Kind, string Path, int Occurrence), string> guids;
                List<AssetDataFile>? rawFiles = LoadStoredRaw(directory, old, importKey, request.StoredItems);
                bool imported = rawFiles is null;
                if (rawFiles is null)
                {
                    rawFiles = importer.Import(request.SourcePath, request.ImportSettings).ToList();
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
                    string name = sourceName + raw.Extension;
                    rawNames.Add(name);
                    output[name] = bytes;
                }
                string rawHash = Convert.ToHexStringLower(rawHasher.GetHashAndReset());
                string processKey = Key("process", rawHash, processSettings, processors);

                List<string> processedNames;
                bool reuseProcessed = old is not null && old.ProcessKey == processKey && AllExist(directory, old.Files);
                if (reuseProcessed)
                {
                    processedNames = old!.Files;
                }
                else
                {
                    processedNames = new List<string>();
                    foreach (AssetDataFile raw in rawFiles)
                    {
                        IAssetProcessor processor = _processors.FirstOrDefault(p => p.Accepts(raw.Extension))
                            ?? throw new InvalidDataException($"No processor accepts {raw.Extension}");
                        AssetDataFile processed = processor.Process(raw, request.ProcessSettings);
                        string name = sourceName + processed.Extension;
                        processedNames.Add(name);
                        output[name] = ToChunkFile(processed, guid, guids).ToBytes();
                    }
                }

                Commit(directory, output, rawNames.Concat(processedNames));
                SaveManifest(directory, new AssetBuildManifest
                {
                    Source = sourceName,
                    InputKey = inputKey,
                    ImportKey = importKey,
                    RawHash = rawHash,
                    ProcessKey = processKey,
                    RawFiles = rawNames,
                    RawInMemory = rawFiles.Any(f => !f.Store),
                    Files = processedNames,
                });
                if (!reuseProcessed)
                    return new AssetBuildResult(imported ? AssetBuildStatus.Imported : AssetBuildStatus.Processed);
                return new AssetBuildResult(AssetBuildStatus.ManifestUpdated);
            }
            catch (Exception e)
            {
                AssetBuildManifest failed = old ?? new AssetBuildManifest { Source = Path.GetFileName(request.SourcePath) };
                failed.Error = e.Message;
                failed.ErrorKey = inputKey;
                try
                {
                    if (inputKey is not null)
                        SaveManifest(directory, failed);
                }
                catch (Exception saveError) when (saveError is IOException or UnauthorizedAccessException)
                {
                    return new AssetBuildResult(AssetBuildStatus.Failed, $"{e.Message} (the failure could not be recorded: {saveError.Message})");
                }
                return new AssetBuildResult(AssetBuildStatus.Failed, e.Message);
            }
        }

        // Deletes generated folders whose GUID isn't in the set. Returns how many were removed.
        public int RemoveOrphans(IReadOnlySet<string> liveGuids)
        {
            if (!Directory.Exists(Root))
                return 0;
            int removed = 0;
            foreach (string prefix in Directory.GetDirectories(Root))
            {
                if (Path.GetFileName(prefix).Length != 2)
                    continue;
                foreach (string directory in Directory.GetDirectories(prefix))
                {
                    string name = Path.GetFileName(directory);
                    bool temporary = name.EndsWith(TempSuffix, StringComparison.Ordinal);
                    string guid = temporary ? name[..^TempSuffix.Length] : name;
                    if (!AssetMeta.IsValidGuid(guid) || (!temporary && liveGuids.Contains(guid)))
                        continue;
                    Directory.Delete(directory, true);
                    if (!temporary)
                        removed++;
                }
                if (!Directory.EnumerateFileSystemEntries(prefix).Any())
                    Directory.Delete(prefix);
            }
            return removed;
        }

        private IAssetImporter? FindImporter(string extension) => _importers.FirstOrDefault(i => i.Accepts(extension));

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

        private static List<AssetDataFile>? LoadStoredRaw(string directory, AssetBuildManifest? manifest, string importKey, IReadOnlyList<AssetItem> storedItems)
        {
            if (manifest is null || manifest.ImportKey != importKey || manifest.RawInMemory || manifest.RawFiles.Count == 0 || !AllExist(directory, manifest.RawFiles))
                return null;
            Dictionary<string, AssetItem> byGuid = storedItems.Where(i => !i.Missing).ToDictionary(i => i.Guid, StringComparer.OrdinalIgnoreCase);
            List<AssetDataFile> files = new();
            try
            {
                foreach (string name in manifest.RawFiles)
                {
                    ChunkFile chunkFile = ChunkFile.Parse(File.ReadAllBytes(Path.Combine(directory, name)));
                    List<AssetDataItem> items = new();
                    foreach (ChunkItem item in chunkFile.Items)
                    {
                        if (!byGuid.TryGetValue(item.Guid, out AssetItem? stored))
                            return null;
                        items.Add(new AssetDataItem(stored.Kind, stored.Path, item.Chunks));
                    }
                    files.Add(new AssetDataFile(RawExtension(name, manifest.Source), chunkFile.Kind, items));
                }
            }
            catch (InvalidDataException)
            {
                return null;
            }
            return files;
        }

        private static string RawExtension(string fileName, string sourceName) =>
            fileName.StartsWith(sourceName, StringComparison.OrdinalIgnoreCase) ? fileName[sourceName.Length..] : Path.GetExtension(fileName);

        private static void Commit(string directory, Dictionary<string, byte[]> output, IEnumerable<string> keep)
        {
            string temp = directory + TempSuffix;
            if (Directory.Exists(temp))
                Directory.Delete(temp, true);
            Directory.CreateDirectory(temp);
            foreach ((string name, byte[] bytes) in output)
                File.WriteAllBytes(Path.Combine(temp, name), bytes);

            Directory.CreateDirectory(directory);
            foreach (string name in output.Keys)
                File.Move(Path.Combine(temp, name), Path.Combine(directory, name), true);
            Directory.Delete(temp, true);

            HashSet<string> kept = new(keep, StringComparer.OrdinalIgnoreCase) { ManifestName };
            foreach (string file in Directory.GetFiles(directory))
                if (!kept.Contains(Path.GetFileName(file)))
                    File.Delete(file);
        }

        private static bool AllExist(string directory, IEnumerable<string> names) =>
            names.All(name => File.Exists(Path.Combine(directory, name)));

        private static AssetBuildManifest? LoadManifest(string directory)
        {
            string path = Path.Combine(directory, ManifestName);
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

        private static void SaveManifest(string directory, AssetBuildManifest manifest) =>
            JSONFile.Write(Path.Combine(directory, ManifestName), manifest);

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
