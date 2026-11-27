// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.

using xg;
using xg.imui;
using xg.math;
using xg.editor;

namespace xgEditor.CoreCLR
{
    // Editor Hierarchy: the engine's actor tree (every module's actors, read through ActorTree), with expand/collapse,
    // selection and a name search that shows matches as a flat list. Roots are grouped under a header per loaded scene
    // (its members; amber dot while unsaved), then "Not saved (runtime)" for actors no scene owns (script and test
    // actors). Scenes hold editor-only folders (SceneFolder) that group root actors and other folders. State columns on
    // the right (lock, then visibility; more can be added to _columns) are pinned while the name column clips and
    // scrolls sideways. A hidden or locked scene, folder or actor overrides its descendants, which keep their own state
    // (shown subdued). The + button and right-click open the scene menu (SceneContextMenu, VisibilityContextMenu).
    [PanelTool("Hierarchy", Icon = "Hierarchy", Unique = true, Priority = 10, Tooltip = "Actor tree of the scene")]
    public sealed class HierarchyPanel : Panel, IPanelTool
    {
        private const float RowHeight = 20.0f;
        // Where XG_ImUIBeginTreeRow puts the row's draw origin: after a 4 px margin, the depth indent and the 16 px arrow.
        private const float TreeArrowOffset = 20.0f;
        private const float TreeIndent = 14.0f;
        private const float CellWidth = 22.0f;
        private const float HeaderHeight = 20.0f;
        private const float ScrollBarHeight = 12.0f;
        // Tree lines start at this depth (scene headers and their direct rows draw none).
        private const int FirstLineDepth = 2;
        private const int MaxLineDepth = 63;

        private const string RuntimeGroup = "runtime";

        private enum RowKind
        {
            Actor,
            Folder,
            Group,
        }

        private struct Row
        {
            public RowKind Kind;
            public ulong Id;
            public int Depth;
            public bool HasChildren;
            // A group header row: a scene (Scene set) or the runtime group.
            public EditorScene? Scene;
            public string? Group;
            public SceneFolder? Folder;
            // Hidden or locked by an ancestor, folder or the scene (the row's own flag not included).
            public bool ParentHidden;
            public bool ParentLocked;
            // Index of the last visible row under this one (itself when it has none shown).
            public int LastDescendant;
            // No later sibling (its parent's tree line ends at this row), and the levels whose lines pass through it.
            public bool LastSibling;
            public ulong Guides;
        }

        // A state column on the right of every row: drawn into its cell, which also owns the click.
        private sealed class Column
        {
            public required Icon Icon;
            public required string Tooltip;
            public required bool IsLock;
            public float Width = CellWidth;
        }

        private readonly List<Row> _rows = new();
        private readonly List<ActorID> _roots = new();
        private readonly List<List<ActorID>> _childLists = new();
        private readonly HashSet<ulong> _expanded = new();
        private readonly HashSet<string> _collapsedGroups = new(StringComparer.OrdinalIgnoreCase);
        private readonly HashSet<string> _collapsedFolders = new(StringComparer.OrdinalIgnoreCase);
        private readonly ContextMenuHost _contextMenu = new("hierarchy_context");
        private readonly IconPicker _iconPicker = new("hierarchy_icons");
        private readonly int[] _chain = new int[MaxLineDepth + 2];
        private readonly TextBuffer _search = new(128);
        private readonly Column[] _columns =
        {
            new() { Icon = Icon.Lock, Tooltip = "Locked: selectable and browsable, but nothing edits it", IsLock = true },
            new() { Icon = Icon.Eye, Tooltip = "Visible in the Scene view (the game always draws it)", IsLock = false },
        };
        private ListState _list;
        private ulong _selected;
        private ulong _revealed;
        private bool _selectedSeen;
        private bool _expandAll;
        private string? _selectName;
        private string? _laterName;
        private int _laterFrames;
        private int _contextHookFrames;
        private static string? _pendingFolder;
        private string? _selectedFolder;
        private int _hoveredRow = -1;
        private float _listWidth;
        private float _nameWidth;
        private float _maxExtent;
        private float _scrollX;

        public override string Title => "Hierarchy";

        public int InstanceIndex { get; set; }

        public ToolUsability OnSelectItems(object[] selection) => ToolUsability.Usable;
        public override string? MenuPath => "General/Hierarchy";
        public override DockSlot Slot => DockSlot.Left;
        public override Icon PanelIcon => Icon.Hierarchy;
        public override PanelFlags Flags => PanelFlags.NoPadding | PanelFlags.NoScrollbar;

        public ActorID Selected => Selection.ActiveActor;

        private float ColumnsWidth => _columns.Sum(c => c.Width);

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
            if (_selected != 0)
                _selectedFolder = null;
            if (_pendingFolder is not null)
            {
                _selectedFolder = _pendingFolder;
                _pendingFolder = null;
            }
            if (_selected != 0 && _selected != _revealed)
                Reveal(new ActorID(_selected));
            SyncFolderTarget();
            DrawToolbar();

            Rebuild();
            DrawHeader();
            DrawTree();
            DrawScrollBar();
            SyncFolderTarget();
            ++_contextHookFrames;
            if (_contextHookFrames == 160 && Environment.GetEnvironmentVariable("XG_UI_TEST_HIERARCHY_SEARCH") is { Length: > 0 } searchHook)
                _search.Text = searchHook;
            if (_contextHookFrames == 150 && Environment.GetEnvironmentVariable("XG_UI_TEST_HIERARCHY_CONTEXT") is { Length: > 0 } hook)
                OpenContextHook(hook);
            if (_contextHookFrames == 170 && IconPicker.ParseHook() is { Target: "hierarchy" } iconHook)
                OpenIconHook(iconHook.Search, iconHook.Pick);
            _contextMenu.OnGUI();
            _iconPicker.OnGUI();
        }

