// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.

using System.Text;
using xg;
using xg.editor;
using xg.imui;
using xg.math;

namespace xgEditor.CoreCLR
{
    // Editor Console: engine and script logs from EditorLog. A filter rail on the left (All, one toggle per MessageType
    // with its count, then Collapse, Timestamps and Clear), a search row with a Source filter, the message list (rows
    // tinted and striped by MessageType, with a source chip) and a detail card below a horizontal splitter. Rows are
    // rebuilt from EditorLog.Entries, so the history survives closing the tab and re-creating the panel; only a module
    // reload, which resets EditorLog, starts empty.
    [PanelTool("Console", Icon = "Console", Unique = true, Priority = 20, Tooltip = "Engine and script logs")]
    public sealed class ConsolePanel : Panel, IPanelTool
    {
        private const float RowHeight = 22.0f;
        private const float SplitterHeight = 4.0f;
        private const float MinPaneHeight = 40.0f;
        private const float RailWidth = 34.0f;
        private const float RailCell = 26.0f;
        private const float RailButtonHeight = 24.0f;
        private const float LevelCellHeight = 34.0f;
        private const float CompactCellHeight = 24.0f;
        private const float DetailPadding = 6.0f;
        private const float DetailHeaderHeight = 26.0f;
        private const float StripeWidth = 3.0f;
        private const float SourcePopupWidth = 220.0f;
        private const float SourceRowHeight = 22.0f;
        private const string SourcePopupId = "console_sources";

        private readonly struct LevelStyle
        {
            public LevelStyle(MessageType type, Icon icon, StyleColor color, byte tint)
            {
                Type = type;
                Icon = icon;
                Color = color;
                Tint = tint;
                Id = "##level_" + type;
            }

            public MessageType Type { get; }
            public Icon Icon { get; }
            public StyleColor Color { get; }
            // Row background alpha (0 = untinted).
            public byte Tint { get; }
            public string Id { get; }
        }

        private static readonly LevelStyle[] _levels =
        {
            new(MessageType.Info, Icon.Info, StyleColor.Icon, 0),
            new(MessageType.Success, Icon.Info, StyleColor.AxisY, 36),
            new(MessageType.Warning, Icon.Warning, StyleColor.IconWarning, 36),
            new(MessageType.Critical, Icon.Error, StyleColor.ActiveObject, 41),
            new(MessageType.Error, Icon.Error, StyleColor.IconMaterial, 36),
        };

        private sealed class Row
        {
            public Row(LogEntry entry, string source)
            {
                Entry = entry;
                Source = source;
            }

            public LogEntry Entry;
            public string Source;
            public int Count = 1;
        }

        private readonly List<Row> _rows = new();
        private readonly Dictionary<(MessageType, string, string), Row> _collapsed = new();
        private readonly Dictionary<string, int> _sources = new(StringComparer.Ordinal);
        private readonly List<string> _sourceOrder = new();
        private readonly HashSet<string> _hiddenSources = new(StringComparer.Ordinal);
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
        private string _detailTitle = "No message selected";
        private string _detailMeta = string.Empty;
        private LevelStyle _detailLevel = _levels[0];
        private bool _selectLatestError;
        private bool _openSources;
        private int _openSourcesDelay = -1;
        private bool _actionsInToolbar;
        private Rect _sourceButton;

        public ConsolePanel()
        {
            Array.Fill(_visible, true);
        }

        public override string Title => "Console";

        public int InstanceIndex { get; set; }

        public ToolUsability OnSelectItems(object[] selection) => ToolUsability.Usable;
        public override string? MenuPath => "General/Console";
        public override DockSlot Slot => Environment.GetEnvironmentVariable("XG_UI_TEST_CONSOLE_DOCK")?.ToLowerInvariant() switch
        {
            "center" => DockSlot.Center,
            "right" => DockSlot.Right,
            "left" => DockSlot.Left,
            _ => DockSlot.Bottom,
        };
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

        // Test hooks: collapse identical messages, hide a level, open the Source filter.
        public void SetCollapse(bool collapse)
        {
            _collapse = collapse;
            _dirty = true;
        }

