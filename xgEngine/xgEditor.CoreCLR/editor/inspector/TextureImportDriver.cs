// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.

using System.Text.Json;
using xg;
using xg.editor;

namespace xgEditor.CoreCLR
{
    // The data of the Texture Import Inspector tool for one image asset: the decoded source info, a small preview (as
    // pixels; views make the GPU texture), the settings draft, and the last build's result. Apply writes the draft to the
    // asset's .xmd and queues a build; Revert drops it. No UI, so batch tools can drive it too.
    public sealed class TextureImportDriver : IToolDriver
    {
        private const int PreviewSize = 128;

        private int _buildVersion = -1;

        public TextureImportDriver(string path)
        {
            Path = path;
            Load();
            TestLog("created");
        }

        public string Path { get; }
        public string? Error { get; private set; }
        public int Width { get; private set; }
        public int Height { get; private set; }
        public bool HasAlpha { get; private set; }
        public long FileSize { get; private set; }
        public TextureImportDraft? Draft { get; private set; }
        public byte[]? PreviewPixels { get; private set; }
        public int PreviewWidth { get; private set; }
        public int PreviewHeight { get; private set; }
        public string? BuildError { get; private set; }
        public long GeneratedSize { get; private set; }

        public bool HasPendingChanges => Draft?.IsDirty == true;

        public void Apply()
        {
            if (Draft is not { IsDirty: true } || ProjectService.Assets is not { } assets)
                return;
            try
            {
                AssetMeta meta = AssetMeta.Read(assets.FullPath(AssetMeta.PathFor(Path)));
                var process = meta.ProcessSettings;
                Draft.Draft.WriteTo(process);
                if (assets.SetProcessSettings(Path, process))
                    Draft.MarkApplied();
            }
            catch (Exception e) when (e is IOException or JsonException or UnauthorizedAccessException)
            {
                Debug.LogError($"[Inspector] Could not save the settings of {Path}: {e.Message}");
            }
        }

        public void Revert() => Draft?.Revert();

        // Reads the generated size and build error again after a build.
        public void RefreshBuildInfo()
        {
            if (ProjectService.Assets is not { } assets || _buildVersion == assets.BuildVersion)
                return;
            _buildVersion = assets.BuildVersion;
            string? guid = assets.GuidOf(Path);
            BuildError = guid is null ? null : assets.Pipeline.ManifestOf(guid)?.Error;
            string? file = guid is null ? null : assets.Pipeline.FindFile(guid, TextureAssets.ProcessedExtension);
            GeneratedSize = file is null ? 0 : new FileInfo(file).Length;
        }

        public void Dispose()
        {
            TestLog("disposed");
            PreviewPixels = null;
            Draft = null;
        }

        private void TestLog(string what)
        {
            if (!string.IsNullOrEmpty(Environment.GetEnvironmentVariable("XG_UI_TEST_INSPECT_ASSET")))
                Debug.Log($"[Debug] Texture import driver {what}: {Path}");
        }

        private void Load()
        {
            if (ProjectService.Assets is not { } assets)
            {
                Error = "No project is open.";
                return;
            }
            try
            {
                string full = assets.FullPath(Path);
                byte[]? pixels = ImageFile.LoadRGBA(full, out int width, out int height);
                if (pixels is null)
                {
                    Error = "This image can't be decoded.";
                    return;
                }
                Width = width;
                Height = height;
                HasAlpha = TextureFormats.HasAlpha(pixels);
                FileSize = new FileInfo(full).Length;
                TextureSettings saved = TextureSettings.From(AssetMeta.Read(assets.FullPath(AssetMeta.PathFor(Path))).ProcessSettings);
                Draft = new TextureImportDraft(saved);
                string? testDraft = Environment.GetEnvironmentVariable("XG_UI_TEST_TEXTURE_DRAFT");
                if (!string.IsNullOrEmpty(testDraft))
                    Draft.Draft = saved.WithText(testDraft);
                MakePreview(pixels);
                ApplyTestTags(assets);
            }
            catch (Exception e) when (e is IOException or JsonException or UnauthorizedAccessException)
            {
                Error = e.Message;
            }
        }

        private void MakePreview(byte[] pixels)
        {
            int step = Math.Max(1, Math.Max(Width, Height) / PreviewSize);
            int w = Math.Max(1, Width / step);
            int h = Math.Max(1, Height / step);
            byte[] preview = new byte[w * h * 4];
            for (int y = 0; y < h; y++)
                for (int x = 0; x < w; x++)
                    Buffer.BlockCopy(pixels, (y * step * Width + x * step) * 4, preview, (y * w + x) * 4, 4);
            PreviewPixels = preview;
            PreviewWidth = w;
            PreviewHeight = h;
        }

        private void ApplyTestTags(AssetDatabase assets)
        {
            foreach (string tag in (Environment.GetEnvironmentVariable("XG_UI_TEST_TEXTURE_TAGS") ?? string.Empty).Split(',', StringSplitOptions.RemoveEmptyEntries))
            {
                if (tag.StartsWith('-'))
                    assets.Tags.Remove(Path, tag[1..]);
                else
                    assets.Tags.Add(Path, tag);
            }
        }
    }
}