        // Opens the icon picker for an actor (the whole selection when the actor is part of it) at the mouse cursor, or
        // at position (the panel's draw space).
        public void OpenIconPicker(ActorID actor, Vector2? position = null)
        {
            if (!EditorIcons.CanEdit(actor))
                return;
            ActorID[] targets = Selection.Contains(actor) ? Selection.Actors.ToArray() : new[] { actor };
            _iconPicker.Open(ActorEditorState.GetIcon(actor), Icon.Actor, icon => EditorIcons.SetActors(targets, icon), position);
        }

        public void OpenIconPicker(EditorScene scene, SceneFolder folder, Vector2? position = null)
        {
            if (SceneManager.IsFolderLocked(scene, folder.Id))
                return;
            _iconPicker.Open(folder.Icon, Icon.Group, icon => EditorIcons.SetFolder(scene, folder, icon), position);
        }

        // Test hook (XG_UI_TEST_ICON_PICKER=hierarchy[:search=<text>][:pick=<name>]): opens the picker for the selected
        // actor, else the selected group, at a fixed spot.
        private void OpenIconHook(string? search, string? pick)
        {
            Vector2 position = new(60.0f, 120.0f);
            if (Selection.ActiveActor is { } actor && actor.Value != 0)
                OpenIconPicker(actor, position);
            else if (SceneManager.FolderTarget is { } target)
                OpenIconPicker(target.Scene, target.Folder, position);
            else
            {
                Debug.LogWarning("[Debug] Icon picker hook: nothing selected in the Hierarchy");
                return;
            }
            _iconPicker.Hook(search, pick);
            Debug.Log("[Debug] Icon picker opened from the Hierarchy");
        }

        private void DrawToolbar()
        {
            using (Gui.Toolbar())
            {
                Gui.SearchField("##hierarchy_search", _search, 0.0f);
                if (ActorEditorState.ShowHidden)
                    DrawShownChip();
                if (Gui.IconButton("##hierarchy_folder", Icon.Group, 22.0f, HierarchyStyle.FolderIcon))
                    NewFolder();
                Gui.Tooltip("Add a group to the active scene (inside the selected group)");
                if (Gui.IconButton("##hierarchy_add", Icon.Plus, 22.0f, Style.Color(StyleColor.Text)))
                    _contextMenu.Open(new ContextMenuContext(SceneContextMenu.BackgroundSource, Array.Empty<object>(), this));
                Gui.Tooltip("Add actors to the scene");
            }
        }

        // The amber "hidden shown" chip while Show Hidden Actors is on.
        private static void DrawShownChip()
        {
            const string text = "hidden shown";
            float width = Gui.MeasureText(text).x + 30.0f;
            using var canvas = Gui.Canvas("##hidden_chip", new Vector2(width, 22.0f));
            Painter draw = canvas.Draw;
            Rect chip = Rect.FromXYWH(0.0f, 3.0f, width, 16.0f);
            draw.RectFilled(chip, new Color32(224, 168, 60, 46), 8.0f);
            Color32 amber = Style.Color(StyleColor.IconWarning);
            draw.Icon(Icon.Eye, new Vector2(11.0f, chip.Center.y), 11.0f, amber);
            draw.Text(new Vector2(20.0f, MathF.Round(chip.Center.y - Gui.MeasureText("A").y * 0.5f)), amber, text);
            Gui.Tooltip("Show Hidden Actors is on (Tools > Scene > Visibility): the Scene view draws hidden actors");
        }

        private void SyncFolderTarget()
        {
            SceneFolder? previous = SceneManager.FolderTarget?.Folder;
            SceneManager.FolderTarget = null;
            if (_selectedFolder is not null)
                foreach (EditorScene scene in SceneManager.Scenes)
                    if (scene.FindFolder(_selectedFolder) is { } folder)
                    {
                        SceneManager.FolderTarget = (scene, folder);
                        break;
                    }
            if (SceneManager.FolderTarget?.Folder != previous)
                Subsystem.Get<Tools>()?.Invalidate();
        }

        private void NewFolder()
        {
            EditorScene? scene = SceneManager.Active;
            string? parent = null;
            if (_selectedFolder is not null && SceneManager.Scenes.FirstOrDefault(s => s.FindFolder(_selectedFolder) is not null) is { } owner)
            {
                scene = owner;
                parent = _selectedFolder;
            }
            if (scene is not null && SceneManager.CreateFolder(scene, parent) is { } created)
            {
                _selectedFolder = created.Id;
                VisibilityContextMenu.RenameFolder(scene, created);
            }
        }

        // Test hook (XG_UI_TEST_HIERARCHY_CONTEXT=<actor name>|background|rename:<actor name>): opens the scene menu at a
        // fixed spot, or the Rename dialog for an actor.
        private void OpenContextHook(string target)
        {
            if (target.StartsWith("rename:", StringComparison.Ordinal))
            {
                foreach (Row row in _rows)
                    if (row.Kind == RowKind.Actor && ActorTree.GetName(new ActorID(row.Id)) == target[7..])
                        SceneContextMenu.RenameActor(new ActorID(row.Id));
                return;
            }
            ActorID? actor = null;
            foreach (Row row in _rows)
                if (row.Kind == RowKind.Actor && ActorTree.GetName(new ActorID(row.Id)) == target)
                    actor = new ActorID(row.Id);
            object[] targets = actor is { } found ? new object[] { found } : Array.Empty<object>();
            _contextMenu.Open(new ContextMenuContext(actor is null ? SceneContextMenu.BackgroundSource : SceneContextMenu.ItemSource, targets, this), new Vector2(60.0f, 120.0f));
        }