        public void SetLevelVisible(MessageType type, bool visible)
        {
            for (int i = 0; i < _levels.Length; ++i)
                if (_levels[i].Type == type)
                    _visible[i] = visible;
            _dirty = true;
        }

        public void OpenSourceFilter(int delayFrames = 30) => _openSourcesDelay = Math.Max(delayFrames, 0);

        protected override void OnGUI()
        {
            EditorLog.Drain();
            bool grew = EditorLog.Version != _seenVersion;
            if (grew || _dirty)
                Rebuild();

            using (Gui.Horizontal(0.0f))
            {
                DrawRail();
                using (Gui.Vertical())
                {
                    DrawSearchRow();
                    Vector2 avail = Gui.AvailableSize;
                    float maxDetail = Math.Max(MinPaneHeight, avail.y - MinPaneHeight - SplitterHeight);
                    _detailHeight = Math.Clamp(_detailHeight, MinPaneHeight, maxDetail);
                    DrawList(grew);
                    Gui.Splitter("##console_split", ref _detailHeight, MinPaneHeight, maxDetail);
                    DrawDetail();
                }
            }
            DrawSourcePopup();
        }

        // ------------------------------------------------------------
        // Rail
        // ------------------------------------------------------------
        private void DrawRail()
        {
            using var canvas = Gui.Canvas("##console_rail", new Vector2(RailWidth, 0.0f));
            Painter draw = canvas.Draw;
            Vector2 size = canvas.Size;
            draw.RectFilled(new Rect(new Vector2(0.0f, 0.0f), size), Style.Color(StyleColor.TopBarBg));
            float x = (RailWidth - RailCell) * 0.5f;
            float y = 6.0f;

            float top = 6.0f + RailButtonHeight + 9.0f;
            float actions = 3.0f * (RailButtonHeight + 3.0f) + 8.0f;
            float levels = _levels.Length * (LevelCellHeight + 2.0f);
            _actionsInToolbar = top + levels + actions > size.y;
            bool compact = top + levels > size.y;
            float cellHeight = compact ? CompactCellHeight : LevelCellHeight;

            bool all = _visible.All(v => v);
            if (RailButton(draw, "##all", Rect.FromXYWH(x, y, RailCell, RailButtonHeight), Icon.Cards, Style.Color(StyleColor.Icon), all,
                all ? "Showing every level" : "Show every level") && !all)
            {
                Array.Fill(_visible, true);
                _dirty = true;
            }
            y += RailButtonHeight + 4.0f;
            draw.Line(new Vector2(7.0f, y), new Vector2(RailWidth - 7.0f, y), Style.Color(StyleColor.Border));
            y += 5.0f;

            for (int i = 0; i < _levels.Length; ++i)
            {
                LevelStyle level = _levels[i];
                int count = EditorLog.CountOf(level.Type);
                if (LevelCell(draw, level, Rect.FromXYWH(x, y, RailCell, cellHeight), count, _visible[i], !compact))
                {
                    _visible[i] = !_visible[i];
                    _dirty = true;
                }
                y += cellHeight + 2.0f;
            }
            if (_actionsInToolbar)
                return;

            float bottom = size.y - 6.0f - RailButtonHeight;
            Color32 icon = Style.Color(StyleColor.Icon);
            if (RailButton(draw, "##clear", Rect.FromXYWH(x, bottom, RailCell, RailButtonHeight), Icon.Trash, icon, false, "Clear the Console"))
            {
                EditorLog.Clear();
                Select(null);
            }
            bottom -= RailButtonHeight + 3.0f;
            if (RailButton(draw, "##timestamps", Rect.FromXYWH(x, bottom, RailCell, RailButtonHeight), Icon.Clock, icon, _timestamps,
                _timestamps ? "Timestamps shown" : "Show the time of each message"))
                _timestamps = !_timestamps;
            bottom -= RailButtonHeight + 3.0f;
            if (RailButton(draw, "##collapse", Rect.FromXYWH(x, bottom, RailCell, RailButtonHeight), Icon.Stack, icon, _collapse,
                _collapse ? "Identical messages collapsed" : "Collapse identical messages"))
            {
                _collapse = !_collapse;
                _dirty = true;
            }
        }

