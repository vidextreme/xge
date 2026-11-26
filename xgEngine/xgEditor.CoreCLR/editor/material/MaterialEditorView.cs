// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.

using System.Diagnostics;
using xg;
using xg.editor;
using xg.imui;
using xg.math;
using Debug = xg.Debug;

namespace xgEditor.CoreCLR
{
    // The material editor as a mountable control (no panel base class, like Ribbon): a host owns one, sets Driver and
    // calls OnGUI from a panel with NoPadding | NoScrollbar. Two columns when there's room (preview and tools on the
    // left, properties on the right), one stacked column otherwise, so a narrower pane (the future .xco editor) can host
    // it too. Properties: the shader, one card per annotation group (params and textures in declaration order), tags.
    public sealed class MaterialEditorView : IDisposable
    {
        private const float TwoColumnWidth = 560.0f;
        private const float MinLeftWidth = 200.0f;
        private const float MinRightWidth = 300.0f;
        private const float PreviewAspect = 0.9f;
        private const float ActionRowHeight = 22.0f;

        private readonly MaterialPreview _preview;
        private readonly AssetPicker _shaderPicker = new();
        private readonly Dictionary<string, AssetPicker> _texturePickers = new(StringComparer.Ordinal);
        private readonly Dictionary<string, bool> _groupsOpen = new(StringComparer.Ordinal);
        private readonly TagField _tags;
        private float _leftWidth = 800.0f;
        private bool _previewOpen = true;
        private bool _embedded;

        public MaterialEditorView(string id)
        {
            _preview = new MaterialPreview("##" + id + "_preview");
            _tags = new TagField("##" + id + "_tags", TagFieldMode.Asset);
        }

        public void SetLeftWidth(float leftWidth)
        {
            _leftWidth = leftWidth;
        }
        public MaterialDriver? Driver { get; set; }
        // Embedded only: the host shows the draft (DraftMaterial) in place of the saved material, so edits show up before
        // they are saved. Nothing is written until Save.
        public bool AutoApply { get; set; }
        // The renderer material built from the draft (0 until the preview built one).
        public ulong DraftMaterial => _preview.DraftMaterial;
        public MaterialPreview Preview => _preview;
        // Raised after Save as, with the new path (hosts follow it).
        public event Action<string>? SavedAs;

        public void OnGUI()
        {
            Vector2 size = Gui.AvailableSize;
            if (size.x <= 1.0f || size.y <= 1.0f)
                return;
            MaterialDriver? driver = Driver;
            driver?.RefreshAnnotations();
            if (size.x >= TwoColumnWidth)
            {
                float maxLeft = MathF.Max(MinLeftWidth, size.x - MinRightWidth);
                float leftWidth = Math.Clamp(_leftWidth, MinLeftWidth, maxLeft);
                using (Gui.Horizontal(0.0f))
                {
                    using (var left = Gui.ScrollArea("##material_left", new Vector2(leftWidth, 0.0f), ScrollAreaFlags.Padded))
                    {
                        if (left.IsOpen)
                        {
                            FillBackground();
                            DrawPreview(driver);
                        }
                    }
                    if (Gui.SplitterVertical("##material_split", ref leftWidth, MinLeftWidth, maxLeft))
                        _leftWidth = leftWidth;
                    Gui.Tooltip("Drag to resize the preview column");
                    using (var right = Gui.ScrollArea("##material_right", default, ScrollAreaFlags.Padded))
                    {
                        if (right.IsOpen)
                        {
                            FillBackground();
                            DrawProperties(driver);
                            DrawTools(driver);
                        }
                    }
                }
            }
            else
            {
                using var area = Gui.ScrollArea("##material_single", default, ScrollAreaFlags.Padded);
                if (area.IsOpen)
                {
                    FillBackground();
                    DrawPreview(driver);
                    DrawTools(driver);
                    DrawProperties(driver);
                }
            }
        }

