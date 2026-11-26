// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.

using xg.editor;
using xg.imui;
using xg.math;

namespace xgEditor.CoreCLR
{
    // The submesh item table of a model asset's meta (kind "submesh"), cached by the meta's write time. Shared by the
    // Archetype Editor and the Mesh Renderer card.
    public static class ModelSubmeshes
    {
        private static readonly Dictionary<string, (long Time, List<AssetItem> Items)> _cache = new(StringComparer.OrdinalIgnoreCase);

        public static IReadOnlyList<AssetItem> Of(string modelGuid)
        {
            if (!AssetMeta.IsValidGuid(modelGuid) || ProjectService.Assets is not { } assets || assets.PathOf(modelGuid) is not string path)
                return Array.Empty<AssetItem>();
            string full = assets.FullPath(path);
            string meta = AssetMetaStore.IsEmbedded(full) ? full : full + AssetMeta.Extension;
            long time = File.Exists(meta) ? File.GetLastWriteTimeUtc(meta).Ticks : 0;
            if (_cache.TryGetValue(modelGuid, out var cached) && cached.Time == time)
                return cached.Items;
            List<AssetItem> items = AssetMetaStore.TryRead(full, out AssetMeta? read) && read is not null
                ? read.Items.Where(i => i.Kind == MeshAssets.SubmeshKind && !i.Missing).ToList()
                : new List<AssetItem>();
            if (Environment.GetEnvironmentVariable("XG_UI_TEST_SUBMESH_SAMPLES") == "1")
            {
                string[] samples = { "Body/Skin", "Body/Armor", "Body/Cape", "Helmet/Visor", "Helmet/Plume", "Sword/Blade", "Sword/Grip" };
                for (int i = 0; i < samples.Length; i++)
                    items.Add(new AssetItem { Kind = MeshAssets.SubmeshKind, Path = samples[i], Guid = $"5a3b1e00000000000000000000000{i:x3}" });
            }
            _cache[modelGuid] = (time, items);
            return items;
        }

        // The submesh's display name: the part of its item path after the mesh ("bear/Skin" -> "Skin").
        public static string NameOf(AssetItem item) => item.Path.Contains('/') ? item.Path[(item.Path.LastIndexOf('/') + 1)..] : item.Path;

        // The mesh a submesh belongs to: its item path up to the first '/' ("bear/Skin" -> "bear").
        public static string MeshOf(AssetItem item) => item.Path.Contains('/') ? item.Path[..item.Path.IndexOf('/')] : item.Path;
    }

    // The owner of the rows a SubmeshMaterialTable edits (an Object's parts, a MeshRenderer's submesh entries). Material
    // GUIDs are 32 hex digits, empty for None.
    public interface ISubmeshMaterials
    {
        string Model { get; }
        void SetModel(string modelGuid);
        bool IsDrawn(string submesh);
        void SetDrawn(string submesh, bool drawn);
        string MaterialOf(string submesh);
        void SetMaterial(string submesh, string material);
        // False for rows the owner can't store (shown disabled).
        bool CanEdit(string submesh) => true;
    }

    // The Mesh and Material control: the Model picker, a Mesh / Submesh | Material list (scrolls, resizable with the grip
    // under it; a mesh with several submeshes is a folding group row with a tri-state check, a mesh with one submesh a
    // single row with the mesh look, submeshes alternate their row colour), then Edit Here below the list, which mounts
    // the Material Editor view (preview, properties, Auto Apply, Save, Revert, Save As) for one of the drawn materials.
    // One instance per edited item (it keeps the pickers, folds, list height and the driver).
    public sealed class SubmeshMaterialTable : IDisposable
    {
        private const float RowHeight = 22.0f;
        // A row plus the item spacing under it.
        private const float RowPitch = RowHeight + 4.0f;
        private const int DefaultListRows = 6;
        private const int MinListRows = 3;
        private const int MaxListRows = 24;
        // The x of a depth-0 check (after the mesh rows' fold arrow).
        private const float CheckX = 20.0f;

