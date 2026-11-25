// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.

using System.Reflection;
using System.Text.Json.Nodes;
using xg;
using xg.editor;
using xg.imui;
using xg.math;

namespace xgEditor.CoreCLR
{
    // The Composition Object Editor as a mountable control (no panel base class, like MaterialEditorView): a host owns
    // one, sets Driver and calls OnGUI from a panel with NoPadding | NoScrollbar. Header on top; then the left column
    // (Tools card, Objects tree), the preview and the Inspector-style properties (a tab strip with All, Naming, Jointer,
    // Mesh and material, Components); the Timeline strip (a future feature) spans the bottom.
    public sealed class ObjectEditorView : IDisposable
    {
        private const float LeftWidth = 220.0f;
        private const float RightWidth = 350.0f;
        private const float StripSize = 30.0f;
        private const float TabSize = 30.0f;
        private const float TabGap = 3.0f;
        private const float RowHeight = 22.0f;
        private const float TreeArrowOffset = 20.0f;
        private const float TreeIndent = 14.0f;
        private const float TimelineHeight = 30.0f;
        private const string SaveAsModalId = "object_save_as";

        private static readonly string[] _tabNames = { "All", "Naming", "Jointer", "Mesh and material", "Components" };
        private static readonly Icon[] _tabIcons = { Icon.Cards, Icon.Tag, Icon.Transform, Icon.Mesh, Icon.Component };

        private readonly ObjectPreview _preview;
        private readonly AssetPicker _modelPicker = new();
        private readonly Dictionary<string, AssetPicker> _materialPickers = new(StringComparer.OrdinalIgnoreCase);
        private readonly HashSet<string> _collapsed = new(StringComparer.OrdinalIgnoreCase);
        private readonly List<(ObjectNodeAsset Node, int Depth, bool HasChildren)> _rows = new();
        private readonly TextBuffer _name = new(128);
        private readonly TextBuffer _saveAsName = new(128);
        private readonly MaterialEditorView _materialView = new("object_material");
        private readonly Dictionary<string, AuthoredPrototype> _prototypes = new(StringComparer.Ordinal);
        private readonly TextBuffer _componentSearch = new(64);
        private ListState _list;
        private string? _selected;
        private string? _nameFor;
        private string? _eulerFor;
        private float[] _eulerQuaternion = new float[4];
        private Vector3 _euler;
        private string _editMaterial = string.Empty;
        private string? _saveAsError;
        private bool _openSaveAs;
        private bool _previewOpen = true;
        private bool _timelineOpen;

        public ObjectEditorView(string id) => _preview = new ObjectPreview("##" + id + "_preview");

        public ObjectDriver? Driver { get; set; }
        public ObjectPreview Preview => _preview;
        public int Tab { get; set; }
        // The material mounted under the Mesh and material card (asset GUID, empty for none).
        public string EditMaterial
        {
            get => _editMaterial;
            set => _editMaterial = value;
        }
        public string? Selected
        {
            get => _selected;
            set => _selected = value;
        }

        // Raised after Save as, with the new path (hosts follow it).
        public event Action<string>? SavedAs;

        // Selects the first Object with this name (test hooks and tools).
        public bool SelectByName(string name)
        {
            ObjectNodeAsset? node = Driver?.Draft.Objects.FirstOrDefault(o => o.Name == name);
            _selected = node?.Id;
            return node is not null;
        }

        public void OnGUI()
        {
            Vector2 size = Gui.AvailableSize;
            if (size.x <= 1.0f || size.y <= 1.0f)
                return;
            ObjectDriver? driver = Driver;
            if (driver is not null && _selected is not null && driver.Find(_selected) is null)
                _selected = null;
            if (driver is not null && _selected is null && driver.Draft.Objects.Count > 0)
                _selected = driver.Draft.Objects[0].Id;

            using (var top = Gui.ScrollArea("##object_header", new Vector2(0.0f, 34.0f), ScrollAreaFlags.Padded))
            {
                if (top.IsOpen)
                {
                    FillBackground();
                    DrawHeader(driver);
                }
            }
            float bodyHeight = MathF.Max(Gui.AvailableSize.y - TimelineHeight - (_timelineOpen ? 70.0f : 0.0f) - 4.0f, 120.0f);
            using (Gui.Horizontal(0.0f))
            {
                using (var left = Gui.ScrollArea("##object_left", new Vector2(LeftWidth, bodyHeight), ScrollAreaFlags.Padded))
                {
                    if (left.IsOpen)
                    {
                        FillBackground();
                        DrawTools(driver);
                        DrawTree(driver);
                    }
                }
                using (var center = Gui.ScrollArea("##object_center", new Vector2(0.0f, bodyHeight), ScrollAreaFlags.Padded))
                {
                    if (center.IsOpen)
                    {
                        FillBackground();
                        DrawPreview(driver, bodyHeight);
                    }
                }
                bool stripRight = TabStrip.Detect(Gui.PanelPlacement) != PanelSide.Right;
                if (!stripRight)
                    DrawTabs(TabEdge.Left, bodyHeight);
                using (var right = Gui.ScrollArea("##object_right", new Vector2(RightWidth, bodyHeight), ScrollAreaFlags.Padded))
                {
                    if (right.IsOpen)
                    {
                        FillBackground();
                        DrawProperties(driver);
                    }
                }
                if (stripRight)
                    DrawTabs(TabEdge.Right, bodyHeight);
            }
            using (var bottom = Gui.ScrollArea("##object_timeline", default, ScrollAreaFlags.Padded))
            {
                if (bottom.IsOpen)
                {
                    FillBackground();
                    DrawTimeline();
                }
            }
            DrawSaveAs(driver);
        }