        private static bool RailButton(Painter draw, string id, Rect rect, Icon icon, Color32 color, bool on, string tooltip)
        {
            Result hit = Gui.HitArea(id, rect);
            Gui.Tooltip(tooltip);
            if (on)
                draw.RectFilled(rect, Style.Color(hit.Hovered() ? StyleColor.AccentHover : StyleColor.Accent), 4.0f);
            else if (hit.Hovered())
                draw.RectFilled(rect, Style.Color(StyleColor.ButtonHover), 4.0f);
            Vector2 center = new(rect.min.x + rect.Width * 0.5f, rect.min.y + rect.Height * 0.5f);
            draw.Icon(icon, center, 15.0f, on ? Style.Color(StyleColor.TextOnAccent) : color);
            return hit.Clicked();
        }

        private static bool LevelCell(Painter draw, LevelStyle level, Rect rect, int count, bool visible, bool showCount)
        {
            Result hit = Gui.HitArea(level.Id, rect);
            Gui.Tooltip(visible ? $"{level.Type}: {count} (click to hide)" : $"{level.Type}: {count}, hidden (click to show)");
            if (visible)
                draw.RectFilled(rect, Style.Color(hit.Hovered() ? StyleColor.ButtonHover : StyleColor.HeaderBg), 4.0f);
            else if (hit.Hovered())
                draw.RectFilled(rect, Style.Color(StyleColor.SegmentBg), 4.0f);
            Color32 color = Style.Color(visible ? level.Color : StyleColor.TextDim);
            if (!visible)
                color = WithAlpha(color, 110);
            float cx = rect.min.x + rect.Width * 0.5f;
            draw.Icon(level.Icon, new Vector2(cx, showCount ? rect.min.y + 11.0f : rect.min.y + rect.Height * 0.5f), 14.0f, color);
            if (!showCount)
                return hit.Clicked();
            string text = count > 999 ? "999+" : count.ToString();
            float textWidth = Gui.MeasureText(text).x;
            draw.Text(new Vector2(MathF.Round(cx - textWidth * 0.5f), rect.min.y + 18.0f), visible ? Style.Color(StyleColor.TextDim) : color, text);
            return hit.Clicked();
        }

        // ------------------------------------------------------------
        // Search row and Source filter
        // ------------------------------------------------------------
        private void DrawSearchRow()
        {
            using (Gui.Toolbar())
            {
                if (_actionsInToolbar)
                    DrawToolbarActions();
                if (Gui.SearchField("##console_search", _search, 0.0f))
                    _dirty = true;
                string label = _hiddenSources.Count == 0 ? "Source: all" : $"Source: {_sourceOrder.Count - _hiddenSources.Count(s => _sources.ContainsKey(s))}/{_sourceOrder.Count}";
                if (Gui.Button(label))
                    _openSources = true;
                _sourceButton = Gui.ItemRect;
                Gui.Tooltip("Show messages from some sources only");
            }
        }

        // Collapse, Timestamps and Clear in the search row, when the rail is too short for them.
        private void DrawToolbarActions()
        {
            Color32 icon = Style.Color(StyleColor.Icon);
            Color32 on = Style.Color(StyleColor.Accent);
            bool collapse = _collapse;
            if (Gui.IconToggle("##collapse_bar", Icon.Stack, ref collapse, collapse ? on : icon))
            {
                _collapse = collapse;
                _dirty = true;
            }
            Gui.Tooltip("Collapse identical messages");
            Gui.IconToggle("##timestamps_bar", Icon.Clock, ref _timestamps, _timestamps ? on : icon);
            Gui.Tooltip("Show the time of each message");
            if (Gui.IconButton("##clear_bar", Icon.Trash, 16.0f, icon))
            {
                EditorLog.Clear();
                Select(null);
            }
            Gui.Tooltip("Clear the Console");
        }

