// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.

using xg;
using xg.editor;
using xg.imui;
using xg.math;

namespace xgEditor.CoreCLR
{
    // Import settings of the selected texture asset. The asset's data and settings draft are a TextureImportDriver (kept
    // while the asset stays selected; Apply writes the .xmd and queues a build); the preview texture and the tag field
    // are each Inspector's TextureImportViewState.
    [InspectorTool("Texture Import", Icon = "Image", Priority = 0, Tooltip = "How the selected image is built into a texture")]
    public sealed class TextureImportInspectorTool : IInspectorTool
    {
        private const float PreviewHeight = 104.0f;
        private const float TypeRowHeight = 24.0f;
        private const float ActionRowHeight = 22.0f;

        public ToolUsability OnSelectItems(object[] selection) =>
            selection.LastOrDefault() is AssetRef asset && ProjectService.Assets is { } assets && assets.Pipeline.CanImport(asset.Path)
                && AssetIndex.KindOf(Path.GetExtension(asset.Path).TrimStart('.')) == AssetKind.Image
                ? ToolUsability.Usable
                : ToolUsability.NotApplicable;

        public object? DriverKey(ToolSelection selection) => selection.ActiveObject as AssetRef?;

        public IToolDriver? CreateDriver(ToolSelection selection) =>
            selection.ActiveObject is AssetRef asset ? new TextureImportDriver(asset.Path) : null;

        public void OnInspectorGUI(InspectorToolContext context)
        {
            TextureImportViewState view = context.View.State<TextureImportViewState>();
            TextureImportDriver? driver = context.Driver as TextureImportDriver;
            view.Show(driver);
            using var area = Gui.ScrollArea("##texture_import", default, ScrollAreaFlags.Padded);
            if (!area.IsOpen)
                return;
            Vector2 avail = Gui.AvailableSize;
            Gui.Draw.RectFilled(new Rect(new Vector2(-6.0f, -6.0f), new Vector2(avail.x + 6.0f, 100000.0f)), Style.Color(StyleColor.ToolbarBg));

            using (Gui.Horizontal())
            {
                using (Gui.Font(FontStyle.Bold))
                    Gui.Label(driver?.Path is string texturePath ? AssetNames.DisplayName(texturePath) : "Texture");
                Gui.FlexibleSpace();
                context.DrawLock();
            }
            Gui.Space(2.0f);
            if (driver?.Draft is not { } draft)
            {
                Gui.Label(driver?.Error ?? "This texture can't be read.");
                return;
            }

            driver.RefreshBuildInfo();
            int width = driver.Width;
            int height = driver.Height;
            TextureSettings settings = draft.Draft;
            TextureBuildPlan plan = TextureFormats.Plan(settings, width, height, driver.HasAlpha);
            Color32 iconColor = Style.Color(StyleColor.Icon);

            using (Gui.Card("##configuration", "Configuration", Icon.View, null, iconColor))
            {
                Gui.PropertyLabel("Default", plan.Format.ToString());
                settings = Choice("Format", settings, settings.Format, (s, v) => s with { Format = v });
            }
            Gui.Space(2.0f);

            using (Gui.Card("##type", "Texture type", Icon.Image, null, iconColor))
            {
                settings = DrawType(settings);
                if (settings.Type == TextureUsage.Normal)
                {
                    bool flip = settings.FlipGreen;
                    if (Gui.Checkbox("Flip green", ref flip))
                        settings = settings with { FlipGreen = flip };
                }
            }
            Gui.Space(2.0f);

            using (Gui.Card("##compression", "Compression", Icon.Filter, null, iconColor))
            {
                if (settings.Format == TextureCompression.Standard)
                    settings = Choice("Quality", settings, settings.Quality, (s, v) => s with { Quality = v });
                if (settings.Type == TextureUsage.Color)
                    settings = Choice("Alpha channel", settings, settings.Alpha, (s, v) => s with { Alpha = v });
                settings = Choice("Mipmap filtering", settings, settings.MipFilter, (s, v) => s with { MipFilter = v });
                settings = Choice("Scale", settings, settings.Scale, (s, v) => s with { Scale = v });
                plan = TextureFormats.Plan(settings, width, height, driver.HasAlpha);
                Gui.PropertyLabel("Import format", plan.Format.ToString());
                Gui.PropertyLabel("Output", $"{plan.Width} x {plan.Height}, {plan.MipCount} mip(s), {FormatSize(plan.ByteSize)}");
                if (settings.Scale == TextureScale.None && (plan.Width != width || plan.Height != height))
                    Gui.Label("Resized to multiples of 4 for BC.");
            }
            Gui.Space(2.0f);
            draft.Draft = settings;

            using (Gui.Card("##source", "Source info", Icon.File, null, iconColor))
            {
                DrawPreview(view.Preview);
                Gui.PropertyLabel("Path", driver.Path);
                Gui.PropertyLabel("Dimensions", $"{width} x {height}");
                Gui.PropertyLabel("Pixel format", driver.HasAlpha ? "RGBA8, alpha used" : "RGBA8");
                Gui.PropertyLabel("File size", FormatSize(driver.FileSize));
                Gui.PropertyLabel("Generated", driver.GeneratedSize > 0 ? FormatSize(driver.GeneratedSize) : "Not built");
                if (driver.BuildError is not null)
                    Gui.Label("Build error: " + driver.BuildError);
            }
            Gui.Space(2.0f);

            using (Gui.Card("##tags", "Tags", Icon.Tag, null, iconColor))
            {
                if (ProjectService.Assets is { } assets)
                    view.Tags.OnAssetGUI(assets.Tags, assets.Index, driver.Path);
            }

            if (!draft.IsDirty)
                return;
            Gui.Space(2.0f);
            using (Gui.Card("##pending", "Unapplied changes", Icon.Warning, null, Style.Color(StyleColor.IconWarning)))
                DrawPendingActions(driver);
        }