        private static void FillBackground()
        {
            Vector2 avail = Gui.AvailableSize;
            Gui.Draw.RectFilled(new Rect(new Vector2(-6.0f, -6.0f), new Vector2(avail.x + 6.0f, 100000.0f)), Style.Color(StyleColor.ToolbarBg));
        }

        // ------------------------------------------------------------
        // Header
        // ------------------------------------------------------------
        private static void DrawHeader(ObjectDriver? driver)
        {
            if (driver is null)
            {
                Gui.Label("Open a composition object (.xco) to edit it.");
                return;
            }
            using (Gui.Horizontal(EditorStyle.BarGap))
            {
                using (var icon = Gui.Canvas("##icon", new Vector2(16.0f, 18.0f)))
                    icon.Draw.Icon(Icon.Object, new Vector2(8.0f, 9.0f), 14.0f, Style.Color(StyleColor.IconWarning));
                using (Gui.Font(FontStyle.Bold))
                    Gui.Label(driver.Name);
                if (driver.HasPendingChanges)
                {
                    using (var dot = Gui.Canvas("##unsaved", new Vector2(8.0f, 18.0f)))
                        dot.Draw.CircleFilled(new Vector2(4.0f, 9.0f), 3.5f, Style.Color(StyleColor.IconWarning));
                    Gui.Tooltip("Unsaved changes");
                }
                MaterialEditorView.DrawStatusChip(driver.Guid);
                using (Gui.Font(FontStyle.Mono))
                {
                    Vector2 size = Gui.MeasureText(driver.Path);
                    using var path = Gui.Canvas("##path", new Vector2(size.x + 4.0f, 18.0f));
                    path.Draw.Text(new Vector2(0.0f, MathF.Round(9.0f - size.y * 0.5f)), Style.Color(StyleColor.TextDim), driver.Path);
                }
                Gui.FlexibleSpace();
                List<string> problems = driver.Problems();
                if (problems.Count > 0)
                {
                    Gui.Label($"{problems.Count} problem(s)");
                    Gui.Tooltip(string.Join("\n", problems));
                }
            }
        }