        private void DrawSourcePopup()
        {
            if (_openSourcesDelay >= 0 && _openSourcesDelay-- == 0)
                _openSources = true;
            if (_openSources)
            {
                _openSources = false;
                Gui.OpenPopup(SourcePopupId);
            }
            Vector2 position = new(_sourceButton.max.x - SourcePopupWidth, _sourceButton.max.y + 2.0f);
            using var popup = Gui.Popup(SourcePopupId, position, new Vector2(SourcePopupWidth, 0.0f));
            if (!popup.IsOpen)
                return;
            float width = SourcePopupWidth - 8.0f;
            int rows = _sourceOrder.Count + 1;
            using var canvas = Gui.Canvas("##console_source_rows", new Vector2(width, rows * (SourceRowHeight + 2.0f) + 4.0f));
            Painter draw = canvas.Draw;
            float y = 2.0f;
            bool all = _hiddenSources.Count == 0;
            if (ToggleButton.Draw(draw, "##source_all", Rect.FromXYWH(4.0f, y, width - 8.0f, SourceRowHeight), Icon.None, null, "All sources",
                _sources.Values.Sum().ToString(), null, all, true, ToggleDot.Check).Clicked() && !all)
            {
                _hiddenSources.Clear();
                _dirty = true;
            }
            y += SourceRowHeight + 2.0f;
            for (int i = 0; i < _sourceOrder.Count; ++i)
            {
                string source = _sourceOrder[i];
                bool on = !_hiddenSources.Contains(source);
                string label = source.Length == 0 ? "No source" : source;
                if (ToggleButton.Draw(draw, "##source" + i, Rect.FromXYWH(4.0f, y, width - 8.0f, SourceRowHeight), Icon.None, null, label,
                    _sources[source].ToString(), null, on, true, ToggleDot.Check).Clicked())
                {
                    if (on)
                        _hiddenSources.Add(source);
                    else
                        _hiddenSources.Remove(source);
                    _dirty = true;
                }
                y += SourceRowHeight + 2.0f;
            }
        }

        // ------------------------------------------------------------
        // List
        // ------------------------------------------------------------
        private void DrawList(bool grew)
        {
            bool scrollToBottom = _follow && grew;
            _list.Count = _rows.Count;
            _list.RowHeight = RowHeight;
            _list.Flags = ListFlags.NoStripes | (scrollToBottom ? ListFlags.ScrollToBottom : ListFlags.None);

            using ListScope list = Gui.List("##console_list", ref _list);
            if (!list.IsOpen)
                return;
            if (!scrollToBottom)
                _follow = _list.AtBottom;

            if (_timeWidth < 0.0f)
            {
                using (Gui.Font(FontStyle.Mono))
                    _timeWidth = Gui.MeasureText("00:00:00.000").x + 10.0f;
            }

            Color32 text = Style.Color(StyleColor.Text);
            Color32 textSelected = Style.Color(StyleColor.TextSelected);
            Color32 textDim = Style.Color(StyleColor.TextDim);
            Color32 chip = WithAlpha(new Color32(255, 255, 255), 18);
            Color32 separator = Style.Color(StyleColor.SegmentBg);
            Color32 accent = Style.Color(StyleColor.Accent);
            float rowWidth = Math.Max(Gui.AvailableSize.x, 1.0f);
            float textY = (RowHeight - Gui.MeasureText("A").y) * 0.5f;

            for (int i = _list.First; i < _list.Last && i < _rows.Count; ++i)
            {
                Row row = _rows[i];
                LogEntry entry = row.Entry;
                bool selected = IsSelected(entry);
                using ListRowScope scope = Gui.ListRow(i, false);
                if (scope.Clicked)
                    Select(entry);

                Painter draw = scope.Draw;
                LevelStyle level = StyleOf(entry.Type);
                Color32 levelColor = Style.Color(level.Color);
                Rect full = Rect.FromXYWH(0.0f, 0.0f, rowWidth, RowHeight);
                byte tint = selected ? (byte)Math.Min(level.Tint + 16, 255) : level.Tint;
                if (tint > 0)
                    draw.RectFilled(full, WithAlpha(levelColor, tint));
                else if (selected)
                    draw.RectFilled(full, WithAlpha(Style.Color(StyleColor.Selection), 150));
                draw.Line(new Vector2(0.0f, RowHeight - 0.5f), new Vector2(full.max.x, RowHeight - 0.5f), separator);
                draw.RectFilled(Rect.FromXYWH(0.0f, 0.0f, StripeWidth, RowHeight), entry.Type == MessageType.Info ? Style.Color(StyleColor.Grid) : levelColor);
                if (selected)
                    draw.Rect(Rect.FromXYWH(0.5f, 0.5f, rowWidth - 1.0f, RowHeight - 1.0f), accent);

                float x = StripeWidth + 8.0f;
                if (_timestamps && entry.Time.Length > 0)
                {
                    using (Gui.Font(FontStyle.Mono))
                        draw.Text(new Vector2(x, textY), textDim, entry.Time);
                    x += _timeWidth;
                }
                float right = rowWidth - 6.0f;
                if (row.Source.Length > 0)
                    right = DrawChip(draw, right, row.Source, chip, selected ? textSelected : textDim, textY) - 4.0f;
                if (_collapse && row.Count > 1)
                    right = DrawChip(draw, right, "×" + row.Count, chip, selected ? textSelected : text, textY) - 4.0f;
                draw.Text(new Vector2(x, textY), selected ? textSelected : text, Fit(entry.FirstLine, right - x - 4.0f));
            }
        }