        private readonly AssetPicker _modelPicker = new();
        private readonly Dictionary<string, AssetPicker> _materialPickers = new(StringComparer.OrdinalIgnoreCase);
        private readonly MaterialEditorView _materialView;
        private string _editMaterial = string.Empty;
        private readonly HashSet<string> _collapsed = new(StringComparer.Ordinal);
        private float _listHeight = DefaultListRows * RowPitch;

        public SubmeshMaterialTable(string id) => _materialView = new MaterialEditorView(id + "_material");

        public MaterialEditorView MaterialView => _materialView;

        // The material mounted under the table (asset GUID, empty for none).
        public string EditMaterial
        {
            get => _editMaterial;
            set => _editMaterial = value;
        }

        // Auto Apply: the renderer material of the edited material's unsaved draft, for that GUID; 0 otherwise.
        public ulong DraftMaterialFor(string guid) =>
            _materialView.AutoApply && _materialView.Driver is not null && string.Equals(guid, _editMaterial, StringComparison.OrdinalIgnoreCase)
                ? _materialView.DraftMaterial : 0;

        // A note under the table (the Mesh Renderer's submesh limit), or null.
        public string? Note { get; set; }

        public void OnGUI(ISubmeshMaterials owner)
        {
            string model = owner.Model;
            if (_modelPicker.Field("model", "Model", "mesh", ref model))
                owner.SetModel(model);
            IReadOnlyList<AssetItem> submeshes = ModelSubmeshes.Of(owner.Model);
            if (submeshes.Count == 0)
            {
                Gui.Label(AssetMeta.IsValidGuid(owner.Model) ? "The model has no submeshes yet (build it)." : "Pick a model.");
                CloseEditor();
                return;
            }
            List<(string Mesh, List<AssetItem> Items)> meshes = new();
            foreach (AssetItem item in submeshes)
            {
                string mesh = ModelSubmeshes.MeshOf(item);
                int group = meshes.FindIndex(g => g.Mesh == mesh);
                if (group < 0)
                    meshes.Add((mesh, new List<AssetItem> { item }));
                else
                    meshes[group].Items.Add(item);
            }
            int rows = meshes.Sum(g => g.Items.Count == 1 ? 1 : 1 + (_collapsed.Contains(g.Mesh) ? 0 : g.Items.Count));

            float width = MathF.Max(Gui.AvailableSize.x, 160.0f);
            using (var header = Gui.Canvas("##table_header", new Vector2(width, 20.0f)))
            {
                header.Draw.RectFilled(Rect.FromXYWH(0.0f, 0.0f, width, 20.0f), Style.Color(StyleColor.PanelHeaderBg), 2.0f);
                Color32 dim = Style.Color(StyleColor.TextDim);
                float textY = MathF.Round(10.0f - Gui.MeasureText("A").y * 0.5f);
                header.Draw.Text(new Vector2(8.0f, textY), dim, "Mesh / Submesh");
                header.Draw.Text(new Vector2(MaterialColumn(width) + 4.0f, textY), dim, "Material");
            }
            float content = rows * RowPitch;
            float height = MathF.Max(MathF.Min(_listHeight, content), RowPitch);
            List<string> editable = new();
            using (var list = Gui.ScrollArea("##submesh_list", new Vector2(width, height)))
            {
                if (list.IsOpen)
                {
                    int row = 0;
                    foreach ((string mesh, List<AssetItem> items) in meshes)
                    {
                        if (items.Count == 1)
                        {
                            DrawItemRow(owner, items[0], mesh, 0, true, row++, editable);
                            continue;
                        }
                        DrawMeshRow(owner, mesh, items);
                        row++;
                        if (_collapsed.Contains(mesh))
                            continue;
                        int stripe = 0;
                        foreach (AssetItem item in items)
                            DrawItemRow(owner, item, ModelSubmeshes.NameOf(item), 1, false, stripe++, editable);
                        row += items.Count;
                    }
                }
            }
            if (content > MinListRows * RowPitch)
            {
                float maxHeight = MaxListRows * RowPitch;
                float below = maxHeight - _listHeight;
                if (Gui.Splitter("##submesh_grip", ref below, 0.0f, maxHeight - MinListRows * RowPitch))
                    _listHeight = Math.Clamp(maxHeight - below, MinListRows * RowPitch, maxHeight);
                Gui.Tooltip("Drag to show more or fewer rows");
            }
            foreach ((string mesh, List<AssetItem> items) in meshes.Where(g => g.Items.Count > 1 && _collapsed.Contains(g.Mesh)))
            {
                foreach (AssetItem item in items)
                {
                    string material = owner.MaterialOf(item.Guid);
                    if (owner.IsDrawn(item.Guid) && AssetMeta.IsValidGuid(material) && !editable.Contains(material) && !EngineAssets.IsInternal(material))
                        editable.Add(material);
                }
            }
            if (Note is not null)
                Gui.Label(Note);
            Gui.Space(EditorStyle.BarGap);
            DrawMaterialEditor(editable);
        }

