// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.

namespace xg.editor
{
    // A selected project asset: its project-relative path with forward slashes, starting with "Assets". Guid is the
    // asset's meta GUID (empty for folders or when no project is open), resolved through GuidResolver, which the
    // editor's asset database installs.
    public readonly record struct AssetRef(string Path)
    {
        public static Func<string, string?>? GuidResolver { get; set; }

        public string Guid => GuidResolver?.Invoke(Path) ?? string.Empty;

        public string Name => System.IO.Path.GetFileName(Path);

        public string FullPath => System.IO.Path.Combine(ScriptRuntime.ProjectPath, Path);

        public override string ToString() => Path;
    }
}
