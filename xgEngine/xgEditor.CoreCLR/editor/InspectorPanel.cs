// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.

using xg;
using xg.editor;
using xg.imui;
using xg.math;

namespace xgEditor.CoreCLR
{
    // Editor Inspector: the components of the actor selected in the Hierarchy as Ribbon-style cards, with fields drawn by
    // PropertyDrawers (built-in ones, or a project's own from its Editor scripts). A vertical tab strip on the side away
    // from the dock edge shows All (every card), one tab per component (its [ComponentIcon]) and + (Add Component).
    // Edits apply immediately to the live components, which may belong to any CoreCLR module.
    public sealed class InspectorPanel : Panel
    {
        private const float StripSize = 30.0f;
        private const float TabSize = 28.0f;
        private const float TabGap = 2.0f;
        private const float AddPopupWidth = 240.0f;
        private const float AddRowHeight = 22.0f;
        private const float AddListMaxHeight = 300.0f;
        private const string AddPopupId = "inspector_add";

        private readonly List<object> _components = new();
        private readonly Dictionary<object, SerializedObject> _serialized = new(ReferenceEqualityComparer.Instance);
        private readonly Dictionary<string, bool> _open = new();
        private readonly List<ComponentType> _filtered = new();
        private readonly TextBuffer _search = new(64);
        private ulong _shown;
        private int _tab;
        private bool _keepTab;
        private bool _selectLast;
        private Rect _addTab;
        private bool _openAdd;
        private int _openAddDelay = -1;
        private string? _pendingAddType;

        public override string Title => "Inspector";
        public override string? MenuPath => "General/Inspector";
        public override DockSlot Slot => DockSlot.Right;
        public override Icon PanelIcon => Icon.Inspector;
        public override PanelFlags Flags => PanelFlags.NoPadding | PanelFlags.NoScrollbar;

        // 0 = All, n = the n-th component.
        public int SelectedTab
        {
            get => _tab;
            set
            {
                _tab = Math.Max(value, 0);
                _keepTab = true;
            }
        }

        // Opens the Add Component flyout after delayFrames frames (test hook).
        public void OpenAddComponent(int delayFrames = 0) => _openAddDelay = Math.Max(delayFrames, 0);

        // Adds a component of the catalog type with this full name to the selected actor once one is selected (test hook).
        public void AddComponentWhenSelected(string fullName) => _pendingAddType = fullName;

        protected override void OnGUI()
        {
            ActorID? selected = Selection.ActiveActor;
            bool hasActor = selected is ActorID id && ActorTree.GetName(id).Length > 0;
            ActorID actor = hasActor ? selected!.Value : ActorID.InvalidID;
            if (!hasActor)
            {
                _serialized.Clear();
                _components.Clear();
                _shown = 0;
                _tab = 0;
            }
            else
            {
                if (actor.Value != _shown)
                {
                    _serialized.Clear();
                    _shown = actor.Value;
                    if (!_keepTab)
                        _tab = 0;
                    _keepTab = false;
                }
                if (_pendingAddType != null)
                {
                    AddPending(actor, _pendingAddType);
                    _pendingAddType = null;
                }
                ActorTree.GetComponents(actor, _components);
                if (_selectLast)
                {
                    _tab = _components.Count;
                    _selectLast = false;
                }
            }
            if (_tab > _components.Count)
                _tab = 0;
            if (_openAddDelay >= 0 && _openAddDelay-- == 0 && hasActor)
                _openAdd = true;

            bool stripRight = TabStrip.Detect(Gui.PanelPlacement) == PanelSide.Left;
            using (Gui.Horizontal(0.0f))
            {
                if (!stripRight)
                    DrawTabs(TabEdge.Left, hasActor);
                DrawContent(actor, hasActor);
                if (stripRight)
                    DrawTabs(TabEdge.Right, hasActor);
            }
            DrawAddPopup(actor, hasActor, stripRight);
        }

