// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.

using xg;
using xg.imui;
using xg.math;

namespace xgEditor.CoreCLR
{
    // Editor Hierarchy: the engine's actor tree (every module's actors, read through ActorTree), with expand/collapse,
    // selection and a name search that shows matches as a flat list.
    public sealed class HierarchyPanel : Panel
    {
        private const float RowHeight = 20.0f;

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
        private bool _selectedSeen;
        private bool _expandAll;
        private string? _selectName;

        public override string Title => "Hierarchy";
        public override string? MenuPath => "General/Hierarchy";
        public override DockSlot Slot => DockSlot.Left;
        public override Icon PanelIcon => Icon.Hierarchy;
        public override PanelFlags Flags => PanelFlags.NoPadding | PanelFlags.NoScrollbar;

        public event Action<ActorID?>? SelectionChanged;

        public ActorID? Selected => _selected == 0 ? null : new ActorID(_selected);

        public void Select(ActorID? actor)
        {
            ulong id = actor?.Value ?? 0;
            if (id == _selected)
                return;
            _selected = id;
            SelectionChanged?.Invoke(Selected);
        }

        // Expands every actor with children on the next frame (used by the XG_UI_TEST_FOCUS=hierarchy hook).
        public void ExpandAll() => _expandAll = true;

        // Selects the first actor with this name on the next frame (used by the XG_UI_TEST_FOCUS=hierarchy hook).
        public void SelectByName(string name) => _selectName = name;

        protected override void OnGUI()
        {
            using (Gui.Toolbar())
                Gui.SearchField("##hierarchy_search", _search, Math.Max(80.0f, Gui.AvailableSize.x - 4.0f));

            Rebuild();
            DrawTree();
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
                Select(null);
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
                bool selected = row.Id == _selected;
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
                draw.Icon(Icon.Object, new Vector2(7.0f, RowHeight * 0.5f), 13.0f, icon);
                draw.Text(new Vector2(18.0f, textY), selected ? textSelected : text, ActorTree.GetName(new ActorID(row.Id)));
            }
        }
    }
}