        // The x of the Material column: names get the larger share, the picker keeps at least 40% (its own minimum).
        private static float MaterialColumn(float width) => MathF.Round(MathF.Max(120.0f, width * 0.58f));
        // The picker places itself 24 px after the name canvas.
        private const float PickerGap = 24.0f;

        // A mesh with several submeshes: fold arrow, tri-state check (sets every submesh), mesh icon, bold name, count
        // chip and "N of M drawn".
        private void DrawMeshRow(ISubmeshMaterials owner, string mesh, List<AssetItem> items)
        {
            int drawn = items.Count(i => owner.IsDrawn(i.Guid));
            bool folded = _collapsed.Contains(mesh);
            float width = MathF.Max(Gui.AvailableSize.x, 160.0f);
            using (Gui.Id("mesh_" + mesh))
            using (var canvas = Gui.Canvas("##mesh_row", new Vector2(width, RowHeight)))
            {
                Painter draw = canvas.Draw;
                DrawMeshBackground(draw, width);
                float cy = RowHeight * 0.5f;
                Result arrow = Gui.HitArea("##fold", Rect.FromXYWH(2.0f, 0.0f, 16.0f, RowHeight));
                Gui.Tooltip(folded ? "Show the submeshes" : "Hide the submeshes");
                Color32 arrowColor = Style.Color(arrow.Hovered() ? StyleColor.Text : StyleColor.TextDim);
                if (folded)
                    draw.TriangleFilled(new Vector2(8.0f, cy - 4.0f), new Vector2(14.0f, cy), new Vector2(8.0f, cy + 4.0f), arrowColor);
                else
                    draw.TriangleFilled(new Vector2(7.0f, cy - 3.0f), new Vector2(15.0f, cy - 3.0f), new Vector2(11.0f, cy + 3.0f), arrowColor);
                if (arrow.Clicked())
                {
                    if (folded)
                        _collapsed.Remove(mesh);
                    else
                        _collapsed.Add(mesh);
                }
                Result check = Gui.HitArea("##mesh_check", Rect.FromXYWH(CheckX - 2.0f, 2.0f, 18.0f, 18.0f));
                Gui.Tooltip(drawn == items.Count ? "All drawn (click to leave them out)" : "Click to draw every submesh");
                DrawTriCheck(draw, Rect.FromXYWH(CheckX, 4.0f, 14.0f, 14.0f), drawn, items.Count, check.Hovered());
                if (check.Clicked())
                {
                    bool on = drawn != items.Count;
                    foreach (AssetItem item in items)
                        if (owner.CanEdit(item.Guid) && owner.IsDrawn(item.Guid) != on)
                            owner.SetDrawn(item.Guid, on);
                }
                float column = MaterialColumn(width);
                float x = DrawMeshName(draw, CheckX + 20.0f, mesh, drawn > 0, column - CheckX - 60.0f);
                string count = items.Count.ToString();
                Vector2 countSize = Gui.MeasureText(count);
                Rect chip = Rect.FromXYWH(x + 6.0f, MathF.Round(cy - countSize.y * 0.5f) - 1.0f, countSize.x + 8.0f, countSize.y + 2.0f);
                draw.RectFilled(chip, new Color32(24, 25, 28, 255), 2.0f);
                draw.Text(new Vector2(chip.min.x + 4.0f, chip.min.y + 1.0f), Style.Color(StyleColor.TextDim), count);
                string summary = $"{drawn} of {items.Count} drawn";
                draw.Text(new Vector2(column + 4.0f, MathF.Round(cy - Gui.MeasureText(summary).y * 0.5f)), Style.Color(StyleColor.TextDim), summary);
            }
        }