        // Test hook (XG_UI_TEST_HIERARCHY_STATE=hide:<actor>,lock:<actor>,select:<actor>,showhidden,folder:<folder>=<actor>,
        // hidefolder:<folder>,lockfolder:<folder>,selectfolder:<folder>,lockscene,hidescene,create:<name>,removeactor:<actor>,
        // removegroup:<folder>,removecomponent:<actor>=<type name>,icon:<actor>=<icon name or path>,save), applied in order 120 frames
        // after start (SceneManager.Update); save writes the active scene, so restore it.
        public static void StateHook(string spec)
        {
            foreach (string raw in spec.Split(',', StringSplitOptions.RemoveEmptyEntries | StringSplitOptions.TrimEntries))
            {
                int colon = raw.IndexOf(':');
                string verb = colon < 0 ? raw : raw[..colon];
                string arg = colon < 0 ? string.Empty : raw[(colon + 1)..];
                EditorScene? active = SceneManager.Active;
                switch (verb)
                {
                    case "hide" when FindActor(arg) is { } hidden:
                        ActorEditorState.SetHidden(new[] { hidden }, true);
                        break;
                    case "lock" when FindActor(arg) is { } locked:
                        ActorEditorState.SetLocked(new[] { locked }, true);
                        break;
                    case "select" when FindActor(arg) is { } chosen:
                        Selection.Set(chosen);
                        break;
                    case "showhidden":
                        ActorEditorState.ShowHidden = true;
                        break;
                    case "folder" when arg.Split('=') is [var folderName, var actorName]:
                    {
                        ActorID? root = FindActor(actorName);
                        EditorScene? scene = root is { } r ? SceneManager.SceneOf(r) ?? active : active;
                        if (scene is null)
                            break;
                        SceneFolder? folder = scene.Folders.FirstOrDefault(f => f.Name == folderName) ?? SceneManager.CreateFolder(scene, null, folderName);
                        if (folder is not null && root is { } moved)
                            SceneManager.MoveToFolder(moved, folder.Id);
                        break;
                    }
                    case "hidefolder" when FindFolder(arg) is { } hide:
                        SceneManager.SetFolderHidden(hide.Scene, hide.Folder, true);
                        break;
                    case "lockfolder" when FindFolder(arg) is { } lockFolder:
                        SceneManager.SetFolderLocked(lockFolder.Scene, lockFolder.Folder, true);
                        break;
                    case "selectfolder" when FindFolder(arg) is { } select:
                        Selection.Clear();
                        _pendingFolder = select.Folder.Id;
                        break;
                    case "lockscene" when active is not null:
                        SceneManager.SetSceneLocked(active, true);
                        break;
                    case "hidescene" when active is not null:
                        SceneManager.SetSceneHidden(active, true);
                        break;
                    case "icon" when arg.Split('=') is [var iconActor, var iconName] && FindActor(iconActor) is { } iconTarget:
                        EditorIcons.SetActors(new[] { iconTarget }, iconName.Length > 0 ? new EditorIcon(iconName, null) : null);
                        break;
                    case "save" when active is not null:
                        SceneManager.Save(active);
                        break;
                    case "removeactor" when FindActor(arg) is { } removed:
                        SceneCommands.ConfirmRemoveActors(SceneCommands.RemovableActors(new[] { removed }));
                        break;
                    case "removegroup" when FindFolder(arg) is { } group:
                        SceneCommands.ConfirmRemoveGroup(group.Scene, group.Folder);
                        break;
                    case "removecomponent" when arg.Split('=') is [var ownerName, var typeName] && FindActor(ownerName) is { } owner:
                    {
                        List<object> components = new();
                        ActorTree.GetComponents(owner, components);
                        if (components.FirstOrDefault(c => c.GetType().Name == typeName) is { } component)
                            ComponentContextMenu.ConfirmRemove(owner, component);
                        break;
                    }
                    case "create" when arg.Length > 0:
                    {
                        Actor? created = _pendingFolder is not null && active?.FindFolder(_pendingFolder) is { } into
                            ? SceneManager.CreateInFolder(active, into, () => SceneManager.CreateActor(arg))
                            : SceneManager.CreateActor(arg);
                        if (created is not null)
                            Debug.Log($"[Debug] Hierarchy created {arg} in group {(active?.FolderIdOf(created.ID) is { } id ? active.FindFolder(id)?.Name : "none")}");
                        break;
                    }
                    default:
                        Debug.LogWarning($"[Debug] Hierarchy state hook: nothing done for \"{raw}\"");
                        continue;
                }
                Debug.Log($"[Debug] Hierarchy state hook: {raw}");
            }
        }

        private static ActorID? FindActor(string name)
        {
            List<ActorID> roots = new();
            ActorTree.GetRoots(roots);
            Queue<ActorID> queue = new(roots);
            List<ActorID> children = new();
            while (queue.Count > 0)
            {
                ActorID actor = queue.Dequeue();
                if (ActorTree.GetName(actor) == name)
                    return actor;
                ActorTree.GetChildren(actor, children);
                foreach (ActorID child in children)
                    queue.Enqueue(child);
            }
            return null;
        }

        private static (EditorScene Scene, SceneFolder Folder)? FindFolder(string name)
        {
            foreach (EditorScene scene in SceneManager.Scenes)
                if (scene.Folders.FirstOrDefault(f => f.Name == name) is { } folder)
                    return (scene, folder);
            return null;
        }

        // Expands the ancestors (and folders) of a newly selected actor, so a selection made elsewhere shows in the tree.
        private void Reveal(ActorID actor)
        {
            _revealed = actor.Value;
            ActorID root = actor;
            ActorID? parent = ActorTree.GetParent(actor);
            for (int guard = 0; parent is { } p && p.Value != 0 && guard < 256; guard++)
            {
                _expanded.Add(p.Value);
                root = p;
                parent = ActorTree.GetParent(p);
            }
            if (SceneManager.SceneOf(root) is not { } scene)
                return;
            _collapsedGroups.Remove(scene.Guid);
            for (SceneFolder? folder = scene.FindFolder(scene.FolderIdOf(root)); folder is not null; folder = scene.FindFolder(folder.Parent))
                _collapsedFolders.Remove(folder.Id);
        }