        // Draws a pill ending at right; returns its left edge.
        private static float DrawChip(Painter draw, float right, string label, Color32 fill, Color32 color, float textY)
        {
            float width = Gui.MeasureText(label).x + 14.0f;
            Rect pill = Rect.FromXYWH(right - width, 3.0f, width, RowHeight - 6.0f);
            draw.RectFilled(pill, fill, 8.0f);
            draw.Text(new Vector2(pill.min.x + 7.0f, textY), color, label);
            return pill.min.x;
        }

        private static Color32 WithAlpha(Color32 color, byte alpha) => new(color.r, color.g, color.b, alpha);

        // The text cut to fit maxWidth, ending with an ellipsis when cut.
        private static string Fit(string text, float maxWidth)
        {
            if (maxWidth <= 0.0f)
                return string.Empty;
            if (Gui.MeasureText(text).x <= maxWidth)
                return text;
            int low = 0;
            int high = text.Length;
            while (low < high)
            {
                int mid = (low + high + 1) / 2;
                if (Gui.MeasureText(string.Concat(text.AsSpan(0, mid), "...")).x <= maxWidth)
                    low = mid;
                else
                    high = mid - 1;
            }
            return low == 0 ? string.Empty : string.Concat(text.AsSpan(0, low), "...");
        }

        // ------------------------------------------------------------
        // Detail
        // ------------------------------------------------------------
        private void DrawDetail()
        {
            using var area = Gui.ScrollArea("##console_detail_area", new Vector2(0.0f, _detailHeight), ScrollAreaFlags.Padded);
            if (!area.IsOpen)
                return;
            Vector2 avail = Gui.AvailableSize;
            Gui.Draw.RectFilled(new Rect(new Vector2(-DetailPadding, -DetailPadding), new Vector2(avail.x + DetailPadding, avail.y + DetailPadding)),
                Style.Color(StyleColor.ToolbarBg));

            float width = Math.Max(avail.x, 60.0f);
            using (var header = Gui.Canvas("##console_detail_header", new Vector2(width, DetailHeaderHeight)))
            {
                Painter draw = header.Draw;
                draw.RectFilled(Rect.FromXYWH(0.0f, 0.0f, width, DetailHeaderHeight + 4.0f), Style.Color(StyleColor.PanelHeaderBg), 4.0f);
                draw.Icon(_detailLevel.Icon, new Vector2(14.0f, DetailHeaderHeight * 0.5f), 14.0f, Style.Color(_detailLevel.Color));
                float textY = (DetailHeaderHeight - Gui.MeasureText("A").y) * 0.5f;
                float x = 28.0f;
                using (Gui.Font(FontStyle.Bold))
                {
                    draw.Text(new Vector2(x, textY), Style.Color(StyleColor.Text), _detailTitle);
                    x += Gui.MeasureText(_detailTitle).x + 6.0f;
                }
                draw.Text(new Vector2(x, textY), Style.Color(StyleColor.TextDim), _detailMeta);
                if (_selected is not null)
                {
                    if (Gui.IconButton("##copy", Icon.File, new Vector2(width - 22.0f, (DetailHeaderHeight - 18.0f) * 0.5f), 18.0f, Style.Color(StyleColor.Icon)))
                        Gui.SetClipboard(_detail);
                    Gui.Tooltip("Copy the message and stack trace");
                }
            }

            float bodyHeight = Math.Max(_detailHeight - DetailPadding * 2.0f - DetailHeaderHeight, 20.0f);
            using (Gui.Font(FontStyle.Mono))
                Gui.TextArea("##console_detail", _detail, new Vector2(0.0f, bodyHeight));
        }

