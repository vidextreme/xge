// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.

using xg.editor;
using xg.imui;
using xg.math;

namespace xgEditor.CoreCLR
{
    public enum AssetKind
    {
        Folder,
        Script,
        Image,
        Material,
        Mesh,
        Audio,
        Scene,
        Text,
        Other,
    }

    public sealed class AssetEntry
    {
        internal AssetEntry(string relPath, string fullPath, AssetKind kind, DateTime modified, long size = 0)
        {
            RelPath = relPath;
            FullPath = fullPath;
            Kind = kind;
            Modified = modified;
            Size = size;
            int slash = relPath.LastIndexOf('/');
            Name = slash < 0 ? relPath : relPath[(slash + 1)..];
            Parent = slash < 0 ? string.Empty : relPath[..slash];
            Extension = kind == AssetKind.Folder ? string.Empty : Path.GetExtension(Name).TrimStart('.').ToLowerInvariant();
        }

        // Project-relative path with forward slashes, starting with "Assets".
        public string RelPath { get; }
        public string FullPath { get; }
        public string Name { get; }
        public string Parent { get; }
        public string Extension { get; }
        public AssetKind Kind { get; }
        public DateTime Modified { get; }
        public long ModifiedTicksUtc => Modified.ToUniversalTime().Ticks;
        // File size in bytes (0 for folders).
        public long Size { get; }
        public bool IsFolder => Kind == AssetKind.Folder;
    }

    // Everything under the project's Assets folder, rescanned when a directory changes (checked at most once a second)
    // or on request. Version changes after every rescan so views can rebuild their caches. Meta files never become
    // entries (the Navigator never sees them); MetaFiles lists them for the asset database. Hidden names (".x") and
    // temporary files ("x~", "x.tmp") are skipped.
    public sealed class AssetIndex
    {
        public const string RootFolder = "Assets";

        private static readonly Dictionary<string, AssetKind> _kinds = new(StringComparer.OrdinalIgnoreCase)
        {
            ["cs"] = AssetKind.Script,
            ["png"] = AssetKind.Image, ["bmp"] = AssetKind.Image, ["tga"] = AssetKind.Image, ["jpg"] = AssetKind.Image,
            ["jpeg"] = AssetKind.Image, ["psd"] = AssetKind.Image, ["dds"] = AssetKind.Image, ["tif"] = AssetKind.Image,
            ["tiff"] = AssetKind.Image, ["gif"] = AssetKind.Image,
            ["mat"] = AssetKind.Material, ["material"] = AssetKind.Material,
            ["fbx"] = AssetKind.Mesh, ["obj"] = AssetKind.Mesh, ["gltf"] = AssetKind.Mesh, ["glb"] = AssetKind.Mesh,
            ["dae"] = AssetKind.Mesh, ["blend"] = AssetKind.Mesh, ["mesh"] = AssetKind.Mesh, ["mdl"] = AssetKind.Mesh,
            ["wav"] = AssetKind.Audio, ["ogg"] = AssetKind.Audio, ["mp3"] = AssetKind.Audio, ["flac"] = AssetKind.Audio,
            ["scene"] = AssetKind.Scene, ["xgscene"] = AssetKind.Scene,
            ["txt"] = AssetKind.Text, ["md"] = AssetKind.Text, ["json"] = AssetKind.Text, ["xml"] = AssetKind.Text,
            ["yaml"] = AssetKind.Text, ["yml"] = AssetKind.Text, ["csv"] = AssetKind.Text, ["ini"] = AssetKind.Text,
        };

        private readonly List<AssetEntry> _entries = new();
        private readonly Dictionary<string, AssetEntry> _byPath = new(StringComparer.OrdinalIgnoreCase);
        private readonly Dictionary<string, List<AssetEntry>> _children = new(StringComparer.OrdinalIgnoreCase);
        private readonly List<string> _directories = new();
        private readonly List<string> _metaFiles = new();
        private DateTime _stamp;
        private long _lastCheck;

        public AssetIndex(string projectRoot)
        {
            ProjectRoot = projectRoot;
            Rescan();
        }

        public string ProjectRoot { get; }
        public string AssetsPath => Path.Combine(ProjectRoot, RootFolder);
        public int Version { get; private set; }
        public IReadOnlyList<AssetEntry> Entries => _entries;
        // Every directory path ("Assets", "Assets/Scripts", ...) in path order.
        public IReadOnlyList<string> Directories => _directories;
        // Project-relative paths of every meta file ("Assets/Sword.png.xmd").
        public IReadOnlyList<string> MetaFiles => _metaFiles;

        public AssetEntry? Find(string relPath) => _byPath.TryGetValue(Normalize(relPath), out AssetEntry? entry) ? entry : null;

        public bool IsDirectory(string relPath)
        {
            string path = Normalize(relPath);
            return string.Equals(path, RootFolder, StringComparison.OrdinalIgnoreCase) || (Find(path)?.IsFolder ?? false);
        }

        public IReadOnlyList<AssetEntry> Children(string folder) =>
            _children.TryGetValue(Normalize(folder), out List<AssetEntry>? list) ? list : Array.Empty<AssetEntry>();

        public bool HasSubfolders(string folder)
        {
            foreach (AssetEntry child in Children(folder))
                if (child.IsFolder)
                    return true;
            return false;
        }