        // ------------------------------------------------------------
        // Rows
        // ------------------------------------------------------------
        private void Rebuild()
        {
            _rows.Clear();
            _selectedSeen = false;
            string search = _search.Text;
            ActorTree.GetRoots(_roots);
            if (search.Length > 0)
            {
                foreach (EditorScene scene in SceneManager.Scenes)
                    foreach (SceneFolder folder in scene.Folders)
                        if (folder.Name.Contains(search, StringComparison.OrdinalIgnoreCase))
                        {
                            (bool parentHidden, bool parentLocked) = scene.FolderState(folder.Parent);
                            AddRow(new Row { Kind = RowKind.Folder, Scene = scene, Folder = folder, ParentHidden = parentHidden, ParentLocked = parentLocked });
                        }
                foreach (ActorID root in _roots)
                    Visit(root, 0, true, search, false, false);
            }
            else
            {
                foreach (EditorScene scene in SceneManager.Scenes)
                {
                    bool open = !_collapsedGroups.Contains(scene.Guid);
                    int index = AddRow(new Row { Kind = RowKind.Group, Depth = 0, HasChildren = true, Scene = scene, Group = scene.Guid });
                    VisitFolder(scene, string.Empty, 1, open, scene.Hidden, scene.Locked);
                    CloseRow(index);
                }
                List<ActorID> runtime = _roots.Where(r => !SceneManager.IsMember(r)).ToList();
                if (runtime.Count > 0)
                {
                    bool open = !_collapsedGroups.Contains(RuntimeGroup);
                    int index = AddRow(new Row { Kind = RowKind.Group, Depth = 0, HasChildren = true, Group = RuntimeGroup });
                    foreach (ActorID root in runtime)
                        Visit(root, 1, open, search, false, false);
                    CloseRow(index);
                }
            }
            if (search.Length == 0)
                ComputeTreeLines();
            _expandAll = false;
            _selectName = null;
            if (_selected != 0 && !_selectedSeen)
                Select(ActorID.InvalidID);
            if (_selectedFolder is not null && !SceneManager.Scenes.Any(s => s.FindFolder(_selectedFolder) is not null))
                _selectedFolder = null;
        }

        // Last-sibling flags (backward pass) and the levels whose lines continue through each row (forward pass).
        private void ComputeTreeLines()
        {
            Span<bool> laterSibling = stackalloc bool[MaxLineDepth + 2];
            for (int i = _rows.Count - 1; i >= 0; i--)
            {
                Row row = _rows[i];
                int depth = Math.Min(row.Depth, MaxLineDepth);
                for (int d = depth + 1; d < laterSibling.Length; d++)
                    laterSibling[d] = false;
                row.LastSibling = !laterSibling[depth];
                laterSibling[depth] = true;
                _rows[i] = row;
            }
            ulong mask = 0;
            for (int i = 0; i < _rows.Count; i++)
            {
                Row row = _rows[i];
                int depth = Math.Min(row.Depth, MaxLineDepth);
                mask &= (1UL << depth) - 1;
                row.Guides = mask;
                if (row.LastSibling)
                    mask &= ~(1UL << depth);
                else
                    mask |= 1UL << depth;
                _rows[i] = row;
            }
        }

        private int AddRow(Row row)
        {
            row.LastDescendant = _rows.Count;
            _rows.Add(row);
            return _rows.Count - 1;
        }

        private void CloseRow(int index)
        {
            if (index < 0)
                return;
            Row row = _rows[index];
            row.LastDescendant = _rows.Count - 1;
            _rows[index] = row;
        }

        // A folder level of a scene (parentId empty = the scene root): its folders first, then its root actors.
        private void VisitFolder(EditorScene scene, string parentId, int depth, bool visible, bool parentHidden, bool parentLocked)
        {
            foreach (SceneFolder folder in scene.Folders)
            {
                if (!string.Equals(folder.Parent, parentId, StringComparison.OrdinalIgnoreCase))
                    continue;
                bool open = !_collapsedFolders.Contains(folder.Id);
                bool hasChildren = scene.Folders.Any(f => string.Equals(f.Parent, folder.Id, StringComparison.OrdinalIgnoreCase))
                    || scene.FolderOf.Values.Any(v => string.Equals(v, folder.Id, StringComparison.OrdinalIgnoreCase));
                int index = visible
                    ? AddRow(new Row { Kind = RowKind.Folder, Depth = depth, HasChildren = hasChildren, Scene = scene, Folder = folder, ParentHidden = parentHidden, ParentLocked = parentLocked })
                    : -1;
                VisitFolder(scene, folder.Id, depth + 1, visible && open, parentHidden || folder.Hidden, parentLocked || folder.Locked);
                CloseRow(index);
            }
            foreach (ActorID root in _roots)
                if (scene.Contains(root) && string.Equals(scene.FolderIdOf(root) ?? string.Empty, parentId, StringComparison.OrdinalIgnoreCase))
                    Visit(root, depth, visible, string.Empty, parentHidden, parentLocked);
        }

