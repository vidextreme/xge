// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.

using xg;
using xg.imui;
using xg.math;
using xg.editor;

namespace xgEditor.CoreCLR
{
    // Editor Hierarchy: the engine's actor tree (every module's actors, read through ActorTree), with expand/collapse,
    // selection and a name search that shows matches as a flat list.
    [PanelTool("Hierarchy", Icon = "Hierarchy", Unique = true, Priority = 10, Tooltip = "Actor tree of the scene")]
    public sealed class HierarchyPanel : Panel, IPanelTool
    {
        private const float RowHeight = 20.0f;
        // Where XG_ImUIBeginTreeRow puts the row's draw origin: after a 4 px margin, the depth indent and the 16 px arrow.
        private const float TreeArrowOffset = 20.0f;
        private const float TreeIndent = 14.0f;

        private readonly struct Row
        {
            public Row(ulong id, int depth, bool hasChildren)
            {
                Id = id;
                Depth = depth;
                HasChildren = hasChildren;
            }

            public ulong Id { get; }
            public int Depth { get; }
            public bool HasChildren { get; }
        }

        private readonly List<Row> _rows = new();
        private readonly List<ActorID> _roots = new();
        private readonly List<List<ActorID>> _childLists = new();
        private readonly HashSet<ulong> _expanded = new();
        private readonly TextBuffer _search = new(128);
        private ListState _list;
        private ulong _selected;
        private ulong _revealed;
        private bool _selectedSeen;
        private bool _expandAll;
        private string? _selectName;
        private string? _laterName;
        private int _laterFrames;

        public override string Title => "Hierarchy";

        public int InstanceIndex { get; set; }

        public ToolUsability OnSelectItems(object[] selection) => ToolUsability.Usable;
        public override string? MenuPath => "General/Hierarchy";
        public override DockSlot Slot => DockSlot.Left;
        public override Icon PanelIcon => Icon.Hierarchy;
        public override PanelFlags Flags => PanelFlags.NoPadding | PanelFlags.NoScrollbar;

        public ActorID Selected => Selection.ActiveActor;

        // Makes the actor the whole selection (InvalidID clears it). The editor selection lives in Selection, which also
        // raises the select/deselect events.
        public void Select(ActorID actor)
        {
            if (actor == ActorID.InvalidID)
                Selection.Clear();
            else
                Selection.Set(actor);
            _selected = Selection.ActiveActor;
        }

        // Expands every actor with children on the next frame (used by the XG_UI_TEST_FOCUS=hierarchy hook).
        public void ExpandAll() => _expandAll = true;

        // Selects the first actor with this name on the next frame (used by the XG_UI_TEST_FOCUS=hierarchy hook).
        public void SelectByName(string name) => _selectName = name;

        // Selects the first actor with this name after delayFrames frames (test hook).
        public void SelectByNameLater(string name, int delayFrames)
        {
            _laterName = name;
            _laterFrames = Math.Max(delayFrames, 0);
        }

        protected override void OnGUI()
        {
            if (_laterName is not null && _laterFrames-- <= 0)
            {
                _selectName = _laterName;
                _laterName = null;
            }
            _selected = Selection.ActiveActor;
            if (_selected != 0 && _selected != _revealed)
                Reveal(new ActorID(_selected));
            using (Gui.Toolbar())
                Gui.SearchField("##hierarchy_search", _search, 0.0f);

            Rebuild();
            DrawTree();
        }

        // Expands the ancestors of a newly selected actor, so a selection made elsewhere shows in the tree.
        private void Reveal(ActorID actor)
        {
            _revealed = actor.Value;
            ActorID? parent = ActorTree.GetParent(actor);
            for (int guard = 0; parent is { } p && p.Value != 0 && guard < 256; guard++)
            {
                _expanded.Add(p.Value);
                parent = ActorTree.GetParent(p);
            }
        }