        // ------------------------------------------------------------
        // Tools
        // ------------------------------------------------------------
        private void DrawTools(ObjectDriver? driver)
        {
            using (Gui.Card("##object_tools", "Tools", Icon.Settings, null, Style.Color(StyleColor.Icon)))
            {
                bool dirty = driver?.HasPendingChanges == true;
                bool hasSelection = driver?.Find(_selected) is not null;
                float width = MathF.Max(Gui.AvailableSize.x, 120.0f);
                float half = width * 0.5f;
                float step = RowHeight + 2.0f;
                using var canvas = Gui.Canvas("##object_tool_buttons", new Vector2(width, step * 5.0f + 6.0f));
                Painter draw = canvas.Draw;
                float y = 0.0f;
                bool save = ToggleButton.Draw(draw, "##save", Rect.FromXYWH(1.0f, y, half - 2.0f, RowHeight), Icon.Save, null, "Save", null,
                    dirty ? "Write the object and rebuild it" : "No unsaved changes", false, dirty, dirty ? ToggleDot.Accent : ToggleDot.None).Clicked();
                bool revert = ToggleButton.Draw(draw, "##revert", Rect.FromXYWH(half + 1.0f, y, half - 2.0f, RowHeight), Icon.None, null, "Revert", null,
                    "Drop the unsaved changes", false, dirty, ToggleDot.None).Clicked();
                y += step;
                bool saveAs = ToggleButton.Draw(draw, "##save_as", Rect.FromXYWH(1.0f, y, width - 2.0f, RowHeight), Icon.File, null, "Save as...", null,
                    "Save a copy and edit it", false, driver is not null, ToggleDot.None).Clicked();
                y += step + 3.0f;

                Rect segment = Rect.FromXYWH(0.0f, y - 1.0f, width, RowHeight + 2.0f);
                draw.RectFilled(segment, Style.Color(StyleColor.SegmentBg), 3.0f);
                float cell = (width - 2.0f) / 4.0f;
                (Icon Icon, string Tip, bool Enabled)[] tools =
                {
                    (Icon.Hand, "Select", true),
                    (Icon.Move, "Move (gizmos come later; edit the Jointer pose)", false),
                    (Icon.Rotate, "Rotate (gizmos come later; edit the Jointer pose)", false),
                    (Icon.Scale, "Scale (gizmos come later; edit the Jointer pose)", false),
                };
                for (int i = 0; i < tools.Length; i++)
                    ToggleButton.Draw(draw, "##tool" + i, Rect.FromXYWH(1.0f + i * cell, y, cell - 2.0f, RowHeight), tools[i].Icon, null, null, null,
                        tools[i].Tip, i == 0, tools[i].Enabled, ToggleDot.None);
                y += step + 3.0f;

                bool addMesh = ToggleButton.Draw(draw, "##add_mesh", Rect.FromXYWH(1.0f, y, half - 2.0f, RowHeight), Icon.Mesh, Style.Color(StyleColor.IconComponent),
                    "Add mesh", null, "Add a Mesh Object under the selected Object", false, driver is not null, ToggleDot.None).Clicked();
                bool addProxy = ToggleButton.Draw(draw, "##add_proxy", Rect.FromXYWH(half + 1.0f, y, half - 2.0f, RowHeight), Icon.Gizmo, new Color32(230, 160, 208, 255),
                    "Add proxy", null, "Add a proxy (a named point) under the selected Object", false, driver is not null, ToggleDot.None).Clicked();
                y += step;
                bool remove = ToggleButton.Draw(draw, "##remove", Rect.FromXYWH(1.0f, y, width - 2.0f, RowHeight), Icon.Trash, null, "Remove", null,
                    "Remove the selected Object and its children", false, hasSelection, ToggleDot.None).Clicked();

                if (driver is null)
                    return;
                if (save && dirty)
                    driver.Apply();
                else if (revert && dirty)
                    driver.Revert();
                else if (saveAs)
                {
                    _saveAsName.Text = driver.Name + " copy";
                    _saveAsError = null;
                    _openSaveAs = true;
                }
                else if (addMesh || addProxy)
                {
                    ObjectNodeAsset node = driver.Add(addMesh ? ObjectNodeKind.Mesh : ObjectNodeKind.Proxy, _selected ?? string.Empty);
                    if (_selected is not null)
                        _collapsed.Remove(_selected);
                    _selected = node.Id;
                }
                else if (remove && _selected is not null)
                {
                    string parent = driver.Find(_selected)?.Parent ?? string.Empty;
                    driver.Remove(_selected);
                    _selected = parent.Length > 0 ? parent : null;
                }
            }
            Gui.Space(EditorStyle.BarGap);
        }

        // ------------------------------------------------------------
        // Objects tree
        // ------------------------------------------------------------
        private void Flatten(ObjectDriver driver, string parent, int depth, HashSet<string> seen)
        {
            foreach (ObjectNodeAsset node in driver.Children(parent))
            {
                if (!seen.Add(node.Id))
                    continue;
                bool hasChildren = driver.Children(node.Id).Any();
                _rows.Add((node, depth, hasChildren));
                if (hasChildren && !_collapsed.Contains(node.Id))
                    Flatten(driver, node.Id, depth + 1, seen);
            }
        }

