// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.

using System.Text.Json;
using System.Text.Json.Nodes;
using xg;
using xg.editor;

namespace xgEditor.CoreCLR
{
    // The data of one .xcm material being edited: the saved asset, the draft, and the annotations of the draft's shader
    // (read from the shader's source, refreshed when the file changes). Apply writes the .xcm and the name tree in its
    // .xmd (the paths of the assets it references), then queues a build; Revert drops the draft. No UI, so batch tools
    // and the future .xco editor can drive it too.
    public sealed class MaterialDriver : IToolDriver
    {
        private string? _annotatedShader;
        private long _annotatedTime;

        public MaterialDriver(string path)
        {
            Path = path;
            Load();
        }

        public string Path { get; private set; }
        public string? Error { get; private set; }
        public MaterialAsset Saved { get; private set; } = new();
        public MaterialAsset Draft { get; private set; } = new();
        public ShaderAnnotations? Annotations { get; private set; }
        public string? ShaderPath { get; private set; }
        // Bumped whenever the draft changes, so views know when to update their preview material.
        public int DraftVersion { get; private set; }

        public string Name => System.IO.Path.GetFileNameWithoutExtension(Path);
        public string? Guid => ProjectService.Assets?.GuidOf(Path);
        public bool HasPendingChanges => !Draft.SameAs(Saved);

        public void Load()
        {
            Error = null;
            if (ProjectService.Assets is not { } assets)
            {
                Error = "No project is open.";
                return;
            }
            try
            {
                Saved = MaterialAsset.Read(assets.FullPath(Path));
                Draft = Saved.Clone();
                RepairReferences(assets);
            }
            catch (Exception e) when (e is IOException or JsonException or InvalidDataException or UnauthorizedAccessException)
            {
                Error = $"This material can't be read: {e.Message}";
                Saved = new MaterialAsset();
                Draft = new MaterialAsset();
            }
            Changed();
        }

        public void Changed()
        {
            DraftVersion++;
            RefreshAnnotations();
        }

        public void SetShader(string guid)
        {
            Draft.Shader = guid;
            Changed();
        }

        public void SetParam(string name, float[] value)
        {
            Draft.Params[name] = value;
            Changed();
        }

        public void SetTexture(string name, string guid)
        {
            if (string.IsNullOrEmpty(guid))
                Draft.Textures.Remove(name);
            else
                Draft.Textures[name] = guid;
            Changed();
        }

        // The draft's value of a param, or the shader's default.
        public float[] ParamValue(ShaderParam param) =>
            Draft.Params.TryGetValue(param.Name, out float[]? value) ? value : (float[])param.Default.Clone();

        public string TextureGuid(ShaderTexture texture) =>
            Draft.Textures.TryGetValue(texture.Name, out string? id) ? id.Split('/')[0] : string.Empty;

        // Re-reads the shader's annotations when the draft's shader or its source file changed.
        public void RefreshAnnotations()
        {
            AssetDatabase? assets = ProjectService.Assets;
            string? path = assets is null || string.IsNullOrEmpty(Draft.Shader) ? null : assets.PathOf(Draft.Shader);
            ShaderPath = path;
            if (path is null || assets is null)
            {
                Annotations = null;
                _annotatedShader = null;
                return;
            }
            string full = assets.FullPath(path);
            long time = File.Exists(full) ? File.GetLastWriteTimeUtc(full).Ticks : 0;
            if (_annotatedShader == path && _annotatedTime == time)
                return;
            _annotatedShader = path;
            _annotatedTime = time;
            try
            {
                Annotations = ShaderAnnotations.Parse(File.ReadAllText(full));
            }
            catch (IOException e)
            {
                Annotations = null;
                Debug.LogWarning($"[Materials] Could not read {path}: {e.Message}");
            }
        }

        public void Apply() => SaveTo(Path);

        public void Revert()
        {
            Draft = Saved.Clone();
            Changed();
        }

        // Writes the draft to another asset path ("Assets/..."), which this driver then edits. The copy gets a new GUID.
        public bool SaveAs(string path)
        {
            if (!SaveTo(path, true))
                return false;
            Path = path;
            return true;
        }

        public void Dispose()
        {
        }

        // The meta block is the file's current one (tags or item GUIDs may have been written since the material was
        // loaded) with the draft's name tree; a copy (Save as) gets a fresh GUID.
        private bool SaveTo(string path, bool copy = false)
        {
            if (ProjectService.Assets is not { } assets)
                return false;
            if (EngineAssets.IsEnginePath(path))
            {
                Debug.LogWarning($"[Materials] {path} is a built-in asset and can't be changed; use Save As to copy it into Assets");
                return false;
            }
            string full = assets.FullPath(path);
            MaterialAsset written = Draft.Clone();
            written.Meta = CurrentMeta(full) ?? written.Meta?.DeepClone().AsObject() ?? new JsonObject();
            if (copy || written.Meta["guid"] is null)
                written.Meta = new JsonObject { ["guid"] = AssetMeta.NewGuid(), ["tags"] = written.Meta["tags"]?.DeepClone() ?? new JsonArray() };
            Dictionary<string, string> names = new(StringComparer.Ordinal);
            foreach (string guid in written.ReferencedGuids())
                if (assets.PathOf(guid) is string referenced)
                    names[guid] = referenced;
            written.SetNames(names);
            try
            {
                written.Write(full);
            }
            catch (Exception e) when (e is IOException or UnauthorizedAccessException)
            {
                Debug.LogError($"[Materials] Could not save {path}: {e.Message}");
                return false;
            }
            assets.RequestBuild(path);
            Draft.Meta = written.Meta.DeepClone().AsObject();
            Saved = Draft.Clone();
            Changed();
            return true;
        }

        private static JsonObject? CurrentMeta(string full)
        {
            if (!File.Exists(full))
                return null;
            try
            {
                return MaterialAsset.Read(full).Meta;
            }
            catch (Exception e) when (e is IOException or JsonException or InvalidDataException)
            {
                return null;
            }
        }

        // A referenced GUID that no longer resolves is looked up by the path the name tree recorded; when an asset is
        // there, the draft points to it (a pending change, so saving keeps the fix).
        private void RepairReferences(AssetDatabase assets)
        {
            foreach ((string guid, string recorded) in Draft.Names())
            {
                if (assets.PathOf(guid) is not null)
                    continue;
                if (assets.GuidOf(recorded) is string found && found != guid && Draft.ReplaceGuid(guid, found))
                    Debug.LogWarning($"[Materials] {Path}: {recorded} has a new GUID; the material now points to it (save to keep the fix)");
            }
        }
    }
}