        private void Rebuild()
        {
            _rows.Clear();
            _selectedSeen = false;
            string search = _search.Text;
            ActorTree.GetRoots(_roots);
            foreach (ActorID root in _roots)
                Visit(root, 0, true, search);
            _expandAll = false;
            _selectName = null;
            if (_selected != 0 && !_selectedSeen)
                Select(ActorID.InvalidID);
        }

        private void Visit(ActorID id, int depth, bool visible, string search)
        {
            List<ActorID> children = ChildList(depth);
            ActorTree.GetChildren(id, children);
            bool hasChildren = children.Count > 0;
            if (_expandAll && hasChildren)
                _expanded.Add(id);
            if (id.Value == _selected)
                _selectedSeen = true;

            if (_selectName is not null || search.Length > 0)
            {
                string name = ActorTree.GetName(id);
                if (_selectName is not null && name == _selectName)
                {
                    Select(id);
                    _selectedSeen = true;
                    _selectName = null;
                }
                if (search.Length > 0 && name.Contains(search, StringComparison.OrdinalIgnoreCase))
                    _rows.Add(new Row(id, 0, false));
            }

            if (search.Length == 0 && visible)
                _rows.Add(new Row(id, depth, hasChildren));

            bool childrenVisible = visible && _expanded.Contains(id);
            for (int i = 0; i < children.Count; ++i)
                Visit(children[i], depth + 1, childrenVisible, search);
        }

        private List<ActorID> ChildList(int depth)
        {
            while (_childLists.Count <= depth)
                _childLists.Add(new List<ActorID>());
            return _childLists[depth];
        }

        private void DrawTree()
        {
            _list.Count = _rows.Count;
            _list.RowHeight = RowHeight;
            _list.Flags = ListFlags.None;

            using ListScope list = Gui.List("##hierarchy_list", ref _list);
            if (!list.IsOpen)
                return;

            Color32 text = Style.Color(StyleColor.Text);
            Color32 textSelected = Style.Color(StyleColor.TextSelected);
            Color32 icon = Style.Color(StyleColor.IconObject);
            float textY = (RowHeight - Gui.MeasureText("A").y) * 0.5f;

            for (int i = _list.First; i < _list.Last && i < _rows.Count; ++i)
            {
                Row row = _rows[i];
                bool selected = Selection.Contains(new ActorID(row.Id));
                bool expanded = _expanded.Contains(row.Id);
                TreeRowFlags flags = (row.HasChildren ? TreeRowFlags.HasChildren : TreeRowFlags.None)
                    | (expanded ? TreeRowFlags.Expanded : TreeRowFlags.None)
                    | (selected ? TreeRowFlags.Selected : TreeRowFlags.None);

                using TreeRowScope scope = Gui.TreeRow(i, row.Depth, flags);
                if (scope.Toggled || (scope.DoubleClicked && row.HasChildren))
                {
                    if (!_expanded.Remove(row.Id))
                        _expanded.Add(row.Id);
                }
                else if (scope.Clicked)
                {
                    Select(new ActorID(row.Id));
                }

                Painter draw = scope.Draw;
                if (selected)
                    EditorStyle.SelectionBar(draw, -(TreeArrowOffset + row.Depth * TreeIndent), RowHeight);
                bool composed = ObjectDriverComponent.IsCompositionActor(new ActorID(row.Id));
                draw.Icon(Icon.Object, new Vector2(7.0f, RowHeight * 0.5f), 13.0f, composed ? new Color32(143, 209, 158, 255) : icon);
                string name = ActorTree.GetName(new ActorID(row.Id));
                draw.Text(new Vector2(18.0f, textY), selected ? textSelected : text, name);
                if (composed)
                {
                    float x = 18.0f + Gui.MeasureText(name).x + 8.0f;
                    draw.Icon(Icon.Lock, new Vector2(x, RowHeight * 0.5f), 11.0f, Style.Color(StyleColor.TextDim));
                    if (scope.Interaction.Hovered())
                        Gui.Tooltip("A Composition Actor: added and removed by its object (.xco); its transform follows the composition");
                }
            }
        }
    }
}