        // ------------------------------------------------------------
        // Tabs
        // ------------------------------------------------------------
        private void DrawTabs(TabEdge edge, bool hasActor)
        {
            using var canvas = Gui.Canvas("##inspector_tabs", new Vector2(StripSize, 0.0f));
            Painter draw = canvas.Draw;
            draw.RectFilled(new Rect(new Vector2(0.0f, 0.0f), canvas.Size), Style.Color(StyleColor.TopBarBg));
            float iconX = edge == TabEdge.Left
                ? TabStrip.AccentWidth + (StripSize - TabStrip.AccentWidth) * 0.5f
                : (StripSize - TabStrip.AccentWidth) * 0.5f;

            float y = TabGap + 2.0f;
            DrawTab(draw, 0, Rect.FromXYWH(0.0f, y, StripSize, TabSize), edge, iconX, "All components", icon => icon.Draw(draw, new Vector2(iconX, y + TabSize * 0.5f), 15.0f),
                new ComponentIcon(Icon.Cards, null, Style.Color(StyleColor.Icon)));
            y += TabSize + TabGap + 2.0f;
            draw.Line(new Vector2(6.0f, y), new Vector2(StripSize - 6.0f, y), Style.Color(StyleColor.Border));
            y += 3.0f;

            for (int i = 0; i < _components.Count; i++)
            {
                Type type = TypeBridge.ToEditor(_components[i].GetType());
                ComponentIcon icon = ComponentIcons.Get(type);
                float top = y;
                DrawTab(draw, i + 1, Rect.FromXYWH(0.0f, top, StripSize, TabSize), edge, iconX, SerializedProperty.Nicify(type.Name),
                    resolved => resolved.Draw(draw, new Vector2(iconX, top + TabSize * 0.5f), 15.0f), icon);
                y += TabSize + TabGap;
            }

            y += 4.0f;
            _addTab = Rect.FromXYWH(canvas.Bounds.min.x, canvas.Bounds.min.y + y, StripSize, TabSize);
            Rect add = Rect.FromXYWH(0.0f, y, StripSize, TabSize);
            Result hit;
            using (Gui.Id(-1))
                hit = Gui.HitArea("##add", add);
            Gui.Tooltip(hasActor ? "Add Component" : "Select an actor to add components");
            TabStrip.DrawShape(draw, add, edge, false, hasActor && hit.Hovered());
            Color32 plus = Style.Color(hasActor ? StyleColor.Text : StyleColor.TextDim);
            draw.Icon(Icon.Plus, new Vector2(iconX, y + TabSize * 0.5f), 14.0f, plus);
            if (hasActor && hit.Clicked())
                _openAdd = true;
        }

        private void DrawTab(Painter draw, int index, Rect tab, TabEdge edge, float iconX, string tooltip, Action<ComponentIcon> drawIcon, ComponentIcon icon)
        {
            Result hit;
            using (Gui.Id(index))
                hit = Gui.HitArea("##tab", tab);
            Gui.Tooltip(tooltip);
            if (hit.Clicked())
                _tab = index;
            TabStrip.DrawShape(draw, tab, edge, index == _tab, hit.Hovered());
            drawIcon(icon);
        }

        // ------------------------------------------------------------
        // Cards
        // ------------------------------------------------------------
        private void DrawContent(ActorID actor, bool hasActor)
        {
            using var area = Gui.ScrollArea("##inspector_content", default, ScrollAreaFlags.Padded);
            if (!area.IsOpen)
                return;
            Vector2 avail = Gui.AvailableSize;
            Gui.Draw.RectFilled(new Rect(new Vector2(-6.0f, -6.0f), new Vector2(avail.x + 6.0f, 100000.0f)), Style.Color(StyleColor.ToolbarBg));

            if (!hasActor)
            {
                Gui.Label("Select an actor in the Hierarchy.");
                return;
            }

            using (Gui.Horizontal())
            {
                using (Gui.Font(FontStyle.Bold))
                    Gui.Label(ActorTree.GetName(actor));
                Gui.FlexibleSpace();
                Gui.Label($"ID {actor.Value}");
            }
            Gui.Space(2.0f);

            if (_components.Count == 0)
            {
                Gui.Label("No components.");
                return;
            }

            for (int i = 0; i < _components.Count; i++)
            {
                if (_tab != 0 && _tab != i + 1)
                    continue;
                object component = _components[i];
                Type type = TypeBridge.ToEditor(component.GetType());
                ComponentIcon icon = ComponentIcons.Get(type);
                string title = SerializedProperty.Nicify(type.Name);
                using (Gui.Id(i))
                {
                    if (_tab == 0)
                    {
                        string key = $"{TypeBridge.KeyOf(type)}#{i}";
                        bool open = !_open.TryGetValue(key, out bool stored) || stored;
                        using (var card = Gui.Card("##card", title, icon.Icon, icon.Texture, icon.Color, ref open))
                        {
                            if (card.IsOpen)
                                DrawFields(component);
                        }
                        _open[key] = open;
                    }
                    else
                    {
                        using var card = Gui.Card("##card", title, icon.Icon, icon.Texture, icon.Color);
                        DrawFields(component);
                    }
                }
                Gui.Space(2.0f);
            }
        }