        public static string Normalize(string path)
        {
            string result = path.Replace('\\', '/').Trim().TrimEnd('/');
            return result;
        }

        public static AssetKind KindOf(string extension) =>
            _kinds.TryGetValue(extension, out AssetKind kind) ? kind : AssetKind.Other;

        public static Icon IconOf(AssetKind kind) => kind switch
        {
            AssetKind.Folder => Icon.Folder,
            AssetKind.Script => Icon.Script,
            AssetKind.Image => Icon.Image,
            AssetKind.Material => Icon.Material,
            AssetKind.Mesh => Icon.Mesh,
            AssetKind.Audio => Icon.Audio,
            AssetKind.Scene => Icon.Scene,
            _ => Icon.File,
        };

        public static Color32 IconColorOf(AssetKind kind) => kind switch
        {
            AssetKind.Folder => Style.Color(StyleColor.IconObject),
            AssetKind.Script => Style.Color(StyleColor.IconData),
            AssetKind.Image => Style.Color(StyleColor.IconMaterial),
            AssetKind.Material => Style.Color(StyleColor.IconMaterial),
            AssetKind.Mesh => Style.Color(StyleColor.IconComponent),
            AssetKind.Scene => Style.Color(StyleColor.IconWarning),
            _ => Style.Color(StyleColor.Icon),
        };

        public static string PluralName(AssetKind kind) => kind switch
        {
            AssetKind.Folder => "Folders",
            AssetKind.Script => "Scripts",
            AssetKind.Image => "Images",
            AssetKind.Material => "Materials",
            AssetKind.Mesh => "Meshes",
            AssetKind.Audio => "Audio",
            AssetKind.Scene => "Scenes",
            AssetKind.Text => "Text",
            _ => "Other",
        };

        // Rescans when forced, or when a directory's write time changed since the last scan (checked once a second).
        public bool Refresh(bool force = false)
        {
            long now = Environment.TickCount64;
            if (!force && now - _lastCheck < 1000)
                return false;
            _lastCheck = now;
            if (!force && ComputeStamp() == _stamp)
                return false;
            Rescan();
            return true;
        }

        private DateTime ComputeStamp()
        {
            DateTime stamp = DateTime.MinValue;
            try
            {
                if (!Directory.Exists(AssetsPath))
                    return stamp;
                stamp = Directory.GetLastWriteTimeUtc(AssetsPath);
                foreach (string dir in Directory.EnumerateDirectories(AssetsPath, "*", SearchOption.AllDirectories))
                {
                    DateTime time = Directory.GetLastWriteTimeUtc(dir);
                    if (time > stamp)
                        stamp = time;
                }
            }
            catch (Exception e) when (e is IOException or UnauthorizedAccessException)
            {
            }
            return stamp;
        }

        private void Rescan()
        {
            _entries.Clear();
            _byPath.Clear();
            _children.Clear();
            _directories.Clear();
            _metaFiles.Clear();
            _directories.Add(RootFolder);
            _children[RootFolder] = new List<AssetEntry>();
            _stamp = ComputeStamp();
            if (Directory.Exists(AssetsPath))
                Scan(AssetsPath, RootFolder);
            foreach (List<AssetEntry> list in _children.Values)
                list.Sort(CompareEntries);
            _directories.Sort(StringComparer.OrdinalIgnoreCase);
            Version++;
        }

        private void Scan(string fullDir, string relDir)
        {
            IEnumerable<string> dirs;
            IEnumerable<string> files;
            try
            {
                dirs = Directory.GetDirectories(fullDir);
                files = Directory.GetFiles(fullDir);
            }
            catch (Exception e) when (e is IOException or UnauthorizedAccessException)
            {
                return;
            }

            foreach (string dir in dirs)
            {
                string name = Path.GetFileName(dir);
                if (name.StartsWith('.') || name.Equals("obj", StringComparison.OrdinalIgnoreCase) || name.Equals("bin", StringComparison.OrdinalIgnoreCase))
                    continue;
                string rel = relDir + "/" + name;
                Add(new AssetEntry(rel, dir, AssetKind.Folder, Directory.GetLastWriteTime(dir)));
                _directories.Add(rel);
                _children[rel] = new List<AssetEntry>();
                Scan(dir, rel);
            }

            foreach (string file in files)
            {
                string name = Path.GetFileName(file);
                if (name.StartsWith('.') || name.EndsWith('~') || name.EndsWith(".tmp", StringComparison.OrdinalIgnoreCase))
                    continue;
                if (AssetMeta.IsMetaPath(name))
                {
                    _metaFiles.Add(relDir + "/" + name);
                    continue;
                }
                if (AssetMeta.IsLegacyMetaPath(name))
                    continue;
                FileInfo info = new(file);
                Add(new AssetEntry(relDir + "/" + name, file, KindOf(Path.GetExtension(name).TrimStart('.')), info.LastWriteTime, info.Length));
            }
        }

        private void Add(AssetEntry entry)
        {
            _entries.Add(entry);
            _byPath[entry.RelPath] = entry;
            if (_children.TryGetValue(entry.Parent, out List<AssetEntry>? list))
                list.Add(entry);
        }

        private static int CompareEntries(AssetEntry a, AssetEntry b)
        {
            if (a.IsFolder != b.IsFolder)
                return a.IsFolder ? -1 : 1;
            return string.Compare(a.Name, b.Name, StringComparison.OrdinalIgnoreCase);
        }
    }
}