        // The preview, properties and Save / Revert / Save as in one column, for hosts that mount the material inside
        // their own layout (the Archetype Editor's Mesh and material card).
        public void OnEmbeddedGUI()
        {
            MaterialDriver? driver = Driver;
            driver?.RefreshAnnotations();
            _embedded = true;
            if (AutoApply)
                _preview.RefreshDraft(driver);
            DrawPreview(driver);
            DrawProperties(driver);
            DrawTools(driver);
        }

        private static void FillBackground()
        {
            Vector2 avail = Gui.AvailableSize;
            Gui.Draw.RectFilled(new Rect(new Vector2(-6.0f, -6.0f), new Vector2(avail.x + 6.0f, 100000.0f)), Style.Color(StyleColor.ToolbarBg));
        }

        // The preview card folds, and its size follows the column width (drag the splitter to enlarge it).
        private void DrawPreview(MaterialDriver? driver)
        {
            using (var card = Gui.Card("##material_preview", "Preview", Icon.Sphere, null, Style.Color(StyleColor.Icon), ref _previewOpen))
            {
                if (card.IsOpen)
                {
                    float width = MathF.Max(Gui.AvailableSize.x, 64.0f);
                    _preview.OnGUI(driver, new Vector2(width, MathF.Round(width * PreviewAspect)));
                }
            }
            Gui.Space(EditorStyle.BarGap);
        }

        private void DrawTools(MaterialDriver? driver)
        {
            using (Gui.Card("##material_tools", "Tools", Icon.Settings, null, Style.Color(StyleColor.Icon)))
            {
                bool dirty = driver?.HasPendingChanges == true;
                float width = MathF.Max(Gui.AvailableSize.x, 90.0f);
                float cell = width * 0.5f;
                int rows = _embedded ? 3 : 2;
                using var canvas = Gui.Canvas("##material_tool_buttons", new Vector2(width, ActionRowHeight * rows + 2.0f * (rows - 1)));
                Painter draw = canvas.Draw;
                float top = 0.0f;
                if (_embedded)
                {
                    if (ToggleButton.Draw(draw, "##auto_apply", Rect.FromXYWH(1.0f, 0.0f, width - 2.0f, ActionRowHeight), Icon.Object, null, "Auto Apply", null,
                            "Show the unsaved edits on the object right away (Save writes them)", AutoApply, driver is not null, ToggleDot.Check).Clicked())
                        AutoApply = !AutoApply;
                    top = ActionRowHeight + 2.0f;
                }
                bool save = ToggleButton.Draw(draw, "##save", Rect.FromXYWH(1.0f, top, cell - 2.0f, ActionRowHeight), Icon.Save, null, "Save",
                    null, dirty ? "Write the material and rebuild it" : "No unsaved changes", false, dirty, dirty ? ToggleDot.Accent : ToggleDot.None).Clicked();
                bool revert = ToggleButton.Draw(draw, "##revert", Rect.FromXYWH(cell + 1.0f, top, cell - 2.0f, ActionRowHeight), Icon.None, null, "Revert",
                    null, "Drop the unsaved changes", false, dirty, ToggleDot.None).Clicked();
                bool saveAs = ToggleButton.Draw(draw, "##save_as", Rect.FromXYWH(1.0f, top + ActionRowHeight + 2.0f, width - 2.0f, ActionRowHeight), Icon.File, null,
                    "Save As...", null, "Save a copy and edit it", false, driver is not null, ToggleDot.None).Clicked();
                if (save && dirty)
                    driver!.Apply();
                else if (revert && dirty)
                    driver!.Revert();
                else if (saveAs && driver is not null)
                {
                    OpenSaveAs(driver);
                }
            }
        }

        private void DrawProperties(MaterialDriver? driver)
        {
            if (driver is null)
            {
                Gui.Label("Open a material (.xcm) to edit it.");
                return;
            }
            DrawHeader(driver);
            if (driver.Error is not null)
            {
                Gui.Label(driver.Error);
                return;
            }
            DrawShader(driver);
            ShaderAnnotations? annotations = driver.Annotations;
            if (annotations is not null)
            {
                foreach (string group in annotations.Groups)
                    DrawGroup(driver, annotations, group);
                if (annotations.Errors.Count > 0)
                    DrawAnnotationErrors(annotations.Errors);
            }
            using (Gui.Card("##material_tags", "Tags", Icon.Tag, null, Style.Color(StyleColor.Icon)))
            {
                if (ProjectService.Assets is { } assets)
                    _tags.OnAssetGUI(assets.Tags, assets.Index, driver.Path);
            }
        }

