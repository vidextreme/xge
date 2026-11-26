// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.

namespace xg.editor
{
    // One built-in asset: its meta GUID, display path ("engine/materials/default.xcm"), Navigator kind name ("material")
    // and the source file.
    public sealed record EngineAsset(string Guid, string Path, string Kind, string FullPath)
    {
        public string Name => AssetNames.DisplayName(Path);
    }

    // The engine's built-in assets (gameroot/engine/assets, built into gameroot/engine/data by xgEngineData), read-only
    // for the editor: shaders, materials, primitive meshes and textures that projects reference by GUID like their own.
    // Scanned once from the committed metas.
    public static class EngineAssets
    {
        public const string Prefix = "engine/";

        private static readonly object _lock = new();
        private static List<EngineAsset>? _assets;
        private static Dictionary<string, EngineAsset> _byGuid = new(StringComparer.OrdinalIgnoreCase);
        private static Dictionary<string, EngineAsset> _byPath = new(StringComparer.OrdinalIgnoreCase);
        private static readonly string[] _kindNames = { "shader", "material", "mesh", "image", "object" };
        private const string InternalFolder = Prefix + "materials/";
        private const string ErrorShaderPath = Prefix + "shaders/surface.error.hlsl";

        // gameroot/engine/assets, next to the gameroot/bin folder the editor runs from.
        public static string Directory
        {
            get
            {
                string bin = System.IO.Path.GetDirectoryName(typeof(EngineAssets).Assembly.Location) ?? AppContext.BaseDirectory;
                return System.IO.Path.GetFullPath(System.IO.Path.Combine(bin, "..", "engine", "assets"));
            }
        }

        public static IReadOnlyList<EngineAsset> All => Load();

        public static string DefaultMaterial => GuidOf("engine/materials/default.xcm") ?? string.Empty;
        public static string UnlitMaterial => GuidOf("engine/materials/unlit.xcm") ?? string.Empty;
        public static string ErrorMaterial => GuidOf("engine/materials/error.xcm") ?? string.Empty;
        public static string LitShader => GuidOf("engine/shaders/surface.lit.hlsl") ?? string.Empty;
        public static string UnlitShader => GuidOf("engine/shaders/surface.unlit.hlsl") ?? string.Empty;
        public static string Cube => GuidOf("engine/meshes/cube.xcp") ?? string.Empty;
        public static string Sphere => GuidOf("engine/meshes/sphere.xcp") ?? string.Empty;
        public static string Plane => GuidOf("engine/meshes/plane.xcp") ?? string.Empty;
        public static string Quad => GuidOf("engine/meshes/quad.xcp") ?? string.Empty;
        public static string WhiteTexture => GuidOf("engine/textures/white.png") ?? string.Empty;

        public static bool IsEngine(string? guid)
        {
            Load();
            return !string.IsNullOrEmpty(guid) && _byGuid.ContainsKey(guid);
        }

        public static bool IsEnginePath(string? path) => path is not null && path.StartsWith(Prefix, StringComparison.OrdinalIgnoreCase);

        // The engine's own materials (default, unlit, error) and the error shader: drawn by the engine itself (None draws
        // the default material, a broken material the error one), never listed, assigned or edited.
        public static bool IsInternal(string? guidOrPath) => Find(guidOrPath) is { } asset && IsInternalPath(asset.Path);

        private static bool IsInternalPath(string path) =>
            path.StartsWith(InternalFolder, StringComparison.OrdinalIgnoreCase) || path.Equals(ErrorShaderPath, StringComparison.OrdinalIgnoreCase);

        public static string? PathOf(string? guid)
        {
            Load();
            return !string.IsNullOrEmpty(guid) && _byGuid.TryGetValue(guid, out EngineAsset? asset) ? asset.Path : null;
        }

        public static string? GuidOf(string path)
        {
            Load();
            return _byPath.TryGetValue(path.Replace('\\', '/'), out EngineAsset? asset) ? asset.Guid : null;
        }

        public static EngineAsset? Find(string? guidOrPath)
        {
            if (string.IsNullOrEmpty(guidOrPath))
                return null;
            Load();
            return _byGuid.TryGetValue(guidOrPath, out EngineAsset? byGuid) ? byGuid : _byPath.GetValueOrDefault(guidOrPath.Replace('\\', '/'));
        }

        // Built-in assets of a Navigator kind name ("material", "mesh", "shader", "image"; anything else lists all), sorted
        // by path, without the internal assets.
        public static IReadOnlyList<AssetChoice> List(string kind)
        {
            bool any = _kindNames.All(k => !k.Equals(kind, StringComparison.OrdinalIgnoreCase));
            return Load().Where(a => !IsInternalPath(a.Path) && (any || a.Kind.Equals(kind, StringComparison.OrdinalIgnoreCase)))
                .Select(a => new AssetChoice(a.Guid, a.Path, a.Name)).ToList();
        }

        // The Navigator kind name of a file extension, for the kinds built-ins use.
        public static string KindOf(string extension) => extension.TrimStart('.').ToLowerInvariant() switch
        {
            "hlsl" or "hlsli" => "shader",
            "xcm" => "material",
            "xcp" or "fbx" => "mesh",
            "png" or "bmp" => "image",
            "xco" => "object",
            _ => "other",
        };

        // Rescans the folder (after the engine data was rebuilt).
        public static void Refresh()
        {
            lock (_lock)
                _assets = null;
        }

        private static List<EngineAsset> Load()
        {
            lock (_lock)
            {
                if (_assets is not null)
                    return _assets;
                List<EngineAsset> assets = new();
                Dictionary<string, EngineAsset> byGuid = new(StringComparer.OrdinalIgnoreCase);
                Dictionary<string, EngineAsset> byPath = new(StringComparer.OrdinalIgnoreCase);
                string root = Directory;
                if (System.IO.Directory.Exists(root))
                {
                    foreach (string file in System.IO.Directory.EnumerateFiles(root, "*", SearchOption.AllDirectories).Order(StringComparer.OrdinalIgnoreCase))
                    {
                        if (AssetMeta.IsMetaPath(file) || KindOf(System.IO.Path.GetExtension(file)) == "other")
                            continue;
                        if (!AssetMetaStore.TryRead(file, out AssetMeta? meta) || meta is null || !AssetMeta.IsValidGuid(meta.Guid) || byGuid.ContainsKey(meta.Guid))
                            continue;
                        string path = Prefix + System.IO.Path.GetRelativePath(root, file).Replace('\\', '/');
                        EngineAsset asset = new(meta.Guid.ToLowerInvariant(), path, KindOf(System.IO.Path.GetExtension(file)), file);
                        assets.Add(asset);
                        byGuid[asset.Guid] = asset;
                        byPath[asset.Path] = asset;
                    }
                }
                _byGuid = byGuid;
                _byPath = byPath;
                _assets = assets;
                return assets;
            }
        }
    }
}
