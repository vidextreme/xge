// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.

namespace xg.editor
{
    // A selected project asset: its project-relative path with forward slashes, starting with "Assets".
    public readonly record struct AssetRef(string Path)
    {
        public string Name => System.IO.Path.GetFileName(Path);

        public string FullPath => System.IO.Path.Combine(ScriptRuntime.ProjectPath, Path);

        public override string ToString() => Path;
    }
}