        // ------------------------------------------------------------
        // Rows
        // ------------------------------------------------------------
        private void Rebuild()
        {
            _seenVersion = EditorLog.Version;
            _dirty = false;
            _rows.Clear();
            _collapsed.Clear();
            _sources.Clear();
            _sourceOrder.Clear();

            string search = _search.Text;
            IReadOnlyList<LogEntry> entries = EditorLog.Entries;
            LogEntry? latestError = null;
            for (int i = 0; i < entries.Count; ++i)
            {
                LogEntry entry = entries[i];
                if (entry.Type is MessageType.Error or MessageType.Critical)
                    latestError = entry;
                string source = SourceOf(entry);
                if (_sources.TryGetValue(source, out int seen))
                {
                    _sources[source] = seen + 1;
                }
                else
                {
                    _sources[source] = 1;
                    _sourceOrder.Add(source);
                }
                if (!IsVisible(entry.Type) || _hiddenSources.Contains(source))
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
                    Row row = new(entry, source);
                    _collapsed.Add(key, row);
                    _rows.Add(row);
                }
                else
                {
                    _rows.Add(new Row(entry, source));
                }
            }
            _sourceOrder.Sort((a, b) => a.Length == 0 ? 1 : b.Length == 0 ? -1 : string.Compare(a, b, StringComparison.OrdinalIgnoreCase));

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

        // The actor context, else a leading "[Tag]" of the message, else none.
        private static string SourceOf(LogEntry entry)
        {
            if (entry.Context.Length > 0)
                return entry.Context;
            string message = entry.Message;
            if (message.Length > 2 && message[0] == '[')
            {
                int end = message.IndexOf(']', 1);
                if (end > 1 && end <= 32)
                    return message[1..end];
            }
            return string.Empty;
        }

        private void Select(LogEntry? entry)
        {
            _selected = entry;
            _detail = entry is null ? string.Empty : DetailText(entry);
            _detailTitle = entry is null ? "No message selected" : entry.Type.ToString();
            _detailMeta = entry is null ? string.Empty : DetailMeta(entry);
            _detailLevel = entry is null ? _levels[0] : StyleOf(entry.Type);
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

        internal static void LevelVisual(MessageType type, out Icon icon, out StyleColor color)
        {
            LevelStyle level = StyleOf(type);
            icon = level.Icon;
            color = level.Color;
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

        // Header after the type: source and time.
        private static string DetailMeta(LogEntry entry)
        {
            StringBuilder meta = new();
            string source = SourceOf(entry);
            if (source.Length > 0)
                meta.Append("· ").Append(source).Append(' ');
            if (entry.Time.Length > 0)
                meta.Append("· ").Append(entry.Time);
            return meta.ToString();
        }

        // Body: the message, then the stack trace.
        private static string DetailText(LogEntry entry)
        {
            StringBuilder text = new();
            text.Append(entry.Message);
            if (entry.HasStackTrace)
                text.Append("\n\n").Append(entry.StackTrace);
            return text.ToString();
        }
    }
}