        // Name (amber dot while unsaved), build status chip, then the path in dim mono.
        private static void DrawHeader(MaterialDriver driver)
        {
            using (Gui.Horizontal(EditorStyle.BarGap))
            {
                using (Gui.Font(FontStyle.Bold))
                    Gui.Label(driver.Name);
                if (driver.HasPendingChanges)
                {
                    using (var dot = Gui.Canvas("##unsaved", new Vector2(8.0f, 18.0f)))
                        dot.Draw.CircleFilled(new Vector2(4.0f, 9.0f), 3.5f, Style.Color(StyleColor.IconWarning));
                    Gui.Tooltip("Unsaved changes");
                }
                Gui.FlexibleSpace();
                DrawStatusChip(driver.Guid);
            }
            using (Gui.Font(FontStyle.Mono))
            {
                Vector2 size = Gui.MeasureText(driver.Path);
                using var path = Gui.Canvas("##path", new Vector2(0.0f, size.y + 2.0f));
                path.Draw.Text(new Vector2(0.0f, 1.0f), Style.Color(StyleColor.TextDim), driver.Path);
            }
            Gui.Space(EditorStyle.BarGap);
        }

        // Built / Not built / Build failed (the error as a tooltip) for a composition's GUID.
        internal static void DrawStatusChip(string? assetGuid)
        {
            if (ProjectService.Assets is not { } assets || assetGuid is not string guid)
                return;
            string text;
            Color32 fill;
            Color32 color;
            var manifest = assets.Pipeline.ManifestOf(guid);
            if (manifest is null)
            {
                text = "Not built";
                fill = new Color32(43, 43, 43, 255);
                color = Style.Color(StyleColor.TextDim);
            }
            else if (manifest.Error is null)
            {
                text = "Built";
                fill = new Color32(40, 56, 38, 255);
                color = new Color32(143, 192, 112, 255);
            }
            else
            {
                text = "Build failed";
                fill = new Color32(66, 34, 34, 255);
                color = new Color32(224, 120, 112, 255);
            }
            Vector2 size = Gui.MeasureText(text);
            using var chip = Gui.Canvas("##status", new Vector2(size.x + 16.0f, 18.0f));
            chip.Draw.RectFilled(Rect.FromXYWH(0.0f, 0.0f, size.x + 16.0f, 18.0f), fill, 9.0f);
            chip.Draw.Text(new Vector2(8.0f, MathF.Round(9.0f - size.y * 0.5f)), color, text);
            if (manifest?.Error is string error)
                Gui.Tooltip(error);
        }

        private static void DrawAnnotationErrors(List<string> errors)
        {
            using (Gui.Card("##material_errors", "Annotation errors", Icon.Warning, null, Style.Color(StyleColor.IconWarning)))
            {
                foreach (string error in errors)
                    Gui.Label(error);
            }
        }

        private void DrawShader(MaterialDriver driver)
        {
            using (Gui.Card("##material_shader", "Shader", Icon.Code, null, Style.Color(StyleColor.IconData)))
            {
                string guid = driver.Draft.Shader;
                if (_shaderPicker.Field("shader", "Shader", "shader", ref guid))
                    driver.SetShader(guid);
                ShaderAnnotations? annotations = driver.Annotations;
                using (Gui.Horizontal())
                {
                    Gui.Label(annotations is null
                        ? (driver.ShaderPath is null ? "No shader" : "Can't read the shader")
                        : $"{annotations.Params.Count} param(s), {annotations.Textures.Count} texture(s)");
                    Gui.FlexibleSpace();
                    bool builtIn = EngineAssets.IsEnginePath(driver.ShaderPath);
                    if (driver.ShaderPath is not null)
                    {
                        using (Gui.Disabled(builtIn))
                        {
                            if (Gui.Button("Edit Code") && !builtIn && ProjectService.Assets is { } assets)
                                OpenFile(assets.FullPath(driver.ShaderPath));
                        }
                        Gui.Tooltip(builtIn ? "Built-in shaders can't be edited; copy it into Assets to change it" : "Open the shader in its default editor");
                    }
                }
            }
        }