        private void DrawFields(object component)
        {
            if (!_serialized.TryGetValue(component, out SerializedObject? serialized))
            {
                serialized = new SerializedObject(component);
                _serialized[component] = serialized;
            }
            if (serialized.Properties.Count == 0)
                Gui.Label("No visible fields.");
            EditorGUI.ObjectFields(serialized);
        }

        // ------------------------------------------------------------
        // Add Component
        // ------------------------------------------------------------
        private void DrawAddPopup(ActorID actor, bool hasActor, bool stripRight)
        {
            if (_openAdd)
            {
                _openAdd = false;
                ComponentCatalog.Invalidate();
                _search.Text = string.Empty;
                Gui.OpenPopup(AddPopupId);
            }
            Vector2 position = stripRight
                ? new Vector2(_addTab.min.x - AddPopupWidth - 2.0f, _addTab.min.y)
                : new Vector2(_addTab.max.x + 2.0f, _addTab.min.y);
            using var popup = Gui.Popup(AddPopupId, position, new Vector2(AddPopupWidth, 0.0f));
            if (!popup.IsOpen)
                return;

            Gui.Space(4.0f);
            using (Gui.Horizontal())
            {
                Gui.Space(2.0f);
                Gui.SearchField("##add_search", _search, 0.0f);
                Gui.Space(2.0f);
            }

            string filter = _search.Text.Trim();
            _filtered.Clear();
            foreach (ComponentType type in ComponentCatalog.Types)
                if (filter.Length == 0 || type.Name.Contains(filter, StringComparison.OrdinalIgnoreCase))
                    _filtered.Add(type);

            if (_filtered.Count == 0)
            {
                Gui.Label(" No matching components.");
                Gui.Space(4.0f);
                return;
            }

            float listHeight = _filtered.Count * (AddRowHeight + 3.0f) + 6.0f;
            using var list = Gui.ScrollArea("##add_list", new Vector2(AddPopupWidth, MathF.Min(listHeight, AddListMaxHeight)));
            if (!list.IsOpen)
                return;
            float width = MathF.Max(Gui.AvailableSize.x, 1.0f);
            using var canvas = Gui.Canvas("##add_items", new Vector2(width, listHeight));
            Painter draw = canvas.Draw;
            ComponentType? picked = null;
            for (int i = 0; i < _filtered.Count; i++)
            {
                ComponentType type = _filtered[i];
                Rect row = Rect.FromXYWH(4.0f, 3.0f + i * (AddRowHeight + 3.0f), width - 8.0f, AddRowHeight);
                Result hit;
                using (Gui.Id(i))
                    hit = Gui.HitArea("##add_item", row);
                Gui.Tooltip(type.Key);
                Color32 fill = Style.Color(hit.Hovered() ? StyleColor.ButtonHover : StyleColor.Button);
                draw.RectFilled(row, Style.Color(StyleColor.SegmentBg), 3.0f);
                draw.RectFilled(new Rect(row.min, new Vector2(row.max.x, row.max.y - 1.0f)), fill, 3.0f);
                float cy = row.min.y + row.Height * 0.5f;
                ComponentIcons.Get(type.Type).Draw(draw, new Vector2(row.min.x + 15.0f, cy), 14.0f);
                draw.Text(new Vector2(row.min.x + 30.0f, MathF.Round(cy - Gui.MeasureText(type.Name).y * 0.5f)), Style.Color(StyleColor.Text), type.Name);
                if (hit.Clicked())
                    picked = type;
            }
            if (picked != null && hasActor)
            {
                if (ActorTree.AddComponent(actor, picked.Type))
                {
                    _selectLast = true;
                    Gui.ClosePopup();
                }
                else
                {
                    Debug.LogError($"[Inspector] Could not add {picked.Name} to {ActorTree.GetName(actor)}.");
                }
            }
        }

        // Test hook: the type may live in any module, listed in the catalog or not.
        private void AddPending(ActorID actor, string fullName)
        {
            foreach (System.Runtime.Loader.AssemblyLoadContext context in System.Runtime.Loader.AssemblyLoadContext.All)
            {
                foreach (System.Reflection.Assembly assembly in context.Assemblies)
                {
                    Type? type = assembly.IsDynamic ? null : assembly.GetType(fullName, false);
                    if (type is null)
                        continue;
                    bool added = ActorTree.AddComponent(actor, type);
                    Debug.Log($"[Debug] Inspector add {TypeBridge.KeyOf(type)} to {ActorTree.GetName(actor)}: {(added ? "added" : "failed")}");
                    if (added)
                        _selectLast = true;
                    return;
                }
            }
            Debug.LogWarning($"[Debug] Inspector add: no type named {fullName}");
        }
    }
}