        private void DrawTree(ObjectDriver? driver)
        {
            using var card = Gui.Card("##object_tree", "Objects", Icon.Hierarchy, null, Style.Color(StyleColor.Icon));
            if (driver is null)
                return;
            _rows.Clear();
            Flatten(driver, string.Empty, 0, new HashSet<string>(StringComparer.OrdinalIgnoreCase));
            _list.Count = _rows.Count;
            _list.RowHeight = RowHeight;
            _list.Flags = ListFlags.NoStripes;
            using ListScope list = Gui.List("##object_rows", ref _list, new Vector2(0.0f, MathF.Max(_rows.Count, 1) * RowHeight + 6.0f));
            if (!list.IsOpen)
                return;
            Color32 text = Style.Color(StyleColor.Text);
            Color32 textSelected = Style.Color(StyleColor.TextSelected);
            float textY = (RowHeight - Gui.MeasureText("A").y) * 0.5f;
            for (int i = _list.First; i < _list.Last && i < _rows.Count; ++i)
            {
                (ObjectNodeAsset node, int depth, bool hasChildren) = _rows[i];
                bool selected = string.Equals(node.Id, _selected, StringComparison.OrdinalIgnoreCase);
                bool expanded = !_collapsed.Contains(node.Id);
                TreeRowFlags flags = (hasChildren ? TreeRowFlags.HasChildren : TreeRowFlags.None)
                    | (expanded ? TreeRowFlags.Expanded : TreeRowFlags.None)
                    | (selected ? TreeRowFlags.Selected : TreeRowFlags.None);
                using TreeRowScope scope = Gui.TreeRow(i, depth, flags);
                if (scope.Toggled || (scope.DoubleClicked && hasChildren))
                {
                    if (!_collapsed.Remove(node.Id))
                        _collapsed.Add(node.Id);
                }
                else if (scope.Clicked)
                {
                    _selected = node.Id;
                }
                Painter draw = scope.Draw;
                if (selected)
                    EditorStyle.SelectionBar(draw, -(TreeArrowOffset + depth * TreeIndent), RowHeight);
                bool mesh = node.Kind == ObjectNodeKind.Mesh;
                draw.Icon(mesh ? Icon.Mesh : Icon.Gizmo, new Vector2(7.0f, RowHeight * 0.5f), 13.0f,
                    mesh ? Style.Color(StyleColor.IconComponent) : new Color32(230, 160, 208, 255));
                draw.Text(new Vector2(18.0f, textY), selected ? textSelected : text, node.Name.Length > 0 ? node.Name : "(unnamed)");
                if (node.Expose != ObjectNodeExpose.None)
                {
                    float right = LeftWidth - 30.0f - (TreeArrowOffset + depth * TreeIndent);
                    bool actor = node.Expose == ObjectNodeExpose.Actor;
                    draw.Icon(actor ? Icon.Object : Icon.Gizmo, new Vector2(right, RowHeight * 0.5f), 12.0f,
                        actor ? new Color32(143, 209, 158, 255) : new Color32(230, 160, 208, 255));
                }
            }
        }

        // ------------------------------------------------------------
        // Preview
        // ------------------------------------------------------------
        private void DrawPreview(ObjectDriver? driver, float height)
        {
            using var card = Gui.Card("##object_preview", "Preview", Icon.Sphere, null, Style.Color(StyleColor.Icon), ref _previewOpen);
            if (!card.IsOpen)
                return;
            float width = MathF.Max(Gui.AvailableSize.x, 64.0f);
            _preview.OnGUI(driver, _selected, new Vector2(width, MathF.Max(height - 52.0f, 80.0f)));
        }

        // ------------------------------------------------------------
        // Properties (tab strip + cards)
        // ------------------------------------------------------------
        private void DrawTabs(TabEdge edge, float height)
        {
            using var canvas = Gui.Canvas("##object_tabs", new Vector2(StripSize, height));
            Painter draw = canvas.Draw;
            draw.RectFilled(new Rect(new Vector2(0.0f, 0.0f), canvas.Size), Style.Color(StyleColor.TopBarBg));
            float iconX = edge == TabEdge.Left
                ? TabStrip.AccentWidth + (StripSize - TabStrip.AccentWidth) * 0.5f
                : (StripSize - TabStrip.AccentWidth) * 0.5f;
            float y = TabGap + 2.0f;
            for (int i = 0; i < _tabNames.Length; i++)
            {
                Rect tab = Rect.FromXYWH(0.0f, y, StripSize, TabSize);
                Result hit;
                using (Gui.Id(i))
                    hit = Gui.HitArea("##tab", tab);
                Gui.Tooltip(_tabNames[i]);
                if (hit.Clicked())
                    Tab = i;
                TabStrip.DrawShape(draw, tab, edge, i == Tab, hit.Hovered());
                Color32 color = i == 3 ? Style.Color(StyleColor.IconComponent) : Style.Color(StyleColor.Icon);
                draw.Icon(_tabIcons[i], new Vector2(iconX, y + TabSize * 0.5f), 15.0f, color);
                y += TabSize + TabGap;
                if (i == 0)
                {
                    y += 2.0f;
                    draw.Line(new Vector2(6.0f, y), new Vector2(StripSize - 6.0f, y), Style.Color(StyleColor.Border));
                    y += 3.0f;
                }
            }
        }