        private static TextureSettings Choice<T>(string label, TextureSettings settings, T value, Func<TextureSettings, T, TextureSettings> set) where T : struct, Enum
        {
            int index = EnumDescriptions<T>.IndexOf(value);
            return Gui.Combo(label, ref index, EnumDescriptions<T>.Labels) ? set(settings, EnumDescriptions<T>.Values[index]) : settings;
        }

        private static TextureSettings DrawType(TextureSettings settings)
        {
            float width = MathF.Max(Gui.AvailableSize.x, 90.0f);
            using var canvas = Gui.Canvas("##texture_type", new Vector2(width, TypeRowHeight));
            TextureUsage[] types = EnumDescriptions<TextureUsage>.Values;
            float cell = width / types.Length;
            for (int i = 0; i < types.Length; i++)
            {
                TextureUsage type = types[i];
                Rect rect = Rect.FromXYWH(i * cell + 1.0f, 1.0f, cell - 2.0f, TypeRowHeight - 2.0f);
                bool on = settings.Type == type;
                if (ToggleButton.Draw(canvas.Draw, "##type" + i, rect, Icon.None, null, EnumDescriptions<TextureUsage>.Labels[i], null, null, on, true, ToggleDot.Radio).Clicked() && !on)
                    settings = settings with { Type = type, FlipGreen = type == TextureUsage.Normal && settings.FlipGreen };
            }
            return settings;
        }

        private static void DrawPendingActions(TextureImportDriver driver)
        {
            float width = MathF.Max(Gui.AvailableSize.x, 90.0f);
            float cell = width * 0.5f;
            using var canvas = Gui.Canvas("##pending_actions", new Vector2(width, ActionRowHeight));
            bool revert = ToggleButton.Draw(canvas.Draw, "##revert", Rect.FromXYWH(1.0f, 0.0f, cell - 2.0f, ActionRowHeight), Icon.None, null, "Revert", null, null, false, true, ToggleDot.None).Clicked();
            bool apply = ToggleButton.Draw(canvas.Draw, "##apply", Rect.FromXYWH(cell + 1.0f, 0.0f, cell - 2.0f, ActionRowHeight), Icon.None, null, "Apply", null, null, false, true, ToggleDot.None).Clicked();
            if (revert)
                driver.Revert();
            else if (apply)
                driver.Apply();
        }

        private static void DrawPreview(Texture? preview)
        {
            float width = MathF.Max(Gui.AvailableSize.x, 64.0f);
            using var canvas = Gui.Canvas("##texture_preview", new Vector2(width, PreviewHeight));
            canvas.Draw.RectFilled(Rect.FromXYWH(0.0f, 0.0f, width, PreviewHeight), Style.Color(StyleColor.SegmentBg), 3.0f);
            if (preview is null)
                return;
            float scale = MathF.Min((width - 8.0f) / preview.Width, (PreviewHeight - 8.0f) / preview.Height);
            float w = preview.Width * scale;
            float h = preview.Height * scale;
            canvas.Draw.Image(preview, Rect.FromXYWH((width - w) * 0.5f, (PreviewHeight - h) * 0.5f, w, h));
        }

        private static string FormatSize(long bytes) =>
            bytes >= 1024 * 1024 ? $"{bytes / (1024.0 * 1024.0):F1} MB" : $"{Math.Max(1, bytes / 1024)} KB";
    }

    // Per-Inspector UI state of the Texture Import tool: the preview texture of the shown driver and the tag field.
    public sealed class TextureImportViewState : IDisposable
    {
        private TextureImportDriver? _shown;

        public TagField Tags { get; } = new("##texture_tags", TagFieldMode.Asset);
        public Texture? Preview { get; private set; }

        // Switches to the driver: remakes the preview texture and resets the tag field when it's another one.
        public void Show(TextureImportDriver? driver)
        {
            if (ReferenceEquals(driver, _shown))
                return;
            _shown = driver;
            Preview?.Dispose();
            Preview = driver?.PreviewPixels is { } pixels ? new Texture(pixels, driver.PreviewWidth, driver.PreviewHeight) : null;
            Tags.Reset(false);
            if (driver is null)
                return;
            string? typed = Environment.GetEnvironmentVariable("XG_UI_TEST_TEXTURE_TAG_TYPE");
            if (!string.IsNullOrEmpty(typed))
                Tags.Type(typed, 30);
            if (Environment.GetEnvironmentVariable("XG_UI_TEST_TAGS_IMPORT") == "1")
                Tags.ShowImportTags = true;
        }

        public void Dispose()
        {
            Preview?.Dispose();
            Preview = null;
            _shown = null;
        }
    }
}
