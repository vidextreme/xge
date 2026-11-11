// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.

using System.Text;
using xg;
using xg.imui;
using xg.math;

namespace xgEditor.CoreCLR
{
    // Editor Console: engine and script logs from EditorLog, with per-MessageType filters, search, collapse and a
    // detail pane. Rows are rebuilt from EditorLog.Entries, so the history survives closing the tab (the panel is only
    // hidden) and re-creating the panel; only a module reload, which resets EditorLog, starts empty.
    public sealed class ConsolePanel : Panel
    {
        private const float RowHeight = 22.0f;
        private const float SplitterHeight = 4.0f;
        private const float MinPaneHeight = 40.0f;
        private const float ScrollbarAllowance = 16.0f;
        private const float ToggleSpacing = 8.0f;

        private readonly struct LevelStyle
        {
            public LevelStyle(MessageType type, Icon icon, StyleColor color)
            {
                Type = type;
                Icon = icon;
                Color = color;
                Id = "##level_" + type;
            }

            public MessageType Type { get; }
            public Icon Icon { get; }
            public StyleColor Color { get; }
            public string Id { get; }
        }

        private static readonly LevelStyle[] _levels =
        {
            new(MessageType.Info, Icon.Info, StyleColor.Icon),
            new(MessageType.Success, Icon.Info, StyleColor.AxisY),
            new(MessageType.Warning, Icon.Warning, StyleColor.IconWarning),
            new(MessageType.Critical, Icon.Error, StyleColor.ActiveObject),
            new(MessageType.Error, Icon.Error, StyleColor.IconMaterial),
        };

        private sealed class Row
        {
            public Row(LogEntry entry) => Entry = entry;

            public LogEntry Entry;
            public int Count = 1;
        }

        private readonly List<Row> _rows = new();
        private readonly Dictionary<(MessageType, string, string), Row> _collapsed = new();
        private readonly TextBuffer _search = new(256);
        private readonly bool[] _visible = new bool[_levels.Length];
        private ListState _list;
        private int _seenVersion = -1;
        private bool _dirty = true;
        private bool _follow = true;
        private bool _collapse;
        private bool _timestamps = true;
        private float _detailHeight = 110.0f;
        private float _timeWidth = -1.0f;
        private LogEntry? _selected;
        private string _detail = string.Empty;
        private bool _selectLatestError;

        public ConsolePanel()
        {
            Array.Fill(_visible, true);
        }

        public override string Title => "Console";
        public override string? MenuPath => "General/Console";
        public override DockSlot Slot => DockSlot.Bottom;
        public override Icon PanelIcon => Icon.Console;
        public override PanelFlags Flags => PanelFlags.NoPadding | PanelFlags.NoScrollbar;

        public int RowCount => _rows.Count;

        // Selects the newest Error or Critical entry on the next rebuild (used by the XG_UI_TEST_FOCUS=console hook).
        public void SelectLatestError()
        {
            _selectLatestError = true;
            _dirty = true;
        }

        // Rebuilds the rows from EditorLog now; OnGUI does this automatically when the log changes.
        public void Refresh()
        {
            EditorLog.Drain();
            Rebuild();
        }

        protected override void OnGUI()
        {
            EditorLog.Drain();
            bool grew = EditorLog.Version != _seenVersion;
            if (grew || _dirty)
                Rebuild();

            DrawToolbar();

            Vector2 avail = Gui.AvailableSize;
            float maxDetail = Math.Max(MinPaneHeight, avail.y - MinPaneHeight - SplitterHeight);
            _detailHeight = Math.Clamp(_detailHeight, MinPaneHeight, maxDetail);

            DrawList(avail.x, grew);
            Gui.Splitter("##console_split", ref _detailHeight, MinPaneHeight, maxDetail);
            using (Gui.Font(FontStyle.Mono))
                Gui.TextArea("##console_detail", _detail, new Vector2(0.0f, _detailHeight));
        }

        private void DrawToolbar()
        {
            using (Gui.Toolbar())
            {
                if (FlatButton("Clear"))
                {
                    EditorLog.Clear();
                    Select(null);
                }
                if (Gui.Toggle("Collapse", ref _collapse))
                    _dirty = true;
                Gui.Toggle("Timestamps", ref _timestamps);
                Gui.Space(8.0f);
                if (Gui.SearchField("##console_search", _search, 220.0f))
                    _dirty = true;
                Gui.FlexibleSpace();

                using (Gui.Horizontal(ToggleSpacing))
                {
                    for (int i = 0; i < _levels.Length; ++i)
                    {
                        LevelStyle level = _levels[i];
                        string count = EditorLog.CountOf(level.Type).ToString();
                        if (Gui.IconToggle(level.Id, level.Icon, ref _visible[i], Style.Color(level.Color), count))
                            _dirty = true;
                    }
                }
            }
        }