        private void DrawProperties(ObjectDriver? driver)
        {
            if (driver is null)
                return;
            if (driver.Error is not null)
            {
                Gui.Label(driver.Error);
                return;
            }
            if (driver.Find(_selected) is not ObjectNodeAsset node)
            {
                Gui.Label("Select an Object in the tree.");
                return;
            }
            using (Gui.Horizontal(EditorStyle.BarGap))
            {
                using (var icon = Gui.Canvas("##kind", new Vector2(16.0f, 18.0f)))
                    icon.Draw.Icon(node.Kind == ObjectNodeKind.Mesh ? Icon.Mesh : Icon.Gizmo, new Vector2(8.0f, 9.0f), 14.0f,
                        node.Kind == ObjectNodeKind.Mesh ? Style.Color(StyleColor.IconComponent) : new Color32(230, 160, 208, 255));
                using (Gui.Font(FontStyle.Bold))
                    Gui.Label(node.Name);
                Gui.FlexibleSpace();
                Gui.Label(node.Kind == ObjectNodeKind.Mesh ? "Mesh object" : "Proxy");
            }
            Gui.Space(EditorStyle.BarGap);
            if (Tab == 0 || Tab == 1)
                DrawNaming(driver, node);
            if (Tab == 0 || Tab == 2)
                DrawJointer(driver, node);
            if (Tab == 0 || Tab == 3)
                DrawMesh(driver, node);
            if (Tab == 0 || Tab == 4)
                DrawComponents(driver, node);
        }

        private void DrawNaming(ObjectDriver driver, ObjectNodeAsset node)
        {
            using var card = Gui.Card("##object_naming", "Naming", Icon.Tag, null, Style.Color(StyleColor.Icon));
            if (_nameFor != node.Id)
            {
                _nameFor = node.Id;
                _name.Text = node.Name;
            }
            if (Gui.Field("Name", _name) && _name.Text != node.Name)
            {
                node.Name = _name.Text;
                driver.Changed();
            }
            float width = MathF.Max(Gui.AvailableSize.x, 160.0f);
            float labelWidth = MathF.Round(width * 0.4f);
            using var canvas = Gui.Canvas("##expose", new Vector2(width, RowHeight + 2.0f));
            Painter draw = canvas.Draw;
            draw.Text(new Vector2(0.0f, MathF.Round((RowHeight - Gui.MeasureText("A").y) * 0.5f) + 1.0f), Style.Color(StyleColor.Text), "Expose");
            Rect segment = Rect.FromXYWH(labelWidth, 0.0f, width - labelWidth, RowHeight + 2.0f);
            draw.RectFilled(segment, Style.Color(StyleColor.SegmentBg), 3.0f);
            string[] labels = { "None", "Actor", "Proxy" };
            string[] tips =
            {
                "Internal: drawn, not visible outside the composition",
                "A Composition Actor: shown in the Hierarchy, can take components",
                "A Composition Proxy: a named point scripts can query",
            };
            float cell = (segment.Width - 2.0f) / labels.Length;
            for (int i = 0; i < labels.Length; i++)
            {
                bool on = (int)node.Expose == i;
                if (ToggleButton.Draw(draw, "##expose" + i, Rect.FromXYWH(segment.min.x + 1.0f + i * cell, 1.0f, cell - 2.0f, RowHeight), Icon.None, null,
                        labels[i], null, tips[i], on, true, ToggleDot.None).Clicked() && !on)
                {
                    node.Expose = (ObjectNodeExpose)i;
                    driver.Changed();
                }
            }
        }