        private void DrawGroup(MaterialDriver driver, ShaderAnnotations annotations, string group)
        {
            bool open = !_groupsOpen.TryGetValue(group, out bool stored) || stored;
            using (var card = Gui.Card("##group_" + group, group, Icon.Data, null, Style.Color(StyleColor.Icon), ref open))
            {
                _groupsOpen[group] = open;
                if (!card.IsOpen)
                    return;
                foreach (object entry in annotations.Entries(group))
                {
                    if (entry is ShaderParam param)
                        DrawParam(driver, param);
                    else if (entry is ShaderTexture texture)
                        DrawTexture(driver, texture);
                }
            }
        }

        private static void DrawParam(MaterialDriver driver, ShaderParam param)
        {
            string label = ShaderAnnotations.DisplayName(param.Name);
            float[] value = driver.ParamValue(param);
            using (Gui.Id(param.Name))
            {
                bool color = param.Type == ShaderParamType.Color;
                NumberFieldStyle style = new() { NotchTags = param.NotchTags };
                if (param.HasRange)
                {
                    style.Flags |= NumberFieldFlags.Range;
                    style.Min = param.Min!.Value;
                    style.Max = param.Max!.Value;
                }
                if (color)
                    style.Flags |= NumberFieldFlags.Color;
                Span<float> values = stackalloc float[param.Components];
                for (int i = 0; i < values.Length; i++)
                    values[i] = value[i];
                if (Gui.Field(label, values, style, param.HasRange ? 0.0f : 0.01f))
                {
                    float[] next = (float[])value.Clone();
                    for (int i = 0; i < values.Length; i++)
                        next[i] = values[i];
                    driver.SetParam(param.Name, next);
                }
            }
        }

        private void DrawTexture(MaterialDriver driver, ShaderTexture texture)
        {
            if (!_texturePickers.TryGetValue(texture.Name, out AssetPicker? picker))
            {
                picker = new AssetPicker();
                _texturePickers[texture.Name] = picker;
            }
            string guid = driver.TextureGuid(texture);
            string label = ShaderAnnotations.DisplayName(texture.Name);
            if (picker.Field("texture_" + texture.Name, label, "image", ref guid, texture.Usage.ToString().ToLowerInvariant()))
                driver.SetTexture(texture.Name, guid);
        }

        // Save as through the shared SaveAsDialog: the copy goes next to the material unless another folder is chosen.
        // Opens Save as for the edited asset (also the Tools card's Save as... button).
        public void SaveAs()
        {
            if (Driver is { } driver)
                OpenSaveAs(driver);
        }

        private void OpenSaveAs(MaterialDriver driver)
        {
            string folder = driver.Path.Contains('/') ? driver.Path[..driver.Path.LastIndexOf('/')] : AssetIndex.RootFolder;
            AssetEntry? entry = ProjectService.Assets?.Index.Find(driver.Path);
            SaveAsDialog.Open(new SaveAsRequest
            {
                Title = "Save Material As",
                Context = driver.Path,
                Name = driver.Name + " copy",
                Extension = MaterialAsset.Extension,
                Folder = folder,
                DrawPreview = (draw, rect) =>
                {
                    if (entry is not null)
                        AssetThumbnails.Draw(draw, rect, entry, 28.0f);
                },
                Commit = path =>
                {
                    if (!driver.SaveAs(path))
                        return "The material couldn't be saved (see the Console)";
                    SavedAs?.Invoke(path);
                    return null;
                },
            });
        }

        private static void OpenFile(string path)
        {
            try
            {
                Process.Start(new ProcessStartInfo(path) { UseShellExecute = true });
            }
            catch (Exception e) when (e is System.ComponentModel.Win32Exception or InvalidOperationException)
            {
                Debug.LogWarning($"[Materials] Could not open {path}: {e.Message}");
            }
        }

        public void Dispose() => _preview.Dispose();
    }
}