        private void Visit(ActorID id, int depth, bool visible, string search, bool parentHidden, bool parentLocked)
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
                    AddRow(new Row { Kind = RowKind.Actor, Id = id, ParentHidden = ActorEditorState.IsHiddenByParent(id), ParentLocked = ActorEditorState.IsLockedByParent(id) });
            }

            int index = search.Length == 0 && visible
                ? AddRow(new Row { Kind = RowKind.Actor, Id = id, Depth = depth, HasChildren = hasChildren, ParentHidden = parentHidden, ParentLocked = parentLocked })
                : -1;

            bool childrenVisible = visible && _expanded.Contains(id);
            bool hidden = parentHidden || ActorEditorState.IsHidden(id);
            bool locked = parentLocked || ActorEditorState.IsLocked(id);
            ActorID[] list = children.ToArray();
            foreach (ActorID child in list)
                Visit(child, depth + 1, childrenVisible, search, hidden, locked);
            CloseRow(index);
        }

        private List<ActorID> ChildList(int depth)
        {
            while (_childLists.Count <= depth)
                _childLists.Add(new List<ActorID>());
            return _childLists[depth];
        }

        // ------------------------------------------------------------
        // Drawing
        // ------------------------------------------------------------
        private void DrawHeader()
        {
            using var canvas = Gui.Canvas("##hierarchy_header", new Vector2(0.0f, HeaderHeight));
            Painter draw = canvas.Draw;
            float width = canvas.Size.x;
            draw.RectFilled(Rect.FromXYWH(0.0f, 0.0f, width, HeaderHeight), Style.Color(StyleColor.PanelHeaderBg));
            draw.Line(new Vector2(0.0f, HeaderHeight - 0.5f), new Vector2(width, HeaderHeight - 0.5f), Style.Color(StyleColor.Border));
            Color32 dim = Style.Color(StyleColor.TextDim);
            draw.Text(new Vector2(8.0f, MathF.Round((HeaderHeight - Gui.MeasureText("A").y) * 0.5f)), dim, "Name");
            float x = (_listWidth > 0.0f ? _listWidth : width) - ColumnsWidth;
            for (int i = 0; i < _columns.Length; i++)
            {
                Column column = _columns[i];
                Rect cell = Rect.FromXYWH(x, 0.0f, column.Width, HeaderHeight);
                draw.Line(new Vector2(x + 0.5f, 2.0f), new Vector2(x + 0.5f, HeaderHeight - 2.0f), HierarchyStyle.CellDivider);
                draw.Icon(column.Icon, cell.Center, 12.0f, dim);
                using (Gui.Id(i))
                    Gui.HitArea("##column", cell);
                Gui.Tooltip(column.Tooltip);
                x += column.Width;
            }
        }

        private void DrawTree()
        {
            _list.Count = _rows.Count;
            _list.RowHeight = RowHeight;
            _list.Flags = ListFlags.None;

            Vector2 avail = Gui.AvailableSize;
            bool overflow = _maxExtent > _nameWidth + 1.0f && _nameWidth > 0.0f;
            Vector2 size = overflow ? new Vector2(0.0f, MathF.Max(avail.y - ScrollBarHeight, RowHeight)) : default;
            using ListScope list = Gui.List("##hierarchy_list", ref _list, size);
            if (!list.IsOpen)
                return;

            _listWidth = Gui.AvailableSize.x;
            _nameWidth = MathF.Max(_listWidth - ColumnsWidth, 40.0f);
            float maxScroll = MathF.Max(0.0f, _maxExtent - _nameWidth);
            _scrollX = Math.Clamp(_scrollX, 0.0f, maxScroll);

            int selectedFolderRow = -1;
            if (_selectedFolder is not null)
                selectedFolderRow = _rows.FindIndex(r => r.Kind == RowKind.Folder && string.Equals(r.Folder!.Id, _selectedFolder, StringComparison.OrdinalIgnoreCase));
            int hoveredFolderRow = _hoveredRow >= 0 && _hoveredRow < _rows.Count && _rows[_hoveredRow].Kind == RowKind.Folder ? _hoveredRow : -1;
            int hovered = -1;
            float maxExtent = 0.0f;
            float textY = (RowHeight - Gui.MeasureText("A").y) * 0.5f;
            int[] chain = _chain;
            FillSelectedChain(chain, selectedFolderRow);

            for (int i = _list.First; i < _list.Last && i < _rows.Count; ++i)
            {
                Row row = _rows[i];
                bool selected = row.Kind switch
                {
                    RowKind.Actor => Selection.Contains(new ActorID(row.Id)),
                    RowKind.Folder => i == selectedFolderRow,
                    _ => false,
                };
                bool expanded = row.Kind switch
                {
                    RowKind.Actor => _expanded.Contains(row.Id),
                    RowKind.Folder => !_collapsedFolders.Contains(row.Folder!.Id),
                    _ => !_collapsedGroups.Contains(row.Group!),
                };
                TreeRowFlags flags = (row.HasChildren ? TreeRowFlags.HasChildren : TreeRowFlags.None)
                    | (expanded ? TreeRowFlags.Expanded : TreeRowFlags.None)
                    | (selected ? TreeRowFlags.Selected : TreeRowFlags.None);

                using TreeRowScope scope = Gui.TreeRow(i, row.Depth, flags);
                bool rowHovered = scope.Interaction.Hovered();
                if (rowHovered)
                    hovered = i;
                HandleRowInput(scope, row);

                Painter draw = scope.Draw;
                float left = -(TreeArrowOffset + row.Depth * TreeIndent);
                Rect full = Rect.FromXYWH(left, 0.0f, _listWidth, RowHeight);
                if (row.Kind == RowKind.Folder && !selected)
                    draw.RectFilled(full, HierarchyStyle.FolderRow);
                if (selectedFolderRow >= 0 && i > selectedFolderRow && i <= _rows[selectedFolderRow].LastDescendant && !selected)
                {
                    draw.RectFilled(full, HierarchyStyle.DescendantSelected);
                    draw.RectFilled(Rect.FromXYWH(left, 0.0f, EditorStyle.SelectionBarWidth, RowHeight), HierarchyStyle.DescendantBar);
                }
                else if (hoveredFolderRow >= 0 && i > hoveredFolderRow && i <= _rows[hoveredFolderRow].LastDescendant && !selected)
                {
                    draw.RectFilled(full, HierarchyStyle.DescendantHover);
                }
                if (selected)
                    EditorStyle.SelectionBar(draw, left, RowHeight);
                if (_search.Text.Length == 0)
                    DrawTreeLines(draw, row, i, expanded, chain);

                draw.PushClip(Rect.FromXYWH(0.0f, 0.0f, MathF.Max(_nameWidth + left, 0.0f), RowHeight));
                float extent = row.Kind == RowKind.Group ? DrawGroup(draw, row, textY, rowHovered) : DrawName(draw, row, i, textY, selected, rowHovered);
                draw.PopClip();
                maxExtent = MathF.Max(maxExtent, -left + extent);

                float x = left + _listWidth - ColumnsWidth;
                for (int c = 0; c < _columns.Length; c++)
                {
                    Rect cell = Rect.FromXYWH(x, 0.0f, _columns[c].Width, RowHeight);
                    draw.Line(new Vector2(x + 0.5f, 0.0f), new Vector2(x + 0.5f, RowHeight), HierarchyStyle.CellDivider);
                    using (Gui.Id(i))
                    using (Gui.Id(c))
                        DrawCell(draw, cell, _columns[c], row, rowHovered);
                    x += _columns[c].Width;
                }
            }
            _hoveredRow = hovered;
            _maxExtent = maxExtent;
        }

        // The rows on the path from the selected row up to its depth-0 header, by depth (-1 = none).
        private void FillSelectedChain(int[] chain, int selectedFolderRow)
        {
            Array.Fill(chain, -1);
            int selectedRow = selectedFolderRow;
            if (selectedRow < 0 && _selected != 0)
                selectedRow = _rows.FindIndex(r => r.Kind == RowKind.Actor && r.Id == _selected);
            if (selectedRow < 0 || _search.Text.Length > 0)
                return;
            int depth = Math.Min(_rows[selectedRow].Depth, MaxLineDepth);
            chain[depth] = selectedRow;
            for (int i = selectedRow - 1; i >= 0 && depth > 0; i--)
                if (_rows[i].Depth < depth)
                {
                    depth = Math.Min(_rows[i].Depth, MaxLineDepth);
                    chain[depth] = i;
                }
        }

        // Elbow tree lines in row space (origin after the row's arrow; arrows don't scroll sideways, names do): the
        // row's own level line and elbow, the levels passing through, and a stub under an expanded row's arrow. The
        // selected row's path is drawn brighter.
        private void DrawTreeLines(Painter draw, Row row, int index, bool expanded, int[] chain)
        {
            int depth = Math.Min(row.Depth, MaxLineDepth);
            float cy = MathF.Floor(RowHeight * 0.5f) + 0.5f;
            bool OnPath(int level, int i) => level >= 1 && chain[level] >= 0 && chain[level - 1] >= 0 && i > chain[level - 1] && i <= chain[level];
            float LevelX(int level) => -(depth - level + 1) * TreeIndent - 8.0f + 0.5f;

            if (row.HasChildren && expanded && depth + 1 >= FirstLineDepth && index + 1 < _rows.Count && _rows[index + 1].Depth == row.Depth + 1)
            {
                bool path = chain[depth] == index && depth + 1 < chain.Length && chain[depth + 1] >= 0;
                draw.Line(new Vector2(-7.5f, cy + 4.0f), new Vector2(-7.5f, RowHeight), path ? HierarchyStyle.TreeLineBranch : HierarchyStyle.TreeLine);
            }
            if (depth < FirstLineDepth)
                return;
            for (int level = FirstLineDepth; level < depth; level++)
                if ((row.Guides & (1UL << level)) != 0)
                    draw.Line(new Vector2(LevelX(level), 0.0f), new Vector2(LevelX(level), RowHeight), OnPath(level, index) ? HierarchyStyle.TreeLineBranch : HierarchyStyle.TreeLine);
            Color32 own = OnPath(depth, index) ? HierarchyStyle.TreeLineBranch : HierarchyStyle.TreeLine;
            float x = LevelX(depth);
            draw.Line(new Vector2(x, 0.0f), new Vector2(x, row.LastSibling ? cy + 0.5f : RowHeight), own);
            float end = row.HasChildren ? (expanded ? -13.0f : -12.0f) : MathF.Max(-_scrollX - 0.5f, x + 1.0f);
            draw.Line(new Vector2(x, cy), new Vector2(end, cy), own);
        }

        private void HandleRowInput(TreeRowScope scope, Row row)
        {
            bool rightClicked = (scope.Interaction & Result.RightClicked) != 0;
            switch (row.Kind)
            {
                case RowKind.Actor:
                    if (scope.Toggled || (scope.DoubleClicked && row.HasChildren))
                    {
                        if (!_expanded.Remove(row.Id))
                            _expanded.Add(row.Id);
                    }
                    else if (scope.Clicked)
                    {
                        _selectedFolder = null;
                        Select(new ActorID(row.Id));
                    }
                    if (rightClicked)
                        _contextMenu.Open(new ContextMenuContext(SceneContextMenu.ItemSource, new object[] { new ActorID(row.Id) }, this));
                    break;
                case RowKind.Folder:
                    SceneFolder folder = row.Folder!;
                    if (scope.Toggled || scope.DoubleClicked)
                    {
                        if (!_collapsedFolders.Remove(folder.Id))
                            _collapsedFolders.Add(folder.Id);
                    }
                    else if (scope.Clicked)
                    {
                        Selection.Clear();
                        _selected = 0;
                        _selectedFolder = folder.Id;
                    }
                    if (rightClicked)
                        _contextMenu.Open(new ContextMenuContext(VisibilityContextMenu.FolderSource, new object[] { folder, row.Scene! }, this));
                    break;
                default:
                    if (scope.Toggled || scope.DoubleClicked)
                    {
                        if (!_collapsedGroups.Remove(row.Group!))
                            _collapsedGroups.Add(row.Group!);
                    }
                    if (rightClicked)
                        _contextMenu.Open(new ContextMenuContext(SceneContextMenu.BackgroundSource, row.Scene is { } scene ? new object[] { scene } : Array.Empty<object>(), this));
                    break;
            }
        }

        // An actor or folder: icon and name (dim when effectively hidden, bold when effectively locked). Clicking the icon
        // opens the icon picker. Returns the drawn width from the row origin.
        private float DrawName(Painter draw, Row row, int index, float textY, bool selected, bool rowHovered)
        {
            float x = -_scrollX;
            bool hidden;
            bool locked;
            string name;
            ComponentIcon icon;
            bool editable;
            bool composed = false;
            if (row.Kind == RowKind.Folder)
            {
                SceneFolder folder = row.Folder!;
                hidden = row.ParentHidden || folder.Hidden;
                locked = row.ParentLocked || folder.Locked;
                name = folder.Name;
                icon = EditorIcons.OfFolder(folder, row.HasChildren && !_collapsedFolders.Contains(folder.Id));
                editable = !locked;
            }
            else
            {
                ActorID actor = new(row.Id);
                hidden = row.ParentHidden || ActorEditorState.IsHidden(actor);
                locked = row.ParentLocked || ActorEditorState.IsLocked(actor);
                name = ActorTree.GetName(actor);
                composed = ArchetypeDriver.IsCompositionActor(actor);
                icon = composed ? new ComponentIcon(Icon.Actor, null, HierarchyStyle.Composed) : EditorIcons.OfActor(actor);
                editable = !composed && !locked;
            }
            if (hidden)
                icon = EditorIcons.WithAlpha(icon, 115);
            icon.Draw(draw, new Vector2(x + 7.0f, RowHeight * 0.5f), 13.0f);
            Color32 text = hidden ? Style.Color(StyleColor.TextDim) : Style.Color(selected ? StyleColor.TextSelected : StyleColor.Text);
            float width;
            using (Gui.Font(locked ? FontStyle.Bold : FontStyle.Default))
            {
                draw.Text(new Vector2(x + 18.0f, textY), text, name);
                width = Gui.MeasureText(name).x;
            }
            float extent = 18.0f + width + 8.0f;
            if (row.Kind == RowKind.Folder)
            {
                int count = CountChildren(row.Scene!, row.Folder!);
                if (count > 0)
                {
                    string label = count.ToString();
                    draw.Text(new Vector2(x + extent - 4.0f, textY), Style.Color(StyleColor.TextDim), label);
                    extent += Gui.MeasureText(label).x + 4.0f;
                }
            }
            if (rowHovered)
            {
                if (composed)
                    Gui.Tooltip("A Composition Actor: added and removed by its object (.xco); its transform follows the composition");
                else if (row.Kind == RowKind.Folder)
                    Gui.Tooltip("A group: holds root actors in the editor only (no actor, nothing at runtime)");
            }
            if (editable)
            {
                Rect cell = Rect.FromXYWH(x - 2.0f, 1.0f, 18.0f, RowHeight - 2.0f);
                Result hit;
                using (Gui.Id(index))
                    hit = Gui.HitArea("##icon", cell);
                if (hit.Hovered())
                {
                    draw.Rect(cell, Style.Color(StyleColor.Accent), 2.0f);
                    Gui.Tooltip("Click to change the icon");
                }
                if (hit.Clicked())
                {
                    if (row.Kind == RowKind.Folder)
                        OpenIconPicker(row.Scene!, row.Folder!);
                    else
                        OpenIconPicker(new ActorID(row.Id));
                }
            }
            return extent;
        }

        private static int CountChildren(EditorScene scene, SceneFolder folder) =>
            scene.Folders.Count(f => string.Equals(f.Parent, folder.Id, StringComparison.OrdinalIgnoreCase))
            + scene.FolderOf.Values.Count(v => string.Equals(v, folder.Id, StringComparison.OrdinalIgnoreCase));

        // A scene (or runtime) header: icon, bold name and the unsaved dot.
        private float DrawGroup(Painter draw, Row row, float textY, bool rowHovered)
        {
            EditorScene? scene = row.Scene;
            float x = -_scrollX;
            draw.Icon(scene is null ? Icon.Game : Icon.Scene, new Vector2(x + 7.0f, RowHeight * 0.5f), 13.0f,
                scene is null ? Style.Color(StyleColor.TextDim) : Style.Color(StyleColor.IconWarning));
            string label = scene?.Name ?? "Not saved (runtime)";
            float width;
            using (Gui.Font(FontStyle.Bold))
            {
                draw.Text(new Vector2(x + 18.0f, textY), Style.Color(scene is null || scene.Hidden ? StyleColor.TextDim : StyleColor.Text), label);
                width = Gui.MeasureText(label).x;
            }
            if (scene is { IsDirty: true })
                draw.CircleFilled(new Vector2(x + 18.0f + width + 10.0f, RowHeight * 0.5f), 3.5f, Style.Color(StyleColor.IconWarning));
            if (rowHovered)
                Gui.Tooltip(scene is null ? "Actors no scene owns (created by scripts or tests); they aren't saved"
                    : (scene.Path ?? "Not saved to a file yet") + (scene == SceneManager.Active ? " (active: new actors go here)" : string.Empty));
            return 18.0f + width + 20.0f;
        }

        // One state cell (lock or visibility) of a row: the row's own state, subdued when a parent overrides it.
        private void DrawCell(Painter draw, Rect cell, Column column, Row row, bool rowHovered)
        {
            bool own;
            bool overridden;
            switch (row.Kind)
            {
                case RowKind.Actor:
                    ActorID actor = new(row.Id);
                    if (column.IsLock && ArchetypeDriver.IsCompositionActor(actor))
                    {
                        draw.Icon(Icon.Link, cell.Center, 12.0f, HierarchyStyle.Composed);
                        if (Gui.HitArea("##cell", cell).Hovered())
                            Gui.Tooltip("Joined to its archetype by a Jointer; it can't be locked");
                        return;
                    }
                    own = column.IsLock ? ActorEditorState.IsLocked(actor) : ActorEditorState.IsHidden(actor);
                    overridden = column.IsLock ? row.ParentLocked : row.ParentHidden;
                    break;
                case RowKind.Folder:
                    own = column.IsLock ? row.Folder!.Locked : row.Folder!.Hidden;
                    overridden = column.IsLock ? row.ParentLocked : row.ParentHidden;
                    break;
                default:
                    if (row.Scene is not { } scene)
                        return;
                    own = column.IsLock ? scene.Locked : scene.Hidden;
                    overridden = false;
                    break;
            }

            Result hit = Gui.HitArea("##cell", cell);
            bool cellHovered = hit.Hovered();
            Icon on = column.IsLock ? Icon.Lock : Icon.EyeOff;
            Icon off = column.IsLock ? Icon.Unlock : Icon.Eye;
            Rect inset = Rect.FromXYWH(cell.min.x + 1.0f, cell.min.y + 1.0f, cell.Width - 2.0f, cell.Height - 2.0f);
            if (own && !overridden)
            {
                draw.RectFilled(inset, column.IsLock ? HierarchyStyle.LockCell : HierarchyStyle.HiddenCell, 2.0f);
                draw.RectFilled(Rect.FromXYWH(inset.min.x, inset.max.y - 2.0f, inset.Width, 2.0f), column.IsLock ? HierarchyStyle.LockLine : HierarchyStyle.HiddenLine);
                draw.Icon(on, cell.Center, 12.0f, column.IsLock ? HierarchyStyle.Lock : HierarchyStyle.Hidden);
            }
            else if (own)
            {
                draw.RectFilled(inset, column.IsLock ? HierarchyStyle.LockMutedCell : HierarchyStyle.HiddenMutedCell, 2.0f);
                draw.Icon(on, cell.Center, 12.0f, column.IsLock ? HierarchyStyle.LockMuted : HierarchyStyle.HiddenMuted);
            }
            else if (overridden)
            {
                draw.Icon(off, cell.Center, 12.0f, cellHovered ? HierarchyStyle.Unset : HierarchyStyle.Overridden);
            }
            else if (rowHovered || cellHovered)
            {
                draw.Icon(off, cell.Center, 12.0f, cellHovered ? HierarchyStyle.UnsetHover : HierarchyStyle.Unset);
            }
            if (cellHovered)
                Gui.Tooltip(CellTooltip(column, row, own, overridden));
            if (hit.Clicked())
                Toggle(column, row, !own);
        }

        private static string CellTooltip(Column column, Row row, bool own, bool overridden)
        {
            string what = row.Kind switch { RowKind.Folder => "group", RowKind.Group => "scene", _ => "actor" };
            string state = column.IsLock ? (own ? $"Locked {what}: click to unlock" : $"Click to lock the {what}")
                : (own ? $"Hidden {what}: click to show it in the Scene view" : $"Click to hide the {what} in the Scene view");
            return overridden ? state + (column.IsLock ? "\nA parent is locked, which locks it anyway" : "\nA parent is hidden, which hides it anyway") : state;
        }

        private static void Toggle(Column column, Row row, bool on)
        {
            switch (row.Kind)
            {
                case RowKind.Actor:
                    ActorID actor = new(row.Id);
                    IReadOnlyList<ActorID> targets = Selection.Contains(actor) ? Selection.Actors : new[] { actor };
                    if (column.IsLock)
                        ActorEditorState.SetLocked(targets, on);
                    else
                        ActorEditorState.SetHidden(targets, on);
                    break;
                case RowKind.Folder:
                    if (column.IsLock)
                        SceneManager.SetFolderLocked(row.Scene!, row.Folder!, on);
                    else
                        SceneManager.SetFolderHidden(row.Scene!, row.Folder!, on);
                    break;
                default:
                    if (row.Scene is not { } scene)
                        return;
                    if (column.IsLock)
                        SceneManager.SetSceneLocked(scene, on);
                    else
                        SceneManager.SetSceneHidden(scene, on);
                    break;
            }
        }

        // The name column's horizontal scroll bar, shown while the widest row overflows it.
        private void DrawScrollBar()
        {
            float range = _maxExtent - _nameWidth;
            if (range <= 1.0f || _nameWidth <= 0.0f)
                return;
            using var canvas = Gui.Canvas("##hierarchy_hscroll", new Vector2(0.0f, ScrollBarHeight));
            Painter draw = canvas.Draw;
            Rect track = Rect.FromXYWH(0.0f, 0.0f, _nameWidth, ScrollBarHeight);
            draw.RectFilled(Rect.FromXYWH(0.0f, 0.0f, canvas.Size.x, ScrollBarHeight), Style.Color(StyleColor.TopBarBg));
            float thumbWidth = MathF.Max(24.0f, _nameWidth * _nameWidth / _maxExtent);
            float travel = MathF.Max(_nameWidth - thumbWidth, 1.0f);
            Result hit = Gui.HitArea("##track", track);
            if ((hit & Result.Active) != 0)
                _scrollX = Math.Clamp((Gui.MousePosition.x - thumbWidth * 0.5f) / travel, 0.0f, 1.0f) * range;
            float thumbX = _scrollX / range * travel;
            Color32 thumb = Style.Color((hit & Result.Active) != 0 ? StyleColor.Accent : hit.Hovered() ? StyleColor.ButtonHover : StyleColor.Button);
            draw.RectFilled(Rect.FromXYWH(thumbX, 2.0f, thumbWidth, ScrollBarHeight - 4.0f), thumb, 3.0f);
        }
    }
}
