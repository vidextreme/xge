// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.

using System.Text.Json;

namespace xgEditor.CoreCLR
{
    // A game project folder: <Root>/<Name>.xgproject (JSON manifest), Assets/, ProjectSettings/ and the generated
    // <Name>.csproj, <Name>.slnx and Library/ (build output and generated sources).
    public sealed class ProjectInfo
    {
        public const string ManifestExtension = ".xgproject";

        private ProjectInfo(string root, string manifestPath, string name)
        {
            Root = root;
            ManifestPath = manifestPath;
            Name = name;
        }

        public string Root { get; }
        public string ManifestPath { get; }
        public string Name { get; }

        public string AssetsPath => Path.Combine(Root, "Assets");
        public string LibraryPath => Path.Combine(Root, "Library");
        public string GeneratedPath => Path.Combine(LibraryPath, "Generated");
        public string CsprojPath => Path.Combine(Root, Name + ".csproj");
        public string SlnxPath => Path.Combine(Root, Name + ".slnx");
        public string ScriptAssemblyName => Name + ".CoreCLR";
        public string ScriptAssemblyPath => Path.Combine(LibraryPath, "Build", "Editor", ScriptAssemblyName + ".dll");
        // Editor-only scripts (Assets/**/Editor/**) build into a second assembly that only the editor loads.
        public string EditorCsprojPath => Path.Combine(Root, Name + ".Editor.csproj");
        public string EditorAssemblyName => Name + ".Editor";
        public string EditorAssemblyPath => Path.Combine(LibraryPath, "Build", "Editor", EditorAssemblyName + ".dll");
        // Record of the inputs (path, time, size) of the last successful script build.
        public string InputsStampPath => Path.Combine(LibraryPath, "Build", "Editor", "inputs.txt");
        // Present while the latest script build failed; the kernel then notes it is loading the last good build.
        public string BuildFailedMarkerPath => Path.Combine(LibraryPath, "Build", "Editor", "build-failed");

        public static bool IsEditorScript(string assetsPath, string file)
        {
            string relative = Path.GetRelativePath(assetsPath, Path.GetDirectoryName(file) ?? assetsPath);
            foreach (string part in relative.Split(Path.DirectorySeparatorChar, Path.AltDirectorySeparatorChar))
                if (string.Equals(part, "Editor", StringComparison.OrdinalIgnoreCase))
                    return true;
            return false;
        }

        public bool HasEditorScripts =>
            Directory.Exists(AssetsPath) &&
            Directory.EnumerateFiles(AssetsPath, "*.cs", SearchOption.AllDirectories).Any(f => IsEditorScript(AssetsPath, f));

        // Files whose changes require rebuilding the script assemblies: scripts, manifest, generated project files, SDK.
        public IEnumerable<string> ScriptInputs()
        {
            if (Directory.Exists(AssetsPath))
            {
                foreach (string script in Directory.EnumerateFiles(AssetsPath, "*.cs", SearchOption.AllDirectories))
                    yield return script;
            }
            yield return ManifestPath;
            yield return CsprojPath;
            yield return EditorCsprojPath;
            if (Directory.Exists(GeneratedPath))
            {
                foreach (string generated in Directory.EnumerateFiles(GeneratedPath, "*.cs", SearchOption.AllDirectories))
                    yield return generated;
            }
            string lib = Path.Combine(ProjectFiles.SdkDirectory, "lib");
            if (Directory.Exists(lib))
            {
                foreach (string dll in Directory.EnumerateFiles(lib, "*.dll"))
                    yield return dll;
            }
        }

        // Returns null when root has no manifest or the manifest is unreadable.
        public static ProjectInfo? Load(string root)
        {
            if (!Directory.Exists(root))
                return null;
            string? manifest = Directory.EnumerateFiles(root, "*" + ManifestExtension).FirstOrDefault();
            if (manifest is null)
                return null;

            string name = Path.GetFileNameWithoutExtension(manifest);
            try
            {
                using JsonDocument document = JsonDocument.Parse(File.ReadAllText(manifest));
                if (document.RootElement.TryGetProperty("Name", out JsonElement value) && value.ValueKind == JsonValueKind.String)
                    name = value.GetString() ?? name;
            }
            catch (Exception e) when (e is IOException or JsonException or UnauthorizedAccessException)
            {
                xg.Debug.LogError($"[Project] Cannot read {manifest}: {e.Message}");
                return null;
            }

            return new ProjectInfo(Path.GetFullPath(root), manifest, ToIdentifier(name));
        }

        // Turns a folder or display name into a C# identifier usable as namespace and assembly name.
        public static string ToIdentifier(string name)
        {
            Span<char> buffer = stackalloc char[Math.Max(1, name.Length)];
            int length = 0;
            foreach (char c in name)
            {
                if (char.IsLetterOrDigit(c) || c == '_')
                    buffer[length++] = c;
            }
            if (length == 0)
                return "Game";
            string result = new(buffer[..length]);
            return char.IsDigit(result[0]) ? "_" + result : result;
        }
    }
}