        // A submesh (depth 1, alternating stripes) or a mesh with one submesh (depth 0, the mesh look): check, glyph,
        // name, then the material picker in the Material column.
        private void DrawItemRow(ISubmeshMaterials owner, AssetItem item, string name, int depth, bool meshLook, int stripe, List<string> editable)
        {
            bool drawn = owner.IsDrawn(item.Guid);
            bool canEdit = owner.CanEdit(item.Guid);
            float width = MathF.Max(Gui.AvailableSize.x, 160.0f);
            float labelWidth = MathF.Max(MaterialColumn(width) - PickerGap, 60.0f);
            using (Gui.Id(item.Guid))
            using (Gui.Disabled(!canEdit))
            using (Gui.Horizontal(0.0f))
            {
                using (var canvas = Gui.Canvas("##item", new Vector2(labelWidth, RowHeight)))
                {
                    Painter draw = canvas.Draw;
                    float check = CheckX + depth * 14.0f;
                    Result hit = Gui.HitArea("##include_hit", Rect.FromXYWH(check - 2.0f, 2.0f, 18.0f, 18.0f));
                    Gui.Tooltip(drawn ? "Drawn (click to leave it out)" : "Not drawn (click to draw it)");
                    DrawCheck(draw, Rect.FromXYWH(check, 4.0f, 14.0f, 14.0f), drawn, hit.Hovered());
                    if (hit.Clicked() && canEdit)
                        owner.SetDrawn(item.Guid, !drawn);
                    if (meshLook)
                    {
                        DrawMeshName(draw, check + 20.0f, name, drawn, labelWidth - check - 20.0f);
                    }
                    else
                    {
                        Color32 glyph = Style.Color(drawn ? StyleColor.TextDim : StyleColor.Border);
                        draw.Icon(Icon.Data, new Vector2(check + 26.0f, RowHeight * 0.5f), 11.0f, glyph);
                        draw.Text(new Vector2(check + 35.0f, MathF.Round(RowHeight * 0.5f - Gui.MeasureText(name).y * 0.5f)),
                            Style.Color(drawn ? StyleColor.Text : StyleColor.TextDim), Fit(name, labelWidth - check - 37.0f));
                    }
                }
                Gui.Tooltip(item.Path);
                if (!_materialPickers.TryGetValue(item.Guid, out AssetPicker? picker))
                {
                    picker = new AssetPicker();
                    _materialPickers[item.Guid] = picker;
                }
                string material = owner.MaterialOf(item.Guid);
                if (picker.Field("material", "##" + item.Guid, "material", ref material) && canEdit)
                    owner.SetMaterial(item.Guid, material);
                if (drawn && AssetMeta.IsValidGuid(material) && !editable.Contains(material) && !EngineAssets.IsInternal(material))
                    editable.Add(material);
            }
            Rect rect = Gui.ItemRect;
            if (meshLook)
            {
                Gui.Draw.RectFilled(Rect.FromXYWH(0.0f, rect.min.y, width, RowHeight), new Color32(255, 255, 255, 14));
                Gui.Draw.RectFilled(Rect.FromXYWH(0.0f, rect.min.y, 3.0f, RowHeight), Style.Color(StyleColor.IconComponent));
            }
            else if (stripe % 2 == 1)
                Gui.Draw.RectFilled(Rect.FromXYWH(0.0f, rect.min.y - 2.0f, width, RowPitch), new Color32(255, 255, 255, 8));
        }

        private static void DrawMeshBackground(Painter draw, float width)
        {
            draw.RectFilled(Rect.FromXYWH(0.0f, 0.0f, width, RowHeight), new Color32(255, 255, 255, 14));
            draw.RectFilled(Rect.FromXYWH(0.0f, 0.0f, 3.0f, RowHeight), Style.Color(StyleColor.IconComponent));
        }