        private void DrawJointer(ObjectDriver driver, ObjectNodeAsset node)
        {
            using var card = Gui.Card("##object_jointer", "Jointer", Icon.Transform, null, Style.Color(StyleColor.Icon));
            List<ObjectNodeAsset> parents = driver.Draft.Objects
                .Where(o => !string.Equals(o.Id, node.Id, StringComparison.OrdinalIgnoreCase) && !driver.IsDescendant(o.Id, node.Id)).ToList();
            List<string> names = new() { "None (root)" };
            names.AddRange(parents.Select(p => p.Name));
            int index = parents.FindIndex(p => string.Equals(p.Id, node.Parent, StringComparison.OrdinalIgnoreCase)) + 1;
            if (Gui.Combo("Parent", ref index, names))
                driver.SetParent(node.Id, index <= 0 ? string.Empty : parents[index - 1].Id);
            Gui.Tooltip("Reparenting keeps the Object's place in the composition");
            Gui.PropertyLabel("Type", "Fixed");

            NumberFieldStyle xyz = new() { NotchTags = "x,y,z" };
            Span<float> position = stackalloc float[3] { node.Position[0], node.Position[1], node.Position[2] };
            if (Gui.Field("Position", position, xyz, 0.01f))
            {
                node.Position = position.ToArray();
                driver.Changed();
            }
            if (_eulerFor != node.Id || !node.Rotation.AsSpan().SequenceEqual(_eulerQuaternion))
            {
                _eulerFor = node.Id;
                _eulerQuaternion = (float[])node.Rotation.Clone();
                _euler = Transform.ToEuler(new Quaternion(node.Rotation[0], node.Rotation[1], node.Rotation[2], node.Rotation[3]));
            }
            NumberFieldStyle angles = new() { NotchTags = "x,y,z", Flags = NumberFieldFlags.Range, Min = -180.0f, Max = 180.0f };
            Span<float> rotation = stackalloc float[3] { _euler.x, _euler.y, _euler.z };
            if (Gui.Field("Rotation", rotation, angles, 0.0f))
            {
                _euler = new Vector3(rotation[0], rotation[1], rotation[2]);
                Quaternion q = Transform.FromEuler(_euler);
                node.Rotation = new[] { q.x, q.y, q.z, q.w };
                _eulerQuaternion = (float[])node.Rotation.Clone();
                driver.Changed();
            }
            Span<float> scale = stackalloc float[3] { node.Scale[0], node.Scale[1], node.Scale[2] };
            if (Gui.Field("Scale", scale, xyz, 0.01f))
            {
                node.Scale = scale.ToArray();
                driver.Changed();
            }
            Gui.PropertyLabel("Bone", node.Bone.Length > 0 ? node.Bone : "None (later)");
        }

        private void DrawMesh(ObjectDriver driver, ObjectNodeAsset node)
        {
            using var card = Gui.Card("##object_mesh", "Mesh and material", Icon.Mesh, null, Style.Color(StyleColor.IconComponent));
            if (node.Kind != ObjectNodeKind.Mesh)
            {
                Gui.Label("A proxy draws nothing.");
                return;
            }
            string model = node.Model;
            if (_modelPicker.Field("model", "Model", "mesh", ref model))
                driver.SetModel(node, model);
            IReadOnlyList<AssetItem> submeshes = AssetMeta.IsValidGuid(node.Model) ? driver.SubmeshesOf(node.Model) : Array.Empty<AssetItem>();
            if (submeshes.Count == 0)
            {
                Gui.Label(AssetMeta.IsValidGuid(node.Model) ? "The model has no submeshes yet (build it)." : "Pick a model.");
                return;
            }
            float width = MathF.Max(Gui.AvailableSize.x, 160.0f);
            using (var header = Gui.Canvas("##table_header", new Vector2(width, 20.0f)))
            {
                header.Draw.RectFilled(Rect.FromXYWH(0.0f, 0.0f, width, 20.0f), Style.Color(StyleColor.PanelHeaderBg), 2.0f);
                Color32 dim = Style.Color(StyleColor.TextDim);
                float textY = MathF.Round(10.0f - Gui.MeasureText("A").y * 0.5f);
                header.Draw.Text(new Vector2(26.0f, textY), dim, "Submesh");
                header.Draw.Text(new Vector2(MathF.Round(width * 0.4f) + 4.0f, textY), dim, "Material");
            }
            List<string> editable = new();
            foreach (AssetItem item in submeshes)
            {
                ObjectPartAsset? part = node.Parts.FirstOrDefault(p => string.Equals(p.Submesh, item.Guid, StringComparison.OrdinalIgnoreCase));
                bool included = part is not null;
                using (Gui.Id(item.Guid))
                using (Gui.Horizontal(2.0f))
                {
                    using (var check = Gui.Canvas("##include", new Vector2(22.0f, RowHeight)))
                    {
                        Result hit = Gui.HitArea("##include_hit", Rect.FromXYWH(2.0f, 2.0f, 18.0f, 18.0f));
                        Gui.Tooltip(included ? "Drawn (click to leave it out)" : "Not drawn (click to draw it)");
                        DrawCheck(check.Draw, Rect.FromXYWH(4.0f, 4.0f, 14.0f, 14.0f), included, hit.Hovered());
                        if (hit.Clicked())
                            driver.SetPartIncluded(node, item.Guid, !included);
                    }
                    if (!_materialPickers.TryGetValue(item.Guid, out AssetPicker? picker))
                    {
                        picker = new AssetPicker();
                        _materialPickers[item.Guid] = picker;
                    }
                    string material = part?.Material ?? string.Empty;
                    string label = item.Path.Contains('/') ? item.Path[(item.Path.LastIndexOf('/') + 1)..] : item.Path;
                    if (picker.Field("material", label, "material", ref material))
                        driver.SetPartMaterial(node, item.Guid, material);
                    if (AssetMeta.IsValidGuid(material) && !editable.Contains(material))
                        editable.Add(material);
                }
            }
            DrawMaterialEditor(editable);
        }

