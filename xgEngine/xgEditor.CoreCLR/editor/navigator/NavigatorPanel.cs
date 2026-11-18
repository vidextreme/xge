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
    // Editor Navigator: the project's content browser. One panel with a browse section (Directory tree, Tags filter or
    // Search) and the asset grid, side by side when the panel is wide and stacked otherwise. Back/forward walk the
    // runtime history of locations; bookmarks (saved searches and items) are shared through ProjectSettings.
    [PanelTool("Navigator", Icon = "Folder", Unique = true, Priority = 30, Tooltip = "Project content browser")]
    public sealed class NavigatorPanel : Panel, IPanelTool
    {
        private enum OptionTab { None, View, Filter, Sort }
        private enum SortMode { None, Type, Status, User }
        private enum ViewSize { Details, Small, Medium, Large }
        private enum DetailColumn { Name, Type, Size, Modified }

        private readonly struct GridItem
        {
            public GridItem(AssetEntry? entry, bool up)
            {
                Entry = entry;
                Up = up;
            }

            public AssetEntry? Entry { get; }
            public bool Up { get; }
        }

        private readonly struct Section
        {
            public Section(string title, Icon icon, Color32 color)
            {
                Title = title;
                Icon = icon;
                Color = color;
                Items = new List<GridItem>();
            }

            public string Title { get; }
            public Icon Icon { get; }
            public Color32 Color { get; }
            public List<GridItem> Items { get; }
        }

        private readonly struct TreeRow
        {
            public TreeRow(string path, string name, int depth, bool hasChildren)
            {
                Path = path;
                Name = name;
                Depth = depth;
                HasChildren = hasChildren;
            }

            public string Path { get; }
            public string Name { get; }
            public int Depth { get; }
            public bool HasChildren { get; }
        }

        private const float RowHeight = 20.0f;
        private const float TreeArrowOffset = 20.0f;
        private const float TreeIndent = 14.0f;
        private const float SegButton = 24.0f;
        private const float SegHeight = 22.0f;
        private const float OptionRowHeight = 26.0f;
        private const float TileGap = 6.0f;
        private static readonly Color32 _sectionBg = new(58, 58, 58, 255);
        private const float WideRatio = 1.6f;
        private const float WideMinWidth = 560.0f;
        private const int MaxThumbnailSide = 2048;
        private const string BookmarksPopupId = "nav_bookmarks";
        private const string AllTagsPopupId = "nav_all_tags";
        private const string ContextPopupId = "nav_context";
        private const string KindMorePopupId = "nav_kind_more";
        private const string EditTagsModalId = "nav_edit_tags";
        private const string RenameModalId = "nav_rename";
        private const string TagModalId = "nav_tag";

        private static readonly AssetKind[] _filterKinds =
        {
            AssetKind.Script, AssetKind.Image, AssetKind.Material, AssetKind.Mesh, AssetKind.Audio, AssetKind.Scene, AssetKind.Text, AssetKind.Other,
        };

        private readonly NavigatorHistory _history = new();
        private readonly TagInput _tagInput = new("##nav_tags");
        private readonly AutoCompleteField _pathField = new("##nav_path");
        private readonly AutoCompleteField _editTagField = new("##nav_edit_tag", 128);
        private readonly TextBuffer _searchText = new(256);
        private readonly TextBuffer _gridSearch = new(256);
        private readonly HashSet<string> _expanded = new(StringComparer.OrdinalIgnoreCase) { AssetIndex.RootFolder };
        private readonly List<TreeRow> _treeRows = new();
        private readonly List<Section> _sections = new();
        private readonly List<AssetEntry> _base = new();
        private readonly int[] _kindCounts = new int[Enum.GetValues<AssetKind>().Length];
        private readonly Dictionary<string, (Texture? Texture, DateTime Modified)> _thumbnails = new(StringComparer.OrdinalIgnoreCase);
        private readonly Dictionary<(string, int), string> _ellipsized = new();

        private readonly TextBuffer _renameText = new(256);
        private readonly TextBuffer _tagRenameText = new(128);
        private readonly TextBuffer _tagDescriptionText = new(256);

        private AssetDatabase? _db;
        private int _seenIndexVersion = -1;
        private AssetIndex? _index;
        private AssetTags? _tags;
        private Bookmarks? _bookmarks;
        private ListState _treeList;
        private NavigatorMode _mode = NavigatorMode.Directory;
        private string _folder = AssetIndex.RootFolder;
        private string? _pathError;
        private string? _selected;
        private int _kinds;
        private OptionTab _option = OptionTab.None;
        private SortMode _sort = SortMode.None;
        private ViewSize _view = ViewSize.Medium;
        private DetailColumn _sortColumn = DetailColumn.Name;
        private bool _sortDescending;
        private bool _filterBookmarked;
        private bool _filterRecent;
        private bool _filterUntagged;
        private bool _searchDirty;
        private bool _sideBySide = true;
        private bool _candidateSide = true;
        private int _candidateFrames;
        private float _browseWidth = 260.0f;
        private float _gridHeight = 220.0f;
        private int _thumbnailLoads;

        private bool _openBookmarks;
        private int _openBookmarksIn;
        private bool _openAllTags;
        private bool _openKindMore;
        private Rect _kindMoreRect;
        private bool _openContext;
        private int _contextSection = -1;
        private Rect _contextRect;
        private string? _contextPath;
        private bool _openEditTags;
        private string? _editTagsPath;
        private string? _typeTags;
        private string? _pendingFolder;
        private int _typeTagsIn;
        private bool _openRename;
        private string? _renameHookPath;
        private int _renameHookIn;
        private bool _renameSetText;
        private string? _renamePath;
        private string? _renameError;
        private bool _openTag;
        private bool _tagConfirmDelete;
        private string? _tagEdit;

        public override string Title => "Navigator";

        public int InstanceIndex { get; set; }

        public ToolUsability OnSelectItems(object[] selection) => ToolUsability.Usable;
        public override string? MenuPath => "General/Navigator";
        public override DockSlot Slot => Environment.GetEnvironmentVariable("XG_UI_TEST_NAV_DOCK")?.ToLowerInvariant() switch
        {
            "center" => DockSlot.Center,
            "right" => DockSlot.Right,
            "left" => DockSlot.Left,
            _ => DockSlot.Bottom,
        };
        public override Icon PanelIcon => Icon.Project;
        public override PanelFlags Flags => PanelFlags.NoPadding | PanelFlags.NoScrollbar;

        public NavigatorPanel()
        {
            _pathField.Source = SuggestFolders;
            _editTagField.Source = SuggestUserTags;
        }

        // ------------------------------------------------------------
        // Test hooks (XG_UI_TEST_NAV_*)
        // ------------------------------------------------------------
        public void SetMode(string mode) => ApplyState(CurrentState() with { Mode = NavigatorState.ParseMode(mode) }, true);

        public void SetTags(string query)
        {
            _tagInput.SetQuery(query);
            ApplyState(CurrentState() with { Mode = NavigatorMode.Tags, Tags = _tagInput.Query.ToString() }, true);
        }

        public void TypeTags(string text, int frames)
        {
            _typeTags = text;
            _typeTagsIn = frames;
        }

        public void OpenBookmarks(int frames) => _openBookmarksIn = Math.Max(frames, 1);

        public void SetSort(string sort) => _sort = sort.ToLowerInvariant() == "type" ? SortMode.Type : SortMode.None;

        public void SetOptions(string option) => _option = option.ToLowerInvariant() switch
        {
            "view" => OptionTab.View,
            "filter" => OptionTab.Filter,
            "sort" => OptionTab.Sort,
            _ => OptionTab.None,
        };

        public void SetView(string view)
        {
            if (Enum.TryParse(view, true, out ViewSize parsed))
                _view = parsed;
        }

        // "<column>[:desc]", e.g. "size:desc".
        public void SetDetailSort(string sort)
        {
            string[] parts = sort.Split(':');
            if (Enum.TryParse(parts[0], true, out DetailColumn column))
                _sortColumn = column;
            _sortDescending = parts.Length > 1 && parts[1].Equals("desc", StringComparison.OrdinalIgnoreCase);
        }

        public void SetFolder(string folder)
        {
            if (_index is null)
                _pendingFolder = folder;
            else
                Navigate(folder);
        }

        public void SetKinds(string kinds)
        {
            _kinds = 0;
            foreach (string name in kinds.Split(',', StringSplitOptions.RemoveEmptyEntries | StringSplitOptions.TrimEntries))
                if (TryParseKind(name, out AssetKind kind))
                    _kinds |= 1 << (int)kind;
        }

        // ------------------------------------------------------------
        // Frame
        // ------------------------------------------------------------
        protected override void OnGUI()
        {
            if (!EnsureProject())
            {
                Gui.Space(8.0f);
                using (Gui.Horizontal())
                {
                    Gui.Space(8.0f);
                    Gui.Label("Open a project to browse its assets.");
                }
                return;
            }

            if (_index!.Version != _seenIndexVersion)
            {
                _seenIndexVersion = _index.Version;
                _thumbnails.Clear();
                if (!_index.IsDirectory(_folder))
                    _folder = AssetIndex.RootFolder;
            }
            _thumbnailLoads = 0;
            TickHooks();
            BuildResults();

            Vector2 avail = Gui.AvailableSize;
            ResolveOrientation(avail);
            if (_sideBySide)
            {
                float maxBrowse = Math.Max(180.0f, avail.x - 240.0f);
                float browseWidth = Math.Clamp(_browseWidth, 180.0f, maxBrowse);
                using (Gui.Horizontal(0.0f))
                {
                    DrawBrowse(new Vector2(browseWidth, 0.0f));
                    if (Gui.SplitterVertical("##nav_split_v", ref browseWidth, 180.0f, maxBrowse))
                        _browseWidth = browseWidth;
                    using (Gui.Vertical())
                        DrawGrid();
                }
            }
            else
            {
                float maxGrid = Math.Max(120.0f, avail.y - 120.0f);
                float gridHeight = Math.Clamp(_gridHeight, 120.0f, maxGrid);
                DrawBrowse(new Vector2(0.0f, Math.Max(avail.y - gridHeight - 4.0f, 80.0f)));
                if (Gui.Splitter("##nav_split_h", ref gridHeight, 120.0f, maxGrid))
                    _gridHeight = gridHeight;
                DrawGrid();
            }

            DrawEditTagsModal();
            DrawRenameModal();
            DrawTagModal();
        }

        private bool EnsureProject()
        {
            AssetDatabase? db = ProjectService.Assets;
            if (db is null)
                return false;
            if (_db != db)
            {
                if (_db is not null)
                    _db.Moved -= OnAssetMoved;
                _db = db;
                _db.Moved += OnAssetMoved;
                _index = db.Index;
                _tags = db.Tags;
                _bookmarks = db.Bookmarks;
                _seenIndexVersion = _index.Version;
                _thumbnails.Clear();
                _folder = AssetIndex.RootFolder;
                _history.Push(CurrentState());
                if (_pendingFolder is not null)
                {
                    NavigatorMode mode = _mode;
                    Navigate(_pendingFolder);
                    if (mode != NavigatorMode.Directory)
                        ApplyState(CurrentState() with { Mode = mode }, true);
                    _pendingFolder = null;
                }
            }
            return true;
        }

        private void TickHooks()
        {
            if (_openBookmarksIn > 0 && --_openBookmarksIn == 0)
                _openBookmarks = true;
            if (_renameHookPath is not null && _renameHookIn > 0 && --_renameHookIn == 0)
            {
                OpenRename(_renameHookPath);
                _renameHookPath = null;
            }
            if (_typeTags is not null && _typeTagsIn > 0 && --_typeTagsIn == 0)
            {
                if (_mode != NavigatorMode.Tags)
                    ApplyState(CurrentState() with { Mode = NavigatorMode.Tags }, true);
                _tagInput.Type(_typeTags);
                _typeTags = null;
            }
        }

        private void ResolveOrientation(Vector2 avail)
        {
            bool side = avail.x >= avail.y * WideRatio && avail.x >= WideMinWidth;
            if (side == _sideBySide)
            {
                _candidateFrames = 0;
                return;
            }
            if (side != _candidateSide)
            {
                _candidateSide = side;
                _candidateFrames = 0;
            }
            if (++_candidateFrames >= 2)
            {
                _sideBySide = side;
                _candidateFrames = 0;
            }
        }

        // ------------------------------------------------------------
        // State and history
        // ------------------------------------------------------------
        private NavigatorState CurrentState() => new(_mode, _folder, _tagInput.Query.ToString(), _searchText.Text, _kinds);

        private void ApplyState(NavigatorState state, bool push)
        {
            _mode = state.Mode;
            if (_index is not null && _index.IsDirectory(state.Folder))
                _folder = AssetIndex.Normalize(state.Folder);
            if (_tagInput.Query.ToString() != state.Tags)
                _tagInput.SetQuery(state.Tags);
            if (_searchText.Text != state.Search)
                _searchText.Text = state.Search;
            _kinds = state.Kinds;
            _pathError = null;
            if (push)
                _history.Push(CurrentState());
        }

        private void Navigate(string folder)
        {
            string path = AssetIndex.Normalize(folder);
            if (_index is null || !_index.IsDirectory(path))
            {
                _pathError = $"No folder '{folder}'";
                return;
            }
            string parent = path;
            while (parent.Contains('/'))
            {
                parent = parent[..parent.LastIndexOf('/')];
                _expanded.Add(parent);
            }
            ApplyState(CurrentState() with { Mode = NavigatorMode.Directory, Folder = path }, true);
            _pathField.SetText(path, false);
        }

        private void PushKinds()
        {
            _history.Push(CurrentState());
        }

        // ------------------------------------------------------------
        // Results
        // ------------------------------------------------------------
        private void BuildResults()
        {
            _base.Clear();
            Array.Clear(_kindCounts);
            ParseGridSearch(out string nameQuery, out int tokenKinds);
            AssetIndex index = _index!;
            AssetTags tags = _tags!;

            IEnumerable<AssetEntry> source = _mode switch
            {
                NavigatorMode.Directory => index.Children(_folder),
                NavigatorMode.Tags => TagSource(index, tags),
                _ => SearchSource(index, tags),
            };
            foreach (AssetEntry entry in source)
            {
                if (nameQuery.Length > 0 && !entry.Name.Contains(nameQuery, StringComparison.OrdinalIgnoreCase))
                    continue;
                _base.Add(entry);
                if (!entry.IsFolder)
                    _kindCounts[(int)entry.Kind]++;
            }
            _base.Sort(CompareEntries);

            int kinds = _kinds | tokenKinds;
            _sections.Clear();
            Section folders = new("Folders", Icon.Folder, AssetIndex.IconColorOf(AssetKind.Folder));
            if (_mode == NavigatorMode.Directory && _folder.Contains('/') && nameQuery.Length == 0)
                folders.Items.Add(new GridItem(null, true));
            Dictionary<AssetKind, Section> byKind = new();
            Section files = new("Files", Icon.File, Style.Color(StyleColor.Icon));
            foreach (AssetEntry entry in _base)
            {
                if (entry.IsFolder)
                {
                    if (kinds == 0)
                        folders.Items.Add(new GridItem(entry, false));
                    continue;
                }
                if (kinds != 0 && (kinds & (1 << (int)entry.Kind)) == 0)
                    continue;
                if (_sort == SortMode.Type)
                {
                    if (!byKind.TryGetValue(entry.Kind, out Section section))
                        byKind[entry.Kind] = section = new Section(AssetIndex.PluralName(entry.Kind), AssetIndex.IconOf(entry.Kind), AssetIndex.IconColorOf(entry.Kind));
                    section.Items.Add(new GridItem(entry, false));
                }
                else
                {
                    files.Items.Add(new GridItem(entry, false));
                }
            }
            if (folders.Items.Count > 0)
                _sections.Add(folders);
            if (_sort == SortMode.Type)
            {
                foreach (AssetKind kind in Enum.GetValues<AssetKind>())
                    if (byKind.TryGetValue(kind, out Section section))
                        _sections.Add(section);
            }
            else if (files.Items.Count > 0)
            {
                _sections.Add(files);
            }
        }

        private int CompareEntries(AssetEntry a, AssetEntry b)
        {
            if (a.IsFolder != b.IsFolder)
                return a.IsFolder ? -1 : 1;
            int byName = string.Compare(a.Name, b.Name, StringComparison.OrdinalIgnoreCase);
            int result = _sortColumn switch
            {
                DetailColumn.Type => string.Compare(TypeLabel(a), TypeLabel(b), StringComparison.OrdinalIgnoreCase),
                DetailColumn.Size => a.Size.CompareTo(b.Size),
                DetailColumn.Modified => a.Modified.CompareTo(b.Modified),
                _ => byName,
            };
            if (result == 0)
                result = byName;
            return _sortDescending ? -result : result;
        }

        private static string TypeLabel(AssetEntry entry) => entry.IsFolder ? "Folder" : entry.Kind.ToString();

        private static string SizeLabel(AssetEntry entry)
        {
            if (entry.IsFolder)
                return "-";
            long size = entry.Size;
            if (size < 1024)
                return $"{size} B";
            if (size < 1024 * 1024)
                return $"{size / 1024.0:0.#} KB";
            return $"{size / (1024.0 * 1024.0):0.#} MB";
        }

        private IEnumerable<AssetEntry> TagSource(AssetIndex index, AssetTags tags)
        {
            TagQuery query = _tagInput.Query;
            if (query.IsEmpty)
                return Enumerable.Empty<AssetEntry>();
            HashSet<string>? paths = query.MatchingPaths(tags);
            if (paths is null)
                return index.Entries.Where(e => !e.IsFolder && query.Matches(e, tags));
            return paths.Select(index.Find).OfType<AssetEntry>();
        }

        private IEnumerable<AssetEntry> SearchSource(AssetIndex index, AssetTags tags)
        {
            string text = _searchText.Text.Trim();
            bool anyFilter = _filterBookmarked || _filterRecent || _filterUntagged;
            if (text.Length == 0 && !anyFilter)
                yield break;
            DateTime recent = DateTime.Now.AddDays(-7);
            foreach (AssetEntry entry in index.Entries)
            {
                if (text.Length > 0 && !entry.RelPath.Contains(text, StringComparison.OrdinalIgnoreCase))
                    continue;
                if (_filterBookmarked && !_bookmarks!.IsBookmarked(entry.RelPath))
                    continue;
                if (_filterRecent && (entry.IsFolder || entry.Modified < recent))
                    continue;
                if (_filterUntagged && (entry.IsFolder || !tags.IsUntagged(entry.RelPath)))
                    continue;
                yield return entry;
            }
        }

        private void ParseGridSearch(out string nameQuery, out int tokenKinds)
        {
            tokenKinds = 0;
            List<string> words = new();
            foreach (string word in _gridSearch.Text.Split(' ', StringSplitOptions.RemoveEmptyEntries))
            {
                if (word.StartsWith("t:", StringComparison.OrdinalIgnoreCase) && TryParseKind(word[2..], out AssetKind kind))
                    tokenKinds |= 1 << (int)kind;
                else
                    words.Add(word);
            }
            nameQuery = string.Join(' ', words);
        }

        private static bool TryParseKind(string name, out AssetKind kind)
        {
            switch (name.ToLowerInvariant())
            {
                case "images": case "texture": case "textures": case "img": kind = AssetKind.Image; return true;
                case "scripts": case "cs": kind = AssetKind.Script; return true;
                case "meshes": case "model": case "models": kind = AssetKind.Mesh; return true;
                case "mat": case "materials": kind = AssetKind.Material; return true;
                case "sound": case "sounds": kind = AssetKind.Audio; return true;
                case "scenes": kind = AssetKind.Scene; return true;
                case "txt": kind = AssetKind.Text; return true;
            }
            return AssetTags.TryParseType(name, out kind);
        }

        private int MatchCount()
        {
            int count = 0;
            foreach (Section section in _sections)
                foreach (GridItem item in section.Items)
                    if (item.Entry is { IsFolder: false })
                        count++;
            return count;
        }

        // ------------------------------------------------------------
        // Browse section
        // ------------------------------------------------------------
        private void DrawBrowse(Vector2 size)
        {
            using var area = Gui.ScrollArea("##nav_browse", size);
            if (!area.IsOpen)
                return;

            Rect bookmarkButton = DrawBrowseToolbar();
            switch (_mode)
            {
                case NavigatorMode.Directory:
                    DrawDirectoryMode();
                    break;
                case NavigatorMode.Tags:
                    DrawTagsMode();
                    break;
                default:
                    DrawSearchMode();
                    break;
            }
            DrawBookmarksPopup(bookmarkButton);
        }

        private Rect DrawBrowseToolbar()
        {
            Rect bookmarkButton = default;
            using (Gui.Toolbar())
            {
                float navWidth = SegmentWidth(4);
                using (var canvas = Gui.Canvas("##nav_buttons", new Vector2(navWidth, SegHeight + 4.0f)))
                {
                    Painter draw = canvas.Draw;
                    Rect[] cells = SegmentCells(draw, 4);
                    if (SegGlyph(draw, "##back", cells[0], "Back", false, _history.CanBack, DrawLeftArrow).Clicked() && _history.Back() is NavigatorState back)
                        ApplyState(back, false);
                    if (SegGlyph(draw, "##forward", cells[1], "Forward", false, _history.CanForward, DrawRightArrow).Clicked() && _history.Forward() is NavigatorState forward)
                        ApplyState(forward, false);
                    if (SegGlyph(draw, "##refresh", cells[2], "Refresh", false, true, DrawRefresh).Clicked())
                    {
                        _db!.Refresh();
                        _thumbnails.Clear();
                    }
                    if (SegIcon(draw, "##bookmarks", cells[3], Icon.Star, "Bookmarks", false, true).Clicked())
                        _openBookmarks = true;
                    bookmarkButton = Offset(cells[3], canvas.Bounds.min);
                }
                Gui.FlexibleSpace();
                using (var canvas = Gui.Canvas("##nav_modes", new Vector2(SegmentWidth(3), SegHeight + 4.0f)))
                {
                    Painter draw = canvas.Draw;
                    Rect[] cells = SegmentCells(draw, 3);
                    NavigatorMode[] modes = { NavigatorMode.Directory, NavigatorMode.Tags, NavigatorMode.Search };
                    Icon[] icons = { Icon.Folder, Icon.Tag, Icon.Search };
                    string[] tips = { "Browse by directory", "Filter by tags", "Search by name" };
                    for (int i = 0; i < 3; i++)
                        if (SegIcon(draw, "##mode" + i, cells[i], icons[i], tips[i], _mode == modes[i], true).Clicked() && _mode != modes[i])
                            ApplyState(CurrentState() with { Mode = modes[i] }, true);
                }
            }
            return bookmarkButton;
        }

        private void DrawDirectoryMode()
        {
            if (!_pathField.Active && _pathField.Text.Text != _folder && !_pathField.IsOpen)
                _pathField.SetText(_folder, false);
            Gui.Space(2.0f);
            using (Gui.Horizontal(2.0f))
            {
                Gui.Space(4.0f);
                AutoCompleteEvent ev = _pathField.OnGUI("Folder path", 0.0f, out string value);
                if (ev == AutoCompleteEvent.Commit)
                    Navigate(value);
                using (var canvas = Gui.Canvas("##nav_path_buttons", new Vector2(SegButton * 2.0f + 2.0f, SegHeight)))
                {
                    Painter draw = canvas.Draw;
                    Rect history = Rect.FromXYWH(0.0f, 0.0f, SegButton, SegHeight);
                    Rect go = Rect.FromXYWH(SegButton + 2.0f, 0.0f, SegButton, SegHeight);
                    if (SegGlyph(draw, "##recent", history, "Recent folders", false, _history.RecentFolders.Count > 0, DrawChevronDown).Clicked())
                        _pathField.ShowList(_history.RecentFolders.Select(f => new Suggestion(f, null, Icon.Folder, AssetIndex.IconColorOf(AssetKind.Folder))));
                    if (SegGlyph(draw, "##go", go, "Go to folder", false, true, DrawRightArrow).Clicked())
                        Navigate(_pathField.Text.Text);
                }
                Gui.Space(4.0f);
            }
            if (_pathError is not null)
            {
                using (Gui.Horizontal())
                {
                    Gui.Space(6.0f);
                    using var canvas = Gui.Canvas("##nav_path_error", new Vector2(0.0f, 18.0f));
                    canvas.Draw.Text(new Vector2(0.0f, 1.0f), new Color32(232, 96, 96, 255), _pathError);
                }
            }
            Gui.Space(2.0f);
            DrawTree();
        }

        private void DrawTree()
        {
            AssetIndex index = _index!;
            _treeRows.Clear();
            AddTreeRows(index, AssetIndex.RootFolder, AssetIndex.RootFolder, 0);

            _treeList.Count = _treeRows.Count;
            _treeList.RowHeight = RowHeight;
            _treeList.Flags = ListFlags.None;
            using ListScope list = Gui.List("##nav_tree", ref _treeList);
            if (!list.IsOpen)
                return;

            Color32 text = Style.Color(StyleColor.Text);
            Color32 textSelected = Style.Color(StyleColor.TextSelected);
            Color32 folderColor = AssetIndex.IconColorOf(AssetKind.Folder);
            float textY = MathF.Round((RowHeight - Gui.MeasureText("A").y) * 0.5f);
            for (int i = _treeList.First; i < _treeList.Last && i < _treeRows.Count; i++)
            {
                TreeRow row = _treeRows[i];
                bool selected = _mode == NavigatorMode.Directory && string.Equals(row.Path, _folder, StringComparison.OrdinalIgnoreCase);
                bool expanded = _expanded.Contains(row.Path);
                TreeRowFlags flags = (row.HasChildren ? TreeRowFlags.HasChildren : TreeRowFlags.None)
                    | (expanded ? TreeRowFlags.Expanded : TreeRowFlags.None)
                    | (selected ? TreeRowFlags.Selected : TreeRowFlags.None);
                using TreeRowScope scope = Gui.TreeRow(i, row.Depth, flags);
                if (scope.Toggled || (scope.DoubleClicked && row.HasChildren))
                {
                    if (!_expanded.Remove(row.Path))
                        _expanded.Add(row.Path);
                }
                else if (scope.Clicked)
                {
                    Navigate(row.Path);
                }
                Painter draw = scope.Draw;
                if (selected)
                    EditorStyle.SelectionBar(draw, -(TreeArrowOffset + row.Depth * TreeIndent), RowHeight);
                draw.Icon(Icon.Folder, new Vector2(7.0f, RowHeight * 0.5f), 13.0f, folderColor);
                draw.Text(new Vector2(18.0f, textY), selected ? textSelected : text, row.Name);
            }
        }

        private void AddTreeRows(AssetIndex index, string path, string name, int depth)
        {
            bool hasChildren = index.HasSubfolders(path);
            _treeRows.Add(new TreeRow(path, name, depth, hasChildren));
            if (!hasChildren || !_expanded.Contains(path))
                return;
            foreach (AssetEntry child in index.Children(path))
                if (child.IsFolder)
                    AddTreeRows(index, child.RelPath, child.Name, depth + 1);
        }

        private void DrawTagsMode()
        {
            Gui.Space(2.0f);
            using (Gui.Horizontal())
            {
                Gui.Space(6.0f);
                using (Gui.Vertical())
                {
                    using (var card = Gui.Card("##nav_tag_card", "Filter by tags", Icon.Tag, null, Style.Color(StyleColor.IconComponent)))
                    {
                        if (card.IsOpen)
                        {
                            string summary = $"{MatchCount()} of {_index!.Entries.Count(e => !e.IsFolder)} files";
                            if (_tagInput.OnGUI(_tags!, _index, summary))
                                _history.Push(CurrentState());
                        }
                    }
                    Gui.Space(6.0f);
                    if (Gui.Button("Show all tags"))
                        _openAllTags = true;
                    DrawAllTagsPopup();
                }
                Gui.Space(6.0f);
            }
        }

        private void DrawAllTagsPopup()
        {
            Rect anchor = Gui.ItemRect;
            if (_openAllTags)
            {
                _openAllTags = false;
                Gui.OpenPopup(AllTagsPopupId);
            }
            using var popup = Gui.Popup(AllTagsPopupId, new Vector2(anchor.min.x, anchor.max.y + 2.0f), new Vector2(240.0f, 0.0f));
            if (!popup.IsOpen)
                return;
            List<TagCount> all = _tags!.UserTags().Concat(_tags.AttributeTags(_index!)).ToList();
            float listHeight = Math.Min(all.Count * RowHeight + 4.0f, 320.0f);
            using var scroll = Gui.ScrollArea("##nav_all_tags_list", new Vector2(240.0f, listHeight));
            if (!scroll.IsOpen)
                return;
            using var canvas = Gui.Canvas("##nav_all_tags_rows", new Vector2(240.0f, all.Count * RowHeight + 4.0f));
            Painter draw = canvas.Draw;
            float textY = MathF.Round((RowHeight - Gui.MeasureText("A").y) * 0.5f);
            for (int i = 0; i < all.Count; i++)
            {
                Rect row = Rect.FromXYWH(2.0f, 2.0f + i * RowHeight, 236.0f, RowHeight);
                Result hit;
                using (Gui.Id(i))
                    hit = Gui.HitArea("##tag_row", row);
                if (hit.Hovered())
                    draw.RectFilled(row, Style.Color(StyleColor.Accent), 3.0f);
                bool user = !all[i].Tag.StartsWith('@');
                if (user)
                {
                    string description = _tags.Description(all[i].Tag);
                    Gui.Tooltip(description.Length > 0 ? description + "\nRight-click to rename, describe or delete" : "Right-click to rename, describe or delete");
                }
                Color32 text = hit.Hovered() ? Style.Color(StyleColor.TextOnAccent) : Style.Color(StyleColor.Text);
                Color32 iconColor = !hit.Hovered() && user && _tags.Color(all[i].Tag) is Color32 tint ? tint : text;
                draw.Icon(user ? Icon.Tag : Icon.File, new Vector2(row.min.x + 10.0f, row.min.y + RowHeight * 0.5f), 12.0f, iconColor);
                draw.Text(new Vector2(row.min.x + 22.0f, row.min.y + textY), text, all[i].Tag);
                string count = all[i].Count.ToString();
                draw.Text(new Vector2(row.max.x - Gui.MeasureText(count).x - 8.0f, row.min.y + textY), hit.Hovered() ? text : Style.Color(StyleColor.TextDim), count);
                if (hit.Clicked())
                {
                    _tagInput.AddChip(all[i].Tag, false);
                    _history.Push(CurrentState());
                    Gui.ClosePopup();
                }
                else if (user && (hit & Result.RightClicked) != 0)
                {
                    OpenTagModal(all[i].Tag);
                    Gui.ClosePopup();
                }
            }
        }

        private void DrawSearchMode()
        {
            Gui.Space(2.0f);
            using (Gui.Horizontal())
            {
                Gui.Space(6.0f);
                using (Gui.Vertical())
                {
                    if (Gui.SearchField("##nav_search", _searchText, 0.0f))
                        _searchDirty = true;
                    if (_searchDirty && !Gui.ItemActive)
                    {
                        _searchDirty = false;
                        _history.Push(CurrentState());
                    }
                    Gui.Space(6.0f);
                    using var card = Gui.Card("##nav_search_filters", "Search filter", Icon.Search, null, Style.Color(StyleColor.Icon));
                    if (card.IsOpen)
                    {
                        float width = Gui.AvailableSize.x;
                        using var canvas = Gui.Canvas("##nav_quick_filters", new Vector2(width, 3.0f * (SegHeight + 3.0f)));
                        Painter draw = canvas.Draw;
                        DrawQuickFilter(draw, 0, "Bookmarked", Icon.Star, Style.Color(StyleColor.IconWarning), ref _filterBookmarked, width);
                        DrawQuickFilter(draw, 1, "Recently changed", Icon.Step, Style.Color(StyleColor.IconComponent), ref _filterRecent, width);
                        DrawQuickFilter(draw, 2, "Untagged", Icon.Tag, Style.Color(StyleColor.TextDim), ref _filterUntagged, width);
                    }
                }
                Gui.Space(6.0f);
            }
        }

        private static void DrawQuickFilter(Painter draw, int index, string label, Icon icon, Color32 color, ref bool on, float width)
        {
            Rect rect = Rect.FromXYWH(0.0f, index * (SegHeight + 3.0f), width, SegHeight);
            if (ToggleButton.Draw(draw, "##quick" + index, rect, icon, color, label, null, null, on, true, ToggleDot.State).Clicked())
                on = !on;
        }

        // ------------------------------------------------------------
        // Bookmarks
        // ------------------------------------------------------------
        private void DrawBookmarksPopup(Rect button)
        {
            const float width = 260.0f;
            if (_openBookmarks)
            {
                _openBookmarks = false;
                Gui.OpenPopup(BookmarksPopupId);
            }
            using var popup = Gui.Popup(BookmarksPopupId, new Vector2(button.min.x, button.max.y + 2.0f), new Vector2(width, 0.0f));
            if (!popup.IsOpen)
                return;
            Bookmarks bookmarks = _bookmarks!;
            string query = CurrentQuery();
            float inner = width - 8.0f;
            float height = 22.0f + 4.0f + SegHeight + 4.0f + SegHeight + 6.0f
                + 22.0f + Math.Max(bookmarks.Searches.Count, 1) * RowHeight + 4.0f
                + 22.0f + Math.Max(bookmarks.Items.Count, 1) * RowHeight + 6.0f + SegHeight + 6.0f;
            using var canvas = Gui.Canvas("##nav_bookmarks", new Vector2(width, height));
            Painter draw = canvas.Draw;
            float y = 0.0f;
            draw.RectFilled(Rect.FromXYWH(0.0f, 0.0f, width, 22.0f), Style.Color(StyleColor.PanelHeaderBg), 4.0f);
            using (Gui.Font(FontStyle.Bold))
                draw.Text(new Vector2(8.0f, 4.0f), Style.Color(StyleColor.Text), "Bookmarks");
            y += 26.0f;
            float half = (inner - 4.0f) * 0.5f;
            if (ToggleButton.Draw(draw, "##add_search", Rect.FromXYWH(4.0f, y, half, SegHeight), Icon.Search, null, "Add search", null,
                query.Length > 0 ? $"Save {NavigatorState.ModeName(_mode)}: {query}" : "Nothing to save", false, query.Length > 0, ToggleDot.None).Clicked())
                bookmarks.AddSearch(NavigatorState.ModeName(_mode), query);
            if (ToggleButton.Draw(draw, "##add_selection", Rect.FromXYWH(8.0f + half, y, half, SegHeight), Icon.Plus, null, "Add selection", null,
                _selected ?? "Select an asset first", false, _selected is not null, ToggleDot.None).Clicked() && _selected is not null)
                bookmarks.AddItem(_selected);
            y += SegHeight + 4.0f;
            if (ToggleButton.Draw(draw, "##close_on_select", Rect.FromXYWH(4.0f, y, inner, SegHeight), Icon.None, null, "Close on selection", null, null,
                bookmarks.CloseOnSelection, true, ToggleDot.State).Clicked())
                bookmarks.CloseOnSelection = !bookmarks.CloseOnSelection;
            y += SegHeight + 6.0f;

            bool close = false;
            y = DrawBookmarkHeader(draw, "Saved searches", y, width);
            if (bookmarks.Searches.Count == 0)
                y = DrawEmptyRow(draw, "No saved searches", y);
            SavedSearch? removeSearch = null;
            for (int i = 0; i < bookmarks.Searches.Count; i++)
            {
                SavedSearch search = bookmarks.Searches[i];
                NavigatorMode mode = NavigatorState.ParseMode(search.Mode);
                Icon icon = mode == NavigatorMode.Tags ? Icon.Tag : mode == NavigatorMode.Search ? Icon.Search : Icon.Folder;
                int action = DrawBookmarkRow(draw, "##search" + i, icon, search.Query, y, width);
                if (action == 1)
                {
                    ApplySavedSearch(search);
                    close = bookmarks.CloseOnSelection;
                }
                else if (action == 2)
                    removeSearch = search;
                y += RowHeight;
            }
            y += 4.0f;
            y = DrawBookmarkHeader(draw, "Saved items", y, width);
            if (bookmarks.Items.Count == 0)
                y = DrawEmptyRow(draw, "No saved items", y);
            string? removeItem = null;
            for (int i = 0; i < bookmarks.Items.Count; i++)
            {
                string item = bookmarks.Items[i];
                AssetEntry? entry = _index!.Find(item);
                Icon icon = entry is null ? Icon.Warning : AssetIndex.IconOf(entry.Kind);
                int action = DrawBookmarkRow(draw, "##item" + i, icon, item, y, width);
                if (action == 1)
                {
                    RevealItem(item);
                    close = bookmarks.CloseOnSelection;
                }
                else if (action == 2)
                    removeItem = item;
                y += RowHeight;
            }
            y += 6.0f;
            if (ToggleButton.Draw(draw, "##bookmarks_done", Rect.FromXYWH(4.0f, y, inner, SegHeight), Icon.None, null, "Done", null, null, false, true, ToggleDot.None).Clicked())
                close = true;
            if (removeSearch is not null)
                bookmarks.RemoveSearch(removeSearch);
            if (removeItem is not null)
                bookmarks.RemoveItem(removeItem);
            if (close)
                Gui.ClosePopup();
        }

        private static float DrawBookmarkHeader(Painter draw, string title, float y, float width)
        {
            draw.RectFilled(Rect.FromXYWH(0.0f, y, width, 22.0f), Style.Color(StyleColor.PanelHeaderBg));
            float w = Gui.MeasureText(title).x;
            draw.Text(new Vector2(MathF.Round((width - w) * 0.5f), y + 4.0f), Style.Color(StyleColor.Text), title);
            return y + 22.0f;
        }

        private static float DrawEmptyRow(Painter draw, string text, float y)
        {
            draw.Text(new Vector2(10.0f, y + 2.0f), Style.Color(StyleColor.TextDim), text);
            return y + RowHeight;
        }

        // 0 = nothing, 1 = row clicked, 2 = remove clicked.
        private static int DrawBookmarkRow(Painter draw, string id, Icon icon, string text, float y, float width)
        {
            Rect row = Rect.FromXYWH(2.0f, y, width - 4.0f, RowHeight);
            Result hit = Gui.HitArea(id, row);
            bool hovered = hit.Hovered();
            if (hovered)
                draw.RectFilled(row, Style.Color(StyleColor.Selection), 3.0f);
            draw.Icon(icon, new Vector2(row.min.x + 10.0f, y + RowHeight * 0.5f), 13.0f, Style.Color(StyleColor.Icon));
            float textY = MathF.Round(y + (RowHeight - Gui.MeasureText("A").y) * 0.5f);
            draw.PushClip(new Rect(new Vector2(row.min.x, y), new Vector2(row.max.x - 22.0f, y + RowHeight)));
            draw.Text(new Vector2(row.min.x + 22.0f, textY), Style.Color(StyleColor.Text), text);
            draw.PopClip();
            if (hovered || (hit & Result.Active) != 0)
            {
                Rect trash = Rect.FromXYWH(row.max.x - 20.0f, y + 2.0f, 16.0f, 16.0f);
                Result trashHit = Gui.HitArea(id + "_remove", trash);
                Gui.Tooltip("Remove bookmark");
                draw.Icon(Icon.Trash, trash.Center, 12.0f, Style.Color(trashHit.Hovered() ? StyleColor.Text : StyleColor.TextDim));
                if (trashHit.Clicked())
                    return 2;
            }
            return hit.Clicked() ? 1 : 0;
        }

        private string CurrentQuery() => _mode switch
        {
            NavigatorMode.Directory => _folder,
            NavigatorMode.Tags => _tagInput.Query.ToString(),
            _ => _searchText.Text.Trim(),
        };

        private void ApplySavedSearch(SavedSearch search)
        {
            NavigatorMode mode = NavigatorState.ParseMode(search.Mode);
            switch (mode)
            {
                case NavigatorMode.Directory:
                    Navigate(search.Query);
                    break;
                case NavigatorMode.Tags:
                    _tagInput.SetQuery(search.Query);
                    ApplyState(CurrentState() with { Mode = NavigatorMode.Tags, Tags = _tagInput.Query.ToString() }, true);
                    break;
                default:
                    ApplyState(CurrentState() with { Mode = NavigatorMode.Search, Search = search.Query }, true);
                    break;
            }
        }

        private void RevealItem(string relPath)
        {
            AssetEntry? entry = _index!.Find(relPath);
            if (entry is null)
                return;
            Navigate(entry.IsFolder ? entry.RelPath : entry.Parent);
            _selected = entry.RelPath;
        }

        // ------------------------------------------------------------
        // Grid section
        // ------------------------------------------------------------
        private void DrawGrid()
        {
            DrawBreadcrumb();
            DrawGridToolbar();
            if (_option != OptionTab.None)
                DrawOptionRow();
            DrawResults();
        }

        private void DrawBreadcrumb()
        {
            using var canvas = Gui.Canvas("##nav_breadcrumb", new Vector2(0.0f, 22.0f));
            Painter draw = canvas.Draw;
            draw.RectFilled(new Rect(new Vector2(0.0f, 0.0f), canvas.Size), Style.Color(StyleColor.TopBarBg));
            float textY = MathF.Round((22.0f - Gui.MeasureText("A").y) * 0.5f);
            Color32 text = Style.Color(StyleColor.Text);
            Color32 dim = Style.Color(StyleColor.TextDim);
            if (_mode != NavigatorMode.Directory)
            {
                string title = _mode == NavigatorMode.Tags ? "Tag results" : "Search results";
                draw.Text(new Vector2(8.0f, textY), text, title);
                draw.Text(new Vector2(14.0f + Gui.MeasureText(title).x, textY), dim, $"· {MatchCount()} files");
                return;
            }
            string[] parts = _folder.Split('/');
            float x = 8.0f;
            for (int i = 0; i < parts.Length; i++)
            {
                float w = Gui.MeasureText(parts[i]).x;
                Rect segment = Rect.FromXYWH(x - 3.0f, 2.0f, w + 6.0f, 18.0f);
                Result hit;
                using (Gui.Id(i))
                    hit = Gui.HitArea("##crumb", segment);
                bool last = i == parts.Length - 1;
                if (hit.Hovered() && !last)
                    draw.RectFilled(segment, Style.Color(StyleColor.PanelHeaderHover), 3.0f);
                draw.Text(new Vector2(x, textY), last ? text : dim, parts[i]);
                if (hit.Clicked() && !last)
                    Navigate(string.Join('/', parts, 0, i + 1));
                x += w + 6.0f;
                if (!last)
                {
                    DrawChevronRight(draw, new Vector2(x + 3.0f, 11.0f), dim);
                    x += 12.0f;
                }
            }
        }

        private void DrawGridToolbar()
        {
            using (Gui.Toolbar())
            {
                using (var canvas = Gui.Canvas("##nav_options", new Vector2(SegmentWidth(4), SegHeight + 4.0f)))
                {
                    Painter draw = canvas.Draw;
                    Rect[] cells = SegmentCells(draw, 4);
                    int tokenKinds;
                    ParseGridSearch(out _, out tokenKinds);
                    bool filtered = (_kinds | tokenKinds) != 0;
                    if (SegIcon(draw, "##opt_hide", cells[0], Icon.EyeOff, "Hide options", _option == OptionTab.None, true).Clicked())
                        _option = OptionTab.None;
                    if (SegIcon(draw, "##opt_view", cells[1], Icon.View, "View", _option == OptionTab.View, true).Clicked())
                        _option = _option == OptionTab.View ? OptionTab.None : OptionTab.View;
                    if (SegIcon(draw, "##opt_filter", cells[2], Icon.Filter, filtered ? "Filter (active)" : "Filter", _option == OptionTab.Filter, true,
                        filtered ? ToggleDot.Accent : ToggleDot.None).Clicked())
                        _option = _option == OptionTab.Filter ? OptionTab.None : OptionTab.Filter;
                    if (SegIcon(draw, "##opt_sort", cells[3], Icon.Sort, "Sorting", _option == OptionTab.Sort, true).Clicked())
                        _option = _option == OptionTab.Sort ? OptionTab.None : OptionTab.Sort;
                }
                Gui.Space(8.0f);
                Gui.SearchField("##nav_grid_search", _gridSearch, 0.0f);
                Gui.Tooltip("Filter by name; t:image, t:script, ... filter by type");
                if (Gui.Button("Clear"))
                {
                    _gridSearch.Text = string.Empty;
                    _kinds = 0;
                    if (_mode == NavigatorMode.Tags && !_tagInput.Query.IsEmpty)
                        _tagInput.SetQuery(string.Empty);
                    if (_mode == NavigatorMode.Search)
                        _searchText.Text = string.Empty;
                    _history.Push(CurrentState());
                }
            }
        }

        private void DrawOptionRow()
        {
            using (Gui.Horizontal(0.0f))
            {
                Gui.Space(6.0f);
                switch (_option)
                {
                    case OptionTab.View:
                        DrawViewRow();
                        break;
                    case OptionTab.Filter:
                        DrawFilterRow();
                        break;
                    case OptionTab.Sort:
                        DrawSortRow();
                        break;
                }
                Gui.Space(6.0f);
            }
        }

        private void DrawViewRow()
        {
            using var canvas = Gui.Canvas("##nav_view_row", new Vector2(0.0f, OptionRowHeight));
            Painter draw = canvas.Draw;
            string[] labels = { "Details", "Small", "Medium", "Large" };
            Icon[] icons = { Icon.Console, Icon.View, Icon.View, Icon.View };
            float width = Math.Min((canvas.Size.x - 3.0f * 2.0f) / 4.0f, 110.0f);
            for (int i = 0; i < 4; i++)
            {
                Rect rect = Rect.FromXYWH(i * (width + 2.0f), 2.0f, width, SegHeight);
                if (ToggleButton.Draw(draw, "##view" + i, rect, icons[i], null, labels[i], null, null, (int)_view == i, true, ToggleDot.State).Clicked())
                    _view = (ViewSize)i;
            }
        }

        private void DrawSortRow()
        {
            using var canvas = Gui.Canvas("##nav_sort_row", new Vector2(0.0f, OptionRowHeight));
            Painter draw = canvas.Draw;
            string[] labels = { "No sorting", "By type", "By status", "By user" };
            Icon[] icons = { Icon.Console, Icon.Cards, Icon.Info, Icon.Account };
            float width = (canvas.Size.x - 3.0f * 2.0f) / 4.0f;
            for (int i = 0; i < 4; i++)
            {
                bool enabled = i < 2;
                Rect rect = Rect.FromXYWH(i * (width + 2.0f), 2.0f, width, SegHeight);
                bool on = (int)_sort == i;
                Result hit = ToggleButton.Draw(draw, "##sort" + i, rect, icons[i], null, labels[i], null, enabled ? null : "Not available yet", on, enabled, ToggleDot.None);
                if (hit.Clicked())
                    _sort = (SortMode)i;
            }
        }

        private void DrawFilterRow()
        {
            ParseGridSearch(out _, out int tokenKinds);
            int kinds = _kinds | tokenKinds;
            int filesTotal = 0;
            foreach (AssetKind kind in _filterKinds)
                filesTotal += _kindCounts[(int)kind];

            using var canvas = Gui.Canvas("##nav_filter_row", new Vector2(0.0f, OptionRowHeight));
            Painter draw = canvas.Draw;
            float available = canvas.Size.x;
            int count = _filterKinds.Length + 1;
            string[] labels = new string[count];
            string[] badges = new string[count];
            labels[0] = "All";
            badges[0] = filesTotal.ToString();
            for (int i = 0; i < _filterKinds.Length; i++)
            {
                labels[i + 1] = AssetIndex.PluralName(_filterKinds[i]);
                badges[i + 1] = _kindCounts[(int)_filterKinds[i]].ToString();
            }

            bool iconOnly = false;
            float total = 0.0f;
            for (int i = 0; i < count; i++)
                total += ToggleButton.Measure(labels[i], badges[i], false) + 2.0f;
            if (total > available)
                iconOnly = true;
            float iconWidth = ToggleButton.Measure(null, null, true) + 4.0f;
            float moreWidth = 34.0f;
            int visible = count;
            if (iconOnly && count * (iconWidth + 2.0f) > available)
                visible = Math.Max(1, (int)((available - moreWidth - 2.0f) / (iconWidth + 2.0f)));

            float x = 0.0f;
            for (int i = 0; i < visible; i++)
            {
                float w = iconOnly ? iconWidth : ToggleButton.Measure(labels[i], badges[i], false);
                Rect rect = Rect.FromXYWH(x, 2.0f, w, SegHeight);
                DrawKindToggle(draw, i, rect, kinds, iconOnly, labels[i], badges[i]);
                x += w + 2.0f;
            }
            if (visible < count)
            {
                Rect more = Rect.FromXYWH(x, 2.0f, moreWidth, SegHeight);
                int hidden = count - visible;
                bool anyHiddenOn = false;
                for (int i = visible; i < count; i++)
                    anyHiddenOn |= i > 0 && (kinds & (1 << (int)_filterKinds[i - 1])) != 0;
                Result hit = Gui.HitArea("##kind_more", more);
                Gui.Tooltip($"{hidden} more types");
                ToggleButton.Background(draw, more, false, true, hit.Hovered(), (hit & Result.Active) != 0);
                string text = "+" + hidden;
                Vector2 size = Gui.MeasureText(text);
                draw.Text(new Vector2(MathF.Round(more.Center.x - size.x * 0.5f), MathF.Round(more.Center.y - size.y * 0.5f)), Style.Color(StyleColor.Text), text);
                ToggleButton.Dot(draw, more, anyHiddenOn ? ToggleDot.Accent : ToggleDot.None, false, true);
                if (hit.Clicked())
                    _openKindMore = true;
                _kindMoreRect = Offset(more, canvas.Bounds.min);
            }
            DrawKindMorePopup(visible, count, kinds, labels, badges);
        }

        private void DrawKindToggle(Painter draw, int index, Rect rect, int kinds, bool iconOnly, string label, string badge)
        {
            bool on;
            Icon icon;
            Color32? color;
            if (index == 0)
            {
                on = kinds == 0;
                icon = Icon.Cards;
                color = null;
            }
            else
            {
                AssetKind kind = _filterKinds[index - 1];
                on = (kinds & (1 << (int)kind)) != 0;
                icon = AssetIndex.IconOf(kind);
                color = AssetIndex.IconColorOf(kind);
            }
            string tip = $"{label} ({badge})";
            Result hit = ToggleButton.Draw(draw, "##kind" + index, rect, icon, color, iconOnly ? null : label, iconOnly ? null : badge, tip, on, true, ToggleDot.State);
            if (hit.Clicked())
                ToggleKind(index);
        }

        private void ToggleKind(int index)
        {
            if (index == 0)
                _kinds = 0;
            else
                _kinds ^= 1 << (int)_filterKinds[index - 1];
            PushKinds();
        }

        private void DrawKindMorePopup(int visible, int count, int kinds, string[] labels, string[] badges)
        {
            if (_openKindMore)
            {
                _openKindMore = false;
                Gui.OpenPopup(KindMorePopupId);
            }
            const float width = 180.0f;
            Vector2 position = new(Math.Max(_kindMoreRect.max.x - width, 0.0f), _kindMoreRect.max.y + 2.0f);
            using var popup = Gui.Popup(KindMorePopupId, position, new Vector2(width, 0.0f));
            if (!popup.IsOpen)
                return;
            int rows = count - visible;
            using var canvas = Gui.Canvas("##nav_kind_more_rows", new Vector2(width, rows * (SegHeight + 2.0f) + 6.0f));
            Painter draw = canvas.Draw;
            for (int i = visible; i < count; i++)
            {
                Rect rect = Rect.FromXYWH(4.0f, 4.0f + (i - visible) * (SegHeight + 2.0f), width - 8.0f, SegHeight);
                DrawKindToggle(draw, i, rect, kinds, false, labels[i], badges[i]);
            }
        }

        private void DrawResults()
        {
            using var area = Gui.ScrollArea("##nav_results", new Vector2(0.0f, 0.0f), ScrollAreaFlags.Padded);
            if (!area.IsOpen)
                return;
            if (_sections.Count == 0)
            {
                string message = _mode switch
                {
                    NavigatorMode.Tags when _tagInput.Query.IsEmpty => "Add tags to find assets.",
                    NavigatorMode.Search when _searchText.Text.Trim().Length == 0 && !_filterBookmarked && !_filterRecent && !_filterUntagged => "Type a name or pick a filter to search.",
                    _ => "No matching assets.",
                };
                using var canvas = Gui.Canvas("##nav_empty", new Vector2(0.0f, 24.0f));
                canvas.Draw.Text(new Vector2(4.0f, 4.0f), Style.Color(StyleColor.TextDim), message);
                return;
            }
            if (_view == ViewSize.Details)
            {
                DrawDetails();
                DrawContextPopup(0);
                return;
            }
            for (int s = 0; s < _sections.Count; s++)
            {
                using (Gui.Id(s))
                {
                    DrawTileSection(_sections[s], s, s == _sections.Count - 1);
                    DrawContextPopup(s);
                }
                if (s < _sections.Count - 1)
                    Gui.Space(6.0f);
            }
        }

        private static string SectionLabel(Section section) => $"{section.Title} ({section.Items.Count(i => !i.Up)})";

        private void DrawSectionBar(Painter draw, Rect bar, string label)
        {
            using (Gui.Font(FontStyle.Bold))
            {
                float labelWidth = Gui.MeasureText(label).x;
                float textY = MathF.Round((bar.Height - Gui.MeasureText("A").y) * 0.5f);
                draw.Text(new Vector2(MathF.Round(bar.Center.x - labelWidth * 0.5f), bar.min.y + textY), Style.Color(StyleColor.Text), label);
            }
        }

        private (float Tile, float Icon) TileSize() => _view switch
        {
            ViewSize.Small => (60.0f, 26.0f),
            ViewSize.Large => (112.0f, 64.0f),
            _ => (80.0f, 40.0f),
        };

        private void DrawTileSection(Section section, int sectionIndex, bool last)
        {
            const float barHeight = 22.0f;
            const float pad = 8.0f;
            (float tile, float iconSize) = TileSize();
            float lineHeight = Gui.MeasureText("A").y;
            float tileHeight = tile + lineHeight - 8.0f;
            float width = Gui.AvailableSize.x;
            float inner = width - pad * 2.0f;
            int columns = Math.Max(1, (int)((inner + TileGap) / (tile + TileGap)));
            int rows = (section.Items.Count + columns - 1) / columns;
            float height = barHeight + pad * 2.0f + Math.Max(0.0f, rows * (tileHeight + TileGap) - TileGap);
            if (last)
                height = Math.Max(height, MathF.Floor(Gui.AvailableSize.y) - 1.0f);
            using var canvas = Gui.Canvas("##nav_tiles", new Vector2(width, height));
            Painter draw = canvas.Draw;
            Rect frame = Rect.FromXYWH(0.0f, 0.0f, width, height);
            draw.RectFilled(frame, Style.Color(StyleColor.HeaderBg), 4.0f);
            draw.RectFilled(Rect.FromXYWH(0.0f, 0.0f, width, barHeight), _sectionBg, 4.0f);
            draw.RectFilled(Rect.FromXYWH(0.0f, barHeight * 0.5f, width, barHeight * 0.5f), _sectionBg);
            DrawSectionBar(draw, Rect.FromXYWH(0.0f, 0.0f, width, barHeight), SectionLabel(section));
            draw.Rect(frame, Style.Color(StyleColor.Border), 4.0f);
            for (int i = 0; i < section.Items.Count; i++)
            {
                GridItem item = section.Items[i];
                Rect rect = Rect.FromXYWH(pad + (i % columns) * (tile + TileGap), barHeight + pad + (i / columns) * (tileHeight + TileGap), tile, tileHeight);
                Result hit;
                using (Gui.Id(i))
                    hit = Gui.HitArea("##tile", rect);
                string name = item.Up ? ".." : item.Entry!.Name;
                Gui.Tooltip(item.Up ? "Up one folder" : item.Entry!.RelPath);
                bool selected = !item.Up && string.Equals(_selected, item.Entry!.RelPath, StringComparison.OrdinalIgnoreCase);
                if (selected)
                {
                    draw.RectFilled(rect, Style.Color(StyleColor.Selection), 4.0f);
                    draw.RectFilled(Rect.FromXYWH(rect.min.x, rect.min.y, EditorStyle.SelectionBarWidth, rect.Height), Style.Color(StyleColor.Accent));
                }
                else if (hit.Hovered())
                {
                    draw.RectFilled(rect, new Color32(255, 255, 255, 16), 4.0f);
                }

                Rect iconRect = Rect.FromCenterSize(new Vector2(rect.Center.x, rect.min.y + 6.0f + iconSize * 0.5f), new Vector2(iconSize, iconSize));
                DrawAssetIcon(draw, item, iconRect, iconSize);
                string label = Ellipsize(name, (int)(tile - 4.0f));
                float labelWidth = Gui.MeasureText(label).x;
                draw.Text(new Vector2(MathF.Round(rect.Center.x - labelWidth * 0.5f), rect.max.y - lineHeight - 3.0f),
                    selected ? Style.Color(StyleColor.TextSelected) : Style.Color(StyleColor.Text), label);

                HandleItemInput(hit, item, sectionIndex, rect, canvas.Bounds);
            }
        }

        private void DrawDetails()
        {
            const float headerHeight = 22.0f;
            float width = Gui.AvailableSize.x;
            bool showType = width >= 360.0f;
            bool showSize = width >= 280.0f;
            List<(DetailColumn Column, string Title, float X, float Width)> columns = new();
            float nameShare = 0.42f + (showType ? 0.0f : 0.16f) + (showSize ? 0.0f : 0.14f);
            float x = 0.0f;
            columns.Add((DetailColumn.Name, "Name", x, width * nameShare));
            x += width * nameShare;
            if (showType)
            {
                columns.Add((DetailColumn.Type, "Type", x, width * 0.16f));
                x += width * 0.16f;
            }
            if (showSize)
            {
                columns.Add((DetailColumn.Size, "Size", x, width * 0.14f));
                x += width * 0.14f;
            }
            columns.Add((DetailColumn.Modified, "Last modified", x, width - x));

            float height = headerHeight;
            foreach (Section section in _sections)
                height += RowHeight + section.Items.Count * RowHeight;
            float frameHeight = Math.Max(height + 1.0f, MathF.Floor(Gui.AvailableSize.y) - 1.0f);
            using var canvas = Gui.Canvas("##nav_details", new Vector2(width, frameHeight));
            Painter draw = canvas.Draw;
            Color32 border = Style.Color(StyleColor.Border);
            Color32 text = Style.Color(StyleColor.Text);
            Color32 dim = Style.Color(StyleColor.TextDim);
            float textY = MathF.Round((RowHeight - Gui.MeasureText("A").y) * 0.5f);
            Color32 evenRow = Style.Color(StyleColor.ToolbarBg);
            Color32 oddRow = new(42, 42, 42, 255);

            for (int c = 0; c < columns.Count; c++)
            {
                (DetailColumn column, string title, float cx, float cw) = columns[c];
                Rect cell = Rect.FromXYWH(cx, 0.0f, cw, headerHeight);
                Result hit;
                using (Gui.Id(c))
                    hit = Gui.HitArea("##detail_header", cell);
                Gui.Tooltip($"Sort by {title.ToLowerInvariant()}");
                bool active = column == _sortColumn;
                draw.RectFilled(cell, Style.Color(hit.Hovered() ? StyleColor.ButtonHover : StyleColor.Button));
                if (c > 0)
                    draw.Line(new Vector2(cx, 0.0f), new Vector2(cx, headerHeight), Style.Color(StyleColor.ToolbarBg));
                draw.PushClip(Rect.FromXYWH(cx, 0.0f, cw - 2.0f, headerHeight));
                draw.Text(new Vector2(cx + 8.0f, MathF.Round((headerHeight - Gui.MeasureText("A").y) * 0.5f)), active ? Style.Color(StyleColor.TextSelected) : text, title);
                if (active)
                {
                    float ax = cx + 8.0f + Gui.MeasureText(title).x + 9.0f;
                    float ay = headerHeight * 0.5f;
                    Color32 accent = Style.Color(StyleColor.Accent);
                    if (_sortDescending)
                        draw.TriangleFilled(new Vector2(ax - 4.0f, ay - 2.0f), new Vector2(ax + 4.0f, ay - 2.0f), new Vector2(ax, ay + 3.0f), accent);
                    else
                        draw.TriangleFilled(new Vector2(ax - 4.0f, ay + 2.0f), new Vector2(ax, ay - 3.0f), new Vector2(ax + 4.0f, ay + 2.0f), accent);
                }
                draw.PopClip();
                if (hit.Clicked())
                {
                    if (active)
                        _sortDescending = !_sortDescending;
                    else
                    {
                        _sortColumn = column;
                        _sortDescending = false;
                    }
                }
            }

            float y = headerHeight;
            for (int s = 0; s < _sections.Count; s++)
            {
                Section section = _sections[s];
                Rect bar = Rect.FromXYWH(0.0f, y, width, RowHeight);
                draw.RectFilled(bar, _sectionBg);
                draw.Line(new Vector2(0.0f, y), new Vector2(width, y), border);
                draw.Line(new Vector2(0.0f, y + RowHeight - 1.0f), new Vector2(width, y + RowHeight - 1.0f), border);
                DrawSectionBar(draw, bar, SectionLabel(section));
                y += RowHeight;

                for (int i = 0; i < section.Items.Count; i++)
                {
                    GridItem item = section.Items[i];
                    Rect rect = Rect.FromXYWH(0.0f, y, width, RowHeight);
                    Result hit;
                    using (Gui.Id(s * 100000 + i))
                        hit = Gui.HitArea("##detail_row", rect);
                    bool selected = !item.Up && string.Equals(_selected, item.Entry!.RelPath, StringComparison.OrdinalIgnoreCase);
                    if (selected)
                    {
                        draw.RectFilled(rect, Style.Color(StyleColor.Selection));
                        draw.RectFilled(Rect.FromXYWH(0.0f, y, EditorStyle.SelectionBarWidth, RowHeight), Style.Color(StyleColor.Accent));
                    }
                    else
                    {
                        draw.RectFilled(rect, i % 2 == 0 ? evenRow : oddRow);
                        if (hit.Hovered())
                            draw.RectFilled(rect, new Color32(255, 255, 255, 16));
                    }

                    Color32 rowText = selected ? Style.Color(StyleColor.TextSelected) : text;
                    Color32 rowDim = selected ? Style.Color(StyleColor.TextSelected) : dim;
                    foreach ((DetailColumn column, _, float cx, float cw) in columns)
                    {
                        draw.PushClip(Rect.FromXYWH(cx, y, cw - 4.0f, RowHeight));
                        switch (column)
                        {
                            case DetailColumn.Name:
                                DrawAssetIcon(draw, item, Rect.FromCenterSize(new Vector2(cx + 14.0f, y + RowHeight * 0.5f), new Vector2(14.0f, 14.0f)), 14.0f);
                                draw.Text(new Vector2(cx + 28.0f, y + textY), rowText, item.Up ? ".." : item.Entry!.Name);
                                break;
                            case DetailColumn.Type:
                                draw.Text(new Vector2(cx + 8.0f, y + textY), rowDim, item.Up ? "Up one folder" : TypeLabel(item.Entry!));
                                break;
                            case DetailColumn.Size:
                                if (!item.Up)
                                    draw.Text(new Vector2(cx + 8.0f, y + textY), item.Entry!.IsFolder ? rowDim : rowText, SizeLabel(item.Entry!));
                                break;
                            default:
                                if (!item.Up)
                                    draw.Text(new Vector2(cx + 8.0f, y + textY), rowDim, item.Entry!.Modified.ToString("yyyy-MM-dd HH:mm"));
                                break;
                        }
                        draw.PopClip();
                    }
                    HandleItemInput(hit, item, 0, rect, canvas.Bounds);
                    y += RowHeight;
                }
            }
            for (int i = _sections[^1].Items.Count; y < frameHeight; i++)
            {
                draw.RectFilled(Rect.FromXYWH(0.0f, y, width, Math.Min(RowHeight, frameHeight - y)), i % 2 == 0 ? evenRow : oddRow);
                y += RowHeight;
            }
            draw.Rect(Rect.FromXYWH(0.0f, 0.0f, width, frameHeight), border, 3.0f);
        }

        private void HandleItemInput(Result hit, GridItem item, int sectionIndex, Rect rect, Rect canvasBounds)
        {
            if (item.Up)
            {
                if ((hit & Result.DoubleClicked) != 0)
                {
                    string from = _folder;
                    Navigate(_folder[.._folder.LastIndexOf('/')]);
                    _selected = from;
                }
                return;
            }
            AssetEntry entry = item.Entry!;
            if (hit.Clicked())
            {
                _selected = entry.RelPath;
                if (!entry.IsFolder)
                    Selection.Set(new AssetRef(entry.RelPath));
            }
            if ((hit & Result.DoubleClicked) != 0)
            {
                if (entry.IsFolder)
                    Navigate(entry.RelPath);
                else
                    OpenFile(entry.FullPath);
            }
            if ((hit & Result.RightClicked) != 0)
            {
                _selected = entry.RelPath;
                _contextPath = entry.RelPath;
                _contextSection = sectionIndex;
                _contextRect = Offset(rect, canvasBounds.min);
                _openContext = true;
            }
        }

        private void DrawContextPopup(int sectionIndex)
        {
            if (_contextSection != sectionIndex)
                return;
            if (_openContext)
            {
                _openContext = false;
                Gui.OpenPopup(ContextPopupId);
            }
            const float width = 180.0f;
            using var popup = Gui.Popup(ContextPopupId, new Vector2(_contextRect.min.x + 8.0f, _contextRect.max.y - 6.0f), new Vector2(width, 0.0f));
            if (!popup.IsOpen || _contextPath is null)
                return;
            bool bookmarked = _bookmarks!.IsBookmarked(_contextPath);
            using var canvas = Gui.Canvas("##nav_context_rows", new Vector2(width, 4.0f * (SegHeight + 2.0f) + 6.0f));
            Painter draw = canvas.Draw;
            float inner = width - 8.0f;
            AssetEntry? entry = _index!.Find(_contextPath);
            bool canTag = entry is { IsFolder: false };
            if (ToggleButton.Draw(draw, "##ctx_tags", Rect.FromXYWH(4.0f, 4.0f, inner, SegHeight), Icon.Tag, null, "Edit tags...", null,
                canTag ? null : "Folders have no tags", false, canTag, ToggleDot.None).Clicked())
            {
                _editTagsPath = _contextPath;
                _openEditTags = true;
                Gui.ClosePopup();
            }
            if (ToggleButton.Draw(draw, "##ctx_bookmark", Rect.FromXYWH(4.0f, 6.0f + SegHeight, inner, SegHeight), Icon.Star, null,
                bookmarked ? "Remove bookmark" : "Bookmark", null, null, false, true, ToggleDot.None).Clicked())
            {
                if (bookmarked)
                    _bookmarks.RemoveItem(_contextPath);
                else
                    _bookmarks.AddItem(_contextPath);
                Gui.ClosePopup();
            }
            if (ToggleButton.Draw(draw, "##ctx_reveal", Rect.FromXYWH(4.0f, 8.0f + SegHeight * 2.0f, inner, SegHeight), Icon.Folder, null,
                "Reveal in Explorer", null, null, false, entry is not null, ToggleDot.None).Clicked() && entry is not null)
            {
                RevealInExplorer(entry.FullPath);
                Gui.ClosePopup();
            }
            if (ToggleButton.Draw(draw, "##ctx_rename", Rect.FromXYWH(4.0f, 10.0f + SegHeight * 3.0f, inner, SegHeight), Icon.File, null,
                "Rename...", null, null, false, entry is not null, ToggleDot.None).Clicked() && entry is not null)
            {
                OpenRename(entry.RelPath);
                Gui.ClosePopup();
            }
        }

        // ------------------------------------------------------------
        // Rename and tag management
        // ------------------------------------------------------------
        // Opens the Rename modal for an asset or folder, after the given number of frames (test hooks).
        public void OpenRename(string relPath, int frames = 0)
        {
            if (frames > 0)
            {
                _renameHookPath = relPath;
                _renameHookIn = frames;
                return;
            }
            _renamePath = relPath;
            _renameText.Text = Path.GetFileName(relPath);
            _renameError = null;
            _renameSetText = true;
            _openRename = true;
        }

        private void DrawRenameModal()
        {
            if (_openRename)
            {
                _openRename = false;
                Gui.OpenPopup(RenameModalId);
            }
            using var modal = Gui.Modal(RenameModalId, "Rename", new Vector2(360.0f, 0.0f));
            if (!modal.IsOpen || _renamePath is null)
                return;
            Gui.Label(_renamePath);
            Gui.Space(4.0f);
            if (_renameSetText)
                Gui.FocusNext();
            TextFieldFlags flags = _renameSetText ? TextFieldFlags.SetText : TextFieldFlags.None;
            _renameSetText = false;
            if ((Gui.TextField("##nav_rename_name", _renameText, "New name", 0.0f, flags) & Result.Changed) != 0)
                _renameError = AssetDatabase.ValidateName(_renameText.Text.Trim());
            if (_renameError is not null)
            {
                using var error = Gui.Canvas("##nav_rename_error", new Vector2(0.0f, 18.0f));
                error.Draw.Text(new Vector2(0.0f, 2.0f), Style.Color(StyleColor.IconWarning), _renameError);
            }
            Gui.Space(6.0f);
            bool commit = Gui.KeyPressed(Key.Enter, false);
            using (Gui.Horizontal())
            {
                Gui.FlexibleSpace();
                if (Gui.Button("Cancel") || Gui.KeyPressed(Key.Escape, false))
                {
                    Gui.ClosePopup();
                    return;
                }
                commit |= Gui.Button("Rename");
            }
            if (!commit)
                return;
            string? result = _db!.Rename(_renamePath, _renameText.Text);
            if (result is null)
                Gui.ClosePopup();
            else
                _renameError = result;
        }

        private void OpenTagModal(string tag)
        {
            _tagEdit = tag;
            _tagRenameText.Text = tag;
            _tagDescriptionText.Text = _tags!.Description(tag);
            _tagConfirmDelete = false;
            _openTag = true;
        }

        private void DrawTagModal()
        {
            if (_openTag)
            {
                _openTag = false;
                Gui.OpenPopup(TagModalId);
            }
            using var modal = Gui.Modal(TagModalId, "Tag", new Vector2(360.0f, 0.0f));
            if (!modal.IsOpen || _tagEdit is null)
                return;
            AssetTags tags = _tags!;
            int count = tags.PathsWithTag(_tagEdit).Count();
            Gui.Label($"'{_tagEdit}' is on {count} asset(s)");
            Gui.Space(4.0f);
            Gui.Field("Name", _tagRenameText);
            Gui.Field("Description", _tagDescriptionText);
            Gui.Space(6.0f);
            using (Gui.Horizontal())
            {
                if (!_tagConfirmDelete)
                {
                    if (Gui.Button("Delete from all assets"))
                        _tagConfirmDelete = true;
                }
                else if (Gui.Button($"Really delete from {count} asset(s)"))
                {
                    tags.Delete(_tagEdit);
                    _tagInput.SetQuery(string.Join(' ', _tagInput.Query.Chips.Where(c => c.Tag != _tagEdit)));
                    Gui.ClosePopup();
                    return;
                }
                Gui.FlexibleSpace();
                if (Gui.Button("Cancel"))
                {
                    Gui.ClosePopup();
                    return;
                }
                if (Gui.Button("Save"))
                {
                    string name = AssetTags.NormalizeTag(_tagRenameText.Text);
                    if (name.Length > 0 && !name.StartsWith('@') && name != _tagEdit)
                    {
                        tags.Rename(_tagEdit, name);
                        _tagInput.SetQuery(string.Join(' ', _tagInput.Query.Chips.Select(c => c.Tag == _tagEdit ? c with { Tag = name } : c)));
                        _tagEdit = name;
                    }
                    tags.SetDescription(_tagEdit, _tagDescriptionText.Text);
                    Gui.ClosePopup();
                }
            }
        }

        private void OnAssetMoved(AssetMove move)
        {
            _folder = move.Apply(_folder);
            if (_selected is not null)
                _selected = move.Apply(_selected);
            if (_contextPath is not null)
                _contextPath = move.Apply(_contextPath);
            if (_editTagsPath is not null)
                _editTagsPath = move.Apply(_editTagsPath);
            if (move.IsFolder)
            {
                List<string> expanded = _expanded.Select(move.Apply).ToList();
                _expanded.Clear();
                _expanded.UnionWith(expanded);
                _history.Remap(move.Apply);
                if (_mode == NavigatorMode.Directory)
                    _pathField.SetText(_folder, false);
            }
            _thumbnails.Clear();
        }

        private void DrawEditTagsModal()
        {
            if (_openEditTags)
            {
                _openEditTags = false;
                _editTagField.SetText(string.Empty, true);
                Gui.OpenPopup(EditTagsModalId);
            }
            using var modal = Gui.Modal(EditTagsModalId, "Edit tags", new Vector2(340.0f, 0.0f));
            if (!modal.IsOpen || _editTagsPath is null)
                return;
            Gui.Label(_editTagsPath);
            Gui.Space(4.0f);
            IReadOnlyList<string> tags = _tags!.Get(_editTagsPath);
            string? remove = null;
            if (tags.Count == 0)
            {
                using var empty = Gui.Canvas("##edit_tags_empty", new Vector2(0.0f, 20.0f));
                empty.Draw.Text(new Vector2(0.0f, 2.0f), Style.Color(StyleColor.TextDim), "No tags yet.");
            }
            else
            {
                float width = Gui.AvailableSize.x;
                using var canvas = Gui.Canvas("##edit_tags_list", new Vector2(width, tags.Count * RowHeight));
                Painter draw = canvas.Draw;
                for (int i = 0; i < tags.Count; i++)
                {
                    float y = i * RowHeight;
                    draw.Icon(Icon.Tag, new Vector2(8.0f, y + RowHeight * 0.5f), 12.0f, Style.Color(StyleColor.IconComponent));
                    draw.Text(new Vector2(20.0f, y + 2.0f), Style.Color(StyleColor.Text), tags[i]);
                    Rect trash = Rect.FromXYWH(width - 20.0f, y + 2.0f, 16.0f, 16.0f);
                    Result hit;
                    using (Gui.Id(i))
                        hit = Gui.HitArea("##edit_tag_remove", trash);
                    Gui.Tooltip("Remove " + tags[i]);
                    draw.Icon(Icon.Trash, trash.Center, 12.0f, Style.Color(hit.Hovered() ? StyleColor.Text : StyleColor.TextDim));
                    if (hit.Clicked())
                        remove = tags[i];
                }
            }
            if (remove is not null)
                _tags.Remove(_editTagsPath, remove);
            Gui.Space(4.0f);
            AutoCompleteEvent ev = _editTagField.OnGUI("Add a tag", 0.0f, out string value);
            if (ev == AutoCompleteEvent.Commit)
            {
                _tags.Add(_editTagsPath, value);
                _editTagField.SetText(string.Empty, true);
            }
            Gui.Space(6.0f);
            using (Gui.Horizontal())
            {
                Gui.FlexibleSpace();
                if (Gui.Button("Done"))
                    Gui.ClosePopup();
            }
        }

        // ------------------------------------------------------------
        // Icons, thumbnails and helpers
        // ------------------------------------------------------------
        private void DrawAssetIcon(Painter draw, GridItem item, Rect rect, float size)
        {
            if (item.Up)
            {
                draw.Icon(Icon.Folder, rect.Center, size, Style.Color(StyleColor.TextDim));
                DrawLeftArrow(draw, new Vector2(rect.Center.x, rect.Center.y + size * 0.08f), Style.Color(StyleColor.ToolbarBg));
                return;
            }
            AssetEntry entry = item.Entry!;
            if (entry.Kind == AssetKind.Image && Thumbnail(entry) is Texture texture)
            {
                float scale = Math.Min(rect.Width / texture.Width, rect.Height / texture.Height);
                Vector2 fit = new(texture.Width * scale, texture.Height * scale);
                draw.Image(texture, Rect.FromCenterSize(rect.Center, fit));
                return;
            }
            draw.Icon(AssetIndex.IconOf(entry.Kind), rect.Center, size, AssetIndex.IconColorOf(entry.Kind));
        }

        private Texture? Thumbnail(AssetEntry entry)
        {
            if (entry.Extension != "png" && entry.Extension != "bmp")
                return null;
            if (_thumbnails.TryGetValue(entry.RelPath, out var cached) && cached.Modified == entry.Modified)
                return cached.Texture;
            if (_thumbnailLoads >= 4)
                return null;
            _thumbnailLoads++;
            if (cached.Texture is not null)
                cached.Texture.Dispose();
            Texture? texture = null;
            byte[]? pixels = ImageFile.LoadRGBA(entry.FullPath, out int width, out int height);
            if (pixels is not null && width > 0 && height > 0 && width <= MaxThumbnailSide && height <= MaxThumbnailSide)
                texture = new Texture(pixels, width, height);
            _thumbnails[entry.RelPath] = (texture, entry.Modified);
            return texture;
        }

        private string Ellipsize(string text, int width)
        {
            if (_ellipsized.TryGetValue((text, width), out string? cached))
                return cached;
            string result = text;
            if (Gui.MeasureText(text).x > width)
            {
                int length = text.Length;
                while (length > 1 && Gui.MeasureText(text[..length] + "…").x > width)
                    length--;
                result = text[..length] + "…";
            }
            if (_ellipsized.Count > 4096)
                _ellipsized.Clear();
            _ellipsized[(text, width)] = result;
            return result;
        }

        private IEnumerable<Suggestion> SuggestFolders(string text)
        {
            if (_index is null)
                yield break;
            string query = AssetIndex.Normalize(text);
            if (query.Length == 0)
                yield break;
            Color32 color = AssetIndex.IconColorOf(AssetKind.Folder);
            List<string> contains = new();
            foreach (string dir in _index.Directories)
            {
                if (dir.StartsWith(query, StringComparison.OrdinalIgnoreCase))
                    yield return new Suggestion(dir, null, Icon.Folder, color);
                else if (dir.Contains(query, StringComparison.OrdinalIgnoreCase))
                    contains.Add(dir);
            }
            foreach (string dir in contains)
                yield return new Suggestion(dir, null, Icon.Folder, color);
        }

        private IEnumerable<Suggestion> SuggestUserTags(string text)
        {
            if (_tags is null || _editTagsPath is null)
                yield break;
            string query = AssetTags.NormalizeTag(text);
            if (query.Length == 0 || query.StartsWith('@'))
                yield break;
            IReadOnlyList<string> existing = _tags.Get(_editTagsPath);
            foreach (TagCount tag in _tags.UserTags())
                if (!existing.Contains(tag.Tag) && tag.Tag.Contains(query, StringComparison.OrdinalIgnoreCase))
                    yield return new Suggestion(tag.Tag, tag.Count.ToString(), Icon.Tag, Style.Color(StyleColor.IconComponent));
        }

        private static void OpenFile(string path)
        {
            try
            {
                Process.Start(new ProcessStartInfo(path) { UseShellExecute = true });
            }
            catch (Exception e)
            {
                Debug.LogError($"[Navigator] Could not open {path}: {e.Message}");
            }
        }

        private static void RevealInExplorer(string path)
        {
            try
            {
                if (OperatingSystem.IsWindows())
                    Process.Start("explorer.exe", $"/select,\"{path}\"");
                else
                    Process.Start(new ProcessStartInfo(Path.GetDirectoryName(path) ?? path) { UseShellExecute = true });
            }
            catch (Exception e)
            {
                Debug.LogError($"[Navigator] Could not reveal {path}: {e.Message}");
            }
        }

        private static Rect Offset(Rect rect, Vector2 offset) => Rect.FromXYWH(rect.min.x + offset.x, rect.min.y + offset.y, rect.Width, rect.Height);

        private static float SegmentWidth(int count) => count * SegButton + (count - 1) * 2.0f + 4.0f;

        private static Rect[] SegmentCells(Painter draw, int count)
        {
            draw.RectFilled(Rect.FromXYWH(0.0f, 0.0f, SegmentWidth(count), SegHeight + 4.0f), Style.Color(StyleColor.SegmentBg), 4.0f);
            Rect[] cells = new Rect[count];
            for (int i = 0; i < count; i++)
                cells[i] = Rect.FromXYWH(2.0f + i * (SegButton + 2.0f), 2.0f, SegButton, SegHeight);
            return cells;
        }

        private static Result SegIcon(Painter draw, string id, Rect rect, Icon icon, string tip, bool on, bool enabled, ToggleDot dot = ToggleDot.None) =>
            ToggleButton.Draw(draw, id, rect, icon, null, null, null, tip, on, enabled, dot);

        private static Result SegGlyph(Painter draw, string id, Rect rect, string tip, bool on, bool enabled, Action<Painter, Vector2, Color32> glyph)
        {
            Result hit = ToggleButton.Draw(draw, id, rect, Icon.None, null, null, null, tip, on, enabled, ToggleDot.None);
            ToggleButton.Colors(on, enabled, null, out Color32 color, out _);
            glyph(draw, rect.Center, color);
            return hit;
        }

        private static void DrawLeftArrow(Painter draw, Vector2 c, Color32 color)
        {
            draw.Line(new Vector2(c.x + 5.0f, c.y), new Vector2(c.x - 5.0f, c.y), color, 1.5f);
            draw.Line(new Vector2(c.x - 5.0f, c.y), new Vector2(c.x - 1.0f, c.y - 4.0f), color, 1.5f);
            draw.Line(new Vector2(c.x - 5.0f, c.y), new Vector2(c.x - 1.0f, c.y + 4.0f), color, 1.5f);
        }

        private static void DrawRightArrow(Painter draw, Vector2 c, Color32 color)
        {
            draw.Line(new Vector2(c.x - 5.0f, c.y), new Vector2(c.x + 5.0f, c.y), color, 1.5f);
            draw.Line(new Vector2(c.x + 5.0f, c.y), new Vector2(c.x + 1.0f, c.y - 4.0f), color, 1.5f);
            draw.Line(new Vector2(c.x + 5.0f, c.y), new Vector2(c.x + 1.0f, c.y + 4.0f), color, 1.5f);
        }

        private static void DrawChevronDown(Painter draw, Vector2 c, Color32 color)
        {
            draw.TriangleFilled(new Vector2(c.x - 4.0f, c.y - 2.0f), new Vector2(c.x + 4.0f, c.y - 2.0f), new Vector2(c.x, c.y + 2.5f), color);
        }

        private static void DrawChevronRight(Painter draw, Vector2 c, Color32 color)
        {
            draw.Line(new Vector2(c.x - 2.0f, c.y - 4.0f), new Vector2(c.x + 2.0f, c.y), color, 1.2f);
            draw.Line(new Vector2(c.x + 2.0f, c.y), new Vector2(c.x - 2.0f, c.y + 4.0f), color, 1.2f);
        }

        private static void DrawRefresh(Painter draw, Vector2 c, Color32 color)
        {
            const int segments = 14;
            Span<Vector2> points = stackalloc Vector2[segments + 1];
            for (int i = 0; i <= segments; i++)
            {
                float angle = MathF.PI * (0.15f + 1.6f * i / segments);
                points[i] = new Vector2(c.x + MathF.Cos(angle) * 5.0f, c.y - MathF.Sin(angle) * 5.0f);
            }
            draw.Polyline(points, color, false, 1.4f);
            Vector2 end = points[segments];
            draw.TriangleFilled(new Vector2(end.x - 3.0f, end.y - 2.5f), new Vector2(end.x + 3.0f, end.y - 2.5f), new Vector2(end.x, end.y + 2.0f), color);
        }
    }
}