        // Mesh icon and bold name at x (cut to fit maxWidth); returns the x after the name.
        private static float DrawMeshName(Painter draw, float x, string name, bool drawn, float maxWidth)
        {
            draw.Icon(Icon.Mesh, new Vector2(x + 6.0f, RowHeight * 0.5f), 13.0f, Style.Color(drawn ? StyleColor.IconComponent : StyleColor.TextDim));
            using (Gui.Font(FontStyle.Bold))
            {
                string shown = Fit(name, maxWidth - 16.0f);
                Vector2 size = Gui.MeasureText(shown);
                draw.Text(new Vector2(x + 16.0f, MathF.Round(RowHeight * 0.5f - size.y * 0.5f)), Style.Color(drawn ? StyleColor.Text : StyleColor.TextDim), shown);
                return x + 16.0f + size.x;
            }
        }

        // The text cut with "..." to fit the width (in the current font).
        private static string Fit(string text, float width)
        {
            if (Gui.MeasureText(text).x <= width)
                return text;
            for (int length = text.Length - 1; length > 0; length--)
            {
                string cut = text[..length] + "...";
                if (Gui.MeasureText(cut).x <= width)
                    return cut;
            }
            return string.Empty;
        }

        private static void DrawTriCheck(Painter draw, Rect box, int on, int total, bool hovered)
        {
            if (on == 0 || on == total)
            {
                DrawCheck(draw, box, on == total, hovered);
                return;
            }
            draw.RectFilled(box, Style.Color(StyleColor.Accent), 2.0f);
            float cy = box.min.y + box.Height * 0.5f;
            draw.Line(new Vector2(box.min.x + 3.0f, cy), new Vector2(box.max.x - 3.0f, cy), Style.Color(StyleColor.TextOnAccent), 1.8f);
        }

        public static void DrawCheck(Painter draw, Rect box, bool on, bool hovered)
        {
            if (on)
            {
                draw.RectFilled(box, Style.Color(StyleColor.Accent), 2.0f);
                Color32 mark = Style.Color(StyleColor.TextOnAccent);
                draw.Line(new Vector2(box.min.x + 3.0f, box.min.y + 7.5f), new Vector2(box.min.x + 6.0f, box.min.y + 10.5f), mark, 1.8f);
                draw.Line(new Vector2(box.min.x + 6.0f, box.min.y + 10.5f), new Vector2(box.min.x + 11.0f, box.min.y + 4.0f), mark, 1.8f);
                return;
            }
            draw.RectFilled(box, new Color32(26, 27, 30, 255), 2.0f);
            draw.Rect(box, Style.Color(hovered ? StyleColor.Accent : StyleColor.Border), 2.0f);
        }

        // Edit Here: the Material Editor view for one of the drawn materials.
        private void DrawMaterialEditor(List<string> materials)
        {
            if (materials.Count == 0 || ProjectService.Assets is not { } assets)
            {
                CloseEditor();
                return;
            }
            List<string> names = new() { "None" };
            names.AddRange(materials.Select(AssetBrowser.NameOf));
            int index = materials.IndexOf(_editMaterial) + 1;
            if (Gui.Combo("Edit Here", ref index, names))
                _editMaterial = index <= 0 ? string.Empty : materials[index - 1];
            Gui.Tooltip("Edit one of these materials in place (Save writes the .xcm)");
            if (!materials.Contains(_editMaterial) || assets.PathOf(_editMaterial) is not string path)
            {
                CloseEditor();
                return;
            }
            if (_materialView.Driver is null || !string.Equals(_materialView.Driver.Path, path, StringComparison.OrdinalIgnoreCase))
            {
                _materialView.Driver?.Dispose();
                _materialView.Driver = new MaterialDriver(path);
            }
            Gui.Space(EditorStyle.BarGap);
            _materialView.OnEmbeddedGUI();
        }

        private void CloseEditor()
        {
            if (_materialView.Driver is null)
                return;
            _materialView.Driver.Dispose();
            _materialView.Driver = null;
        }

        public void Dispose()
        {
            CloseEditor();
            _materialView.Dispose();
        }
    }
}