        private static void DrawCheck(Painter draw, Rect box, bool on, bool hovered)
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

        // The Material Editor view mounted under the table, for one of the parts' materials.
        private void DrawMaterialEditor(List<string> materials)
        {
            if (materials.Count == 0 || ProjectService.Assets is not { } assets)
                return;
            List<string> names = new() { "None" };
            names.AddRange(materials.Select(AssetBrowser.NameOf));
            int index = materials.IndexOf(_editMaterial) + 1;
            if (Gui.Combo("Edit here", ref index, names))
                _editMaterial = index <= 0 ? string.Empty : materials[index - 1];
            Gui.Tooltip("Edit one of these materials without leaving the object (saves the .xcm)");
            if (!materials.Contains(_editMaterial) || assets.PathOf(_editMaterial) is not string path)
            {
                if (_materialView.Driver is not null)
                {
                    _materialView.Driver.Dispose();
                    _materialView.Driver = null;
                }
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

        // Authored defaults: one card per component, its fields drawn on a prototype instance (made from the stored
        // values) and written back in the capture form whenever they change.
        private void DrawComponents(ObjectDriver driver, ObjectNodeAsset node)
        {
            using var card = Gui.Card("##object_components", "Components", Icon.Component, null, Style.Color(StyleColor.Icon));
            if (node.Expose != ObjectNodeExpose.Actor)
            {
                Gui.Label("Expose this Object as an Actor to give it components.");
                return;
            }
            JsonArray entries = node.Components ??= new JsonArray();
            int remove = -1;
            for (int i = 0; i < entries.Count; i++)
            {
                if (entries[i] is not JsonObject entry)
                    continue;
                string key = entry["type"]?.GetValue<string>() ?? string.Empty;
                ComponentType? type = ComponentCatalog.Types.FirstOrDefault(t => t.Key == key);
                using (Gui.Id(i))
                {
                    Icon icon = Icon.Script;
                    using (var row = Gui.Card("##authored", type?.Name ?? key, icon, null, Style.Color(StyleColor.IconData)))
                    {
                        if (type is null)
                        {
                            Gui.Label("This type isn't loaded (kept as authored).");
                        }
                        else if (Prototype(node.Id, i, type.Type, entry) is { } prototype)
                        {
                            EditorGUI.ObjectFields(prototype.Serialized);
                            string captured = ReloadState.CaptureFields(prototype.Serialized.Target).ToJsonString();
                            if (captured != prototype.Fields)
                            {
                                entry["fields"] = JsonNode.Parse(captured);
                                prototype.Fields = captured;
                                prototype.Stored = captured;
                                driver.Changed();
                            }
                        }
                        if (Gui.Button("Remove"))
                            remove = i;
                    }
                }
            }
            if (remove >= 0)
            {
                entries.RemoveAt(remove);
                _prototypes.Clear();
                driver.Changed();
            }
            if (Gui.Button("Add component"))
            {
                ComponentCatalog.Invalidate();
                _componentSearch.Text = string.Empty;
                Gui.OpenPopup("object_add_component");
            }
            Rect button = Gui.ItemRect;
            DrawAddComponent(driver, node, new Vector2(button.min.x, button.max.y + 2.0f));
        }

        private sealed class AuthoredPrototype
        {
            public required SerializedObject Serialized { get; init; }
            // Fields: the prototype's values as last captured; Stored: the entry's JSON it was made from.
            public required string Fields { get; set; }
            public required string Stored { get; set; }
        }

        private AuthoredPrototype? Prototype(string node, int index, Type type, JsonObject entry)
        {
            string key = $"{node}#{index}#{type.FullName}";
            string stored = entry["fields"]?.ToJsonString() ?? "{}";
            if (_prototypes.TryGetValue(key, out AuthoredPrototype? cached) && cached.Stored == stored)
                return cached;
            object instance;
            try
            {
                instance = Activator.CreateInstance(type)!;
            }
            catch (Exception e) when (e is MissingMethodException or TargetInvocationException or MemberAccessException)
            {
                return null;
            }
            ReloadState.ApplyFields(instance, entry["fields"] as JsonObject);
            AuthoredPrototype prototype = new() { Serialized = new SerializedObject(instance), Fields = ReloadState.CaptureFields(instance).ToJsonString(), Stored = stored };
            _prototypes[key] = prototype;
            return prototype;
        }

        private void DrawAddComponent(ObjectDriver driver, ObjectNodeAsset node, Vector2 position)
        {
            using var popup = Gui.Popup("object_add_component", position, new Vector2(240.0f, 0.0f));
            if (!popup.IsOpen)
                return;
            Gui.Space(4.0f);
            using (Gui.Horizontal())
            {
                Gui.Space(2.0f);
                Gui.SearchField("##component_search", _componentSearch, 0.0f);
                Gui.Space(2.0f);
            }
            string filter = _componentSearch.Text.Trim();
            List<ComponentType> types = ComponentCatalog.Types
                .Where(t => t.Type != typeof(Transform) && (filter.Length == 0 || t.Name.Contains(filter, StringComparison.OrdinalIgnoreCase))).ToList();
            float height = types.Count * 25.0f + 6.0f;
            using var list = Gui.ScrollArea("##component_list", new Vector2(240.0f, MathF.Min(height, 220.0f)));
            if (!list.IsOpen)
                return;
            float width = MathF.Max(Gui.AvailableSize.x, 1.0f);
            using var canvas = Gui.Canvas("##component_items", new Vector2(width, height));
            Painter draw = canvas.Draw;
            for (int i = 0; i < types.Count; i++)
            {
                Rect row = Rect.FromXYWH(4.0f, 3.0f + i * 25.0f, width - 8.0f, 22.0f);
                Result hit;
                using (Gui.Id(i))
                    hit = Gui.HitArea("##component_item", row);
                Gui.Tooltip(types[i].Key);
                draw.RectFilled(row, Style.Color(hit.Hovered() ? StyleColor.ButtonHover : StyleColor.Button), 3.0f);
                float cy = row.min.y + row.Height * 0.5f;
                ComponentIcons.Get(types[i].Type).Draw(draw, new Vector2(row.min.x + 15.0f, cy), 14.0f);
                draw.Text(new Vector2(row.min.x + 30.0f, MathF.Round(cy - Gui.MeasureText(types[i].Name).y * 0.5f)), Style.Color(StyleColor.Text), types[i].Name);
                if (hit.Clicked())
                {
                    AddAuthored(driver, node, types[i]);
                    Gui.ClosePopup();
                }
            }
        }

        // Adds a component type to an actor Object's authored defaults (its default field values).
        public bool AddAuthored(ObjectDriver driver, ObjectNodeAsset node, ComponentType type)
        {
            JsonObject fields;
            try
            {
                fields = ReloadState.CaptureFields(Activator.CreateInstance(type.Type)!);
            }
            catch (Exception e) when (e is MissingMethodException or TargetInvocationException or MemberAccessException)
            {
                fields = new JsonObject();
            }
            (node.Components ??= new JsonArray()).Add(new JsonObject { ["type"] = type.Key, ["fields"] = fields });
            driver.Changed();
            return true;
        }

        // ------------------------------------------------------------
        // Timeline (a future feature)
        // ------------------------------------------------------------
        private void DrawTimeline()
        {
            using var card = Gui.Card("##object_timeline_card", "Timeline (future)", Icon.Clock, null, Style.Color(StyleColor.TextDim), ref _timelineOpen);
            if (card.IsOpen)
                Gui.Label("Animation and VFX tracks per Object will play here.");
        }

        // ------------------------------------------------------------
        // Save as
        // ------------------------------------------------------------
        private void DrawSaveAs(ObjectDriver? driver)
        {
            if (_openSaveAs)
            {
                _openSaveAs = false;
                Gui.OpenPopup(SaveAsModalId);
            }
            using var modal = Gui.Modal(SaveAsModalId, "Save object as", new Vector2(320.0f, 0.0f));
            if (!modal.IsOpen || driver is null)
                return;
            Gui.Field("Name", _saveAsName);
            if (_saveAsError is not null)
                Gui.Label(_saveAsError);
            using (Gui.Horizontal())
            {
                Gui.FlexibleSpace();
                if (Gui.Button("Cancel"))
                    Gui.CloseModal();
                if (Gui.Button("Save"))
                {
                    string name = _saveAsName.Text.Trim();
                    if (!name.EndsWith(ObjectAsset.Extension, StringComparison.OrdinalIgnoreCase))
                        name += ObjectAsset.Extension;
                    string folder = driver.Path[..Math.Max(driver.Path.LastIndexOf('/'), 0)];
                    string path = folder + "/" + name;
                    _saveAsError = AssetDatabase.ValidateName(name)
                        ?? (ProjectService.Assets is { } assets && File.Exists(assets.FullPath(path)) ? "An asset with that name exists" : null);
                    if (_saveAsError is null && driver.SaveAs(path))
                    {
                        Gui.CloseModal();
                        SavedAs?.Invoke(path);
                    }
                }
            }
        }

        public void Dispose()
        {
            _preview.Dispose();
            _materialView.Driver?.Dispose();
            _materialView.Dispose();
        }
    }
}