        private void DrawList(float width, bool grew)
        {
            bool scrollToBottom = _follow && grew;
            _list.Count = _rows.Count;
            _list.RowHeight = RowHeight;
            _list.Flags = scrollToBottom ? ListFlags.ScrollToBottom : ListFlags.None;

            using ListScope list = Gui.List("##console_list", ref _list);
            if (!list.IsOpen)
                return;
            if (!scrollToBottom)
                _follow = _list.AtBottom;

            if (_timeWidth < 0.0f)
                _timeWidth = Gui.MeasureText("[00:00:00.000]").x + 6.0f;

            Color32 text = Style.Color(StyleColor.Text);
            Color32 textSelected = Style.Color(StyleColor.TextSelected);
            Color32 textDim = Style.Color(StyleColor.TextDim);
            Color32 badge = Style.Color(StyleColor.PanelHeaderBg);
            float rowWidth = width - ScrollbarAllowance;
            float textY = (RowHeight - Gui.MeasureText("A").y) * 0.5f;

            for (int i = _list.First; i < _list.Last && i < _rows.Count; ++i)
            {
                Row row = _rows[i];
                LogEntry entry = row.Entry;
                bool selected = IsSelected(entry);
                using ListRowScope scope = Gui.ListRow(i, selected);
                if (scope.Clicked)
                    Select(entry);

                Painter draw = scope.Draw;
                LevelStyle level = StyleOf(entry.Type);
                draw.Icon(level.Icon, new Vector2(14.0f, RowHeight * 0.5f), 14.0f, Style.Color(level.Color));
                float x = 28.0f;
                if (_timestamps && entry.Time.Length > 0)
                {
                    draw.Text(new Vector2(x, textY), textDim, $"[{entry.Time}]");
                    x += _timeWidth;
                }
                draw.Text(new Vector2(x, textY), selected ? textSelected : text, entry.FirstLine);

                if (_collapse && row.Count > 1)
                {
                    string count = row.Count.ToString();
                    float countWidth = Gui.MeasureText(count).x;
                    Rect pill = Rect.FromXYWH(rowWidth - countWidth - 16.0f, 3.0f, countWidth + 10.0f, RowHeight - 6.0f);
                    draw.RectFilled(pill, badge, 8.0f);
                    draw.Text(new Vector2(pill.min.x + 5.0f, textY), text, count);
                }
            }
        }

        private void Rebuild()
        {
            _seenVersion = EditorLog.Version;
            _dirty = false;
            _rows.Clear();
            _collapsed.Clear();

            string search = _search.Text;
            IReadOnlyList<LogEntry> entries = EditorLog.Entries;
            LogEntry? latestError = null;
            for (int i = 0; i < entries.Count; ++i)
            {
                LogEntry entry = entries[i];
                if (entry.Type is MessageType.Error or MessageType.Critical)
                    latestError = entry;
                if (!IsVisible(entry.Type))
                    continue;
                if (search.Length > 0 && entry.Message.IndexOf(search, StringComparison.OrdinalIgnoreCase) < 0)
                    continue;

                if (_collapse)
                {
                    (MessageType, string, string) key = (entry.Type, entry.Message, entry.Context);
                    if (_collapsed.TryGetValue(key, out Row? existing))
                    {
                        ++existing.Count;
                        existing.Entry = entry;
                        continue;
                    }
                    Row row = new(entry);
                    _collapsed.Add(key, row);
                    _rows.Add(row);
                }
                else
                {
                    _rows.Add(new Row(entry));
                }
            }

            if (_selectLatestError && latestError is not null)
            {
                Select(latestError);
                _selectLatestError = false;
            }
            else if (_selected is not null && (entries.Count == 0 || _selected.Sequence < entries[0].Sequence))
            {
                Select(null);
            }
        }

        private void Select(LogEntry? entry)
        {
            _selected = entry;
            _detail = entry is null ? string.Empty : DetailText(entry);
        }

        private bool IsSelected(LogEntry entry)
        {
            if (_selected is null)
                return false;
            if (ReferenceEquals(entry, _selected))
                return true;
            return _collapse && entry.Type == _selected.Type && entry.Message == _selected.Message && entry.Context == _selected.Context;
        }

        private bool IsVisible(MessageType type)
        {
            for (int i = 0; i < _levels.Length; ++i)
            {
                if (_levels[i].Type == type)
                    return _visible[i];
            }
            return true;
        }

        private static LevelStyle StyleOf(MessageType type)
        {
            foreach (LevelStyle level in _levels)
            {
                if (level.Type == type)
                    return level;
            }
            return _levels[0];
        }

        private static string DetailText(LogEntry entry)
        {
            StringBuilder text = new();
            if (entry.Time.Length > 0)
                text.Append('[').Append(entry.Time).Append("] ");
            text.Append(entry.Type);
            if (entry.Context.Length > 0)
                text.Append(" · ").Append(entry.Context);
            text.Append('\n').Append(entry.Message);
            if (entry.HasStackTrace)
                text.Append("\n\n").Append(entry.StackTrace);
            return text.ToString();
        }

        private static bool FlatButton(string label)
        {
            bool pressed = false;
            return Gui.Toggle(label, ref pressed);
        }
    }
}
