// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.

using xg.math;

namespace xg.imui
{
    // Side of the host panel's dock the ribbon is on. Auto follows the panel's dock placement.
    public enum RibbonSide
    {
        Auto = 0,
        Left,
        Right,
        Top,
        Bottom,
    }

    public enum RibbonItemStyle
    {
        // Full-width row (vertical) or column entry (horizontal): icon and label.
        Wide = 0,
        // Square icon-only cell; consecutive icon items pack into a grid.
        Icon,
    }

    public sealed class RibbonItem
    {
        public string Label { get; set; } = "";
        public string? Tooltip { get; set; }
        public Icon Icon { get; set; }
        public Color32? IconColor { get; set; }
        // Custom glyph drawn instead of Icon: (painter, icon rect, color).
        public Action<Painter, Rect, Color32>? DrawIcon { get; set; }
        public RibbonItemStyle Style { get; set; }
        public Func<bool>? IsEnabled { get; set; }
        public Func<bool>? IsToggled { get; set; }
        // Hidden items take no space; null = always visible.
        public Func<bool>? IsVisible { get; set; }
        // Free-form immediate GUI drawn in place of a button, spanning the card width (layout calls, a column). Its height
        // is the content height measured on the previous frame, so it settles one frame after it appears or grows.
        public Action? OnGUI { get; set; }
        // Shows a small dot in the corner: the item has options.
        public bool HasOptions { get; set; }
        // Corner marker drawn by ToggleButton.Dot: Check for a toggle, Radio for one choice in a group. Wins over
        // HasOptions.
        public ToggleDot Dot { get; set; }
        public Action? Clicked { get; set; }

        internal float GuiHeight { get; set; } = 22.0f;
        internal bool IsCell => OnGUI == null && Style == RibbonItemStyle.Icon;
    }

    public sealed class RibbonGroup
    {
        public string Name { get; set; } = "";
        public string? Tooltip { get; set; }
        public bool Collapsed { get; set; }
        // Cells per row for runs of icon items; the subgroup keeps this shape in every layout.
        public int IconColumns { get; set; } = 4;
        // Card width when the ribbon is horizontal (docked top or bottom).
        public float Width { get; set; } = 150.0f;
        public List<RibbonItem> Items { get; } = new();

        internal List<RibbonItem> Visible { get; } = new();

        internal void RefreshVisible()
        {
            Visible.Clear();
            foreach (RibbonItem item in Items)
                if (item.IsVisible?.Invoke() ?? true)
                    Visible.Add(item);
        }
    }

    public sealed class RibbonCategory
    {
        public string Name { get; set; } = "";
        public string? Tooltip { get; set; }
        public Icon Icon { get; set; }
        public Color32? IconColor { get; set; }
        public List<RibbonGroup> Groups { get; } = new();
    }

    // Tool palette with category tabs and groups of commands. Call OnGUI from a panel's OnGUI (best with
    // PanelFlags.NoPadding | NoScrollbar). Docked left or right it lays out vertically with the tabs on the side away
    // from the dock edge; docked top or bottom (or wide while floating) the tabs go on top and the group cards sit side
    // by side at their own widths.
    public sealed class Ribbon
    {
        private const float StripSize = 26.0f;
        private const float TabGap = 2.0f;
        private const float TabIconSize = 13.0f;
        private const float AreaPadding = 6.0f;
        private const float CardGap = 6.0f;
        private const float CardRounding = 4.0f;
        private const float HeaderHeight = 22.0f;
        private const float Inset = 4.0f;
        private const float RowGap = 3.0f;
        private const float RowHeight = 22.0f;
        private const float ButtonRounding = 3.0f;
        private const float SegmentPadding = 2.0f;
        private const float CellGap = 2.0f;
        private const float CellHeight = 24.0f;
        private const float MoreHeight = 18.0f;
        private const string OverflowPopupId = "ribbon_more";

        private enum PlacedKind { Card, Header, Segment, Wide, Cell, More, Gui }

        private struct Placed
        {
            public PlacedKind Kind;
            public Rect Rect;
            public RibbonGroup Group;
            public RibbonItem? Item;
            public int Index;
        }

        private readonly List<Placed> _placed = new();
        private readonly List<Placed> _popupPlaced = new();
        private RibbonGroup? _overflowGroup;
        private int _overflowStart;
        private Rect _overflowButton;
        private bool _openOverflow;
        private string? _requestedOverflow;
        private int _requestDelay;
        private RibbonSide _resolved = RibbonSide.Left;
        private RibbonSide _pending = RibbonSide.Left;
        private int _pendingFrames;
        private bool _hasSide;
        private float _lineHeight = 16.0f;

        public List<RibbonCategory> Categories { get; } = new();
        public int SelectedIndex { get; set; }
        public RibbonSide Side { get; set; } = RibbonSide.Auto;
        // The side in effect this frame (never Auto).
        public RibbonSide ResolvedSide => _resolved;
        public bool IsVertical => _resolved is RibbonSide.Left or RibbonSide.Right;
        public RibbonCategory? SelectedCategory =>
            Categories.Count == 0 ? null : Categories[Math.Clamp(SelectedIndex, 0, Categories.Count - 1)];

        // Removes every category, keeping the side and the selected tab index (for hosts that rebuild their model).
        public void Clear()
        {
            Categories.Clear();
            _placed.Clear();
            _popupPlaced.Clear();
            _overflowGroup = null;
        }

        public void OnGUI()
        {
            if (_requestDelay > 0)
                --_requestDelay;
            ResolveSide();
            _lineHeight = Gui.MeasureText("Ag").y;
            if (IsVertical)
            {
                using (Gui.Horizontal(0.0f))
                {
                    if (_resolved == RibbonSide.Right)
                        DrawTabs(false);
                    DrawContent(true);
                    if (_resolved == RibbonSide.Left)
                        DrawTabs(true);
                }
            }
            else
            {
                DrawHorizontalTabs();
                DrawContent(false);
            }
        }

        private void ResolveSide()
        {
            if (Side != RibbonSide.Auto)
            {
                _resolved = Side;
                _hasSide = true;
                return;
            }
            PanelPlacement placement = Gui.PanelPlacement;
            if (placement.Window.Width <= 0.0f || placement.Window.Height <= 0.0f)
                return;
            RibbonSide detected = (RibbonSide)((int)TabStrip.Detect(placement) + 1);
            if (!_hasSide)
            {
                _resolved = detected;
                _hasSide = true;
                return;
            }
            if (detected == _resolved)
            {
                _pendingFrames = 0;
                return;
            }
            if (detected == _pending)
                ++_pendingFrames;
            else
            {
                _pending = detected;
                _pendingFrames = 1;
            }
            if (_pendingFrames >= 2)
            {
                _resolved = detected;
                _pendingFrames = 0;
            }
        }

        // ------------------------------------------------------------
        // Tabs
        // ------------------------------------------------------------
        private float TabLabelLength(RibbonCategory category) =>
            7.0f + TabIconSize + 5.0f + Gui.MeasureText(category.Name).x + 10.0f;

        private bool UseIconOnlyTabs(float available)
        {
            float total = TabGap;
            for (int i = 0; i < Categories.Count; i++)
                total += TabLabelLength(Categories[i]) + TabGap;
            return total > available;
        }


        // Vertical strip. onRight: the strip is right of the content (its outer edge is the right one).
        private void DrawTabs(bool onRight)
        {
            using var canvas = Gui.Canvas("##ribbon_tabs", new Vector2(StripSize, 0.0f));
            Painter draw = canvas.Draw;
            Vector2 size = canvas.Size;
            draw.RectFilled(new Rect(new Vector2(0.0f, 0.0f), size), Style.Color(StyleColor.TopBarBg));

            bool iconOnly = UseIconOnlyTabs(size.y);
            float bodyLeft = onRight ? 0.0f : TabStrip.AccentWidth;
            float bodyWidth = StripSize - TabStrip.AccentWidth;
            float centerX = bodyLeft + bodyWidth * 0.5f;
            float y = TabGap;
            for (int i = 0; i < Categories.Count; i++)
            {
                RibbonCategory category = Categories[i];
                float length = iconOnly ? StripSize : TabLabelLength(category);
                Rect tab = Rect.FromXYWH(0.0f, y, StripSize, length);
                Result hit;
                using (Gui.Id(i))
                    hit = Gui.HitArea("##tab", tab);
                TabTooltip(category, iconOnly);
                if (hit.Clicked())
                    SelectedIndex = i;

                bool selected = i == SelectedIndex;
                TabStrip.DrawShape(draw, tab, onRight ? TabEdge.Right : TabEdge.Left, selected, hit.Hovered());

                Color32 iconColor = category.IconColor ?? Style.Color(StyleColor.Icon);
                if (iconOnly)
                {
                    draw.Icon(category.Icon, new Vector2(centerX, y + length * 0.5f), TabIconSize, iconColor);
                }
                else
                {
                    draw.Icon(category.Icon, new Vector2(centerX, y + 7.0f + TabIconSize * 0.5f), TabIconSize, iconColor);
                    Color32 textColor = Style.Color(selected || hit.Hovered() ? StyleColor.TextSelected : StyleColor.Text);
                    Vector2 textPos = new(MathF.Round(bodyLeft + (bodyWidth - _lineHeight) * 0.5f), y + 7.0f + TabIconSize + 5.0f);
                    draw.TextRotated(textPos, textColor, category.Name, onRight ? 1 : 3);
                }
                y += length + TabGap;
            }
        }

        private static void TabTooltip(RibbonCategory category, bool iconOnly)
        {
            string? tip = iconOnly
                ? (string.IsNullOrEmpty(category.Tooltip) ? category.Name : $"{category.Name}\n{category.Tooltip}")
                : category.Tooltip;
            if (!string.IsNullOrEmpty(tip))
                Gui.Tooltip(tip);
        }

        private void DrawHorizontalTabs()
        {
            using var canvas = Gui.Canvas("##ribbon_tabs", new Vector2(0.0f, StripSize));
            Painter draw = canvas.Draw;
            Vector2 size = canvas.Size;
            draw.RectFilled(new Rect(new Vector2(0.0f, 0.0f), size), Style.Color(StyleColor.TopBarBg));

            bool iconOnly = UseIconOnlyTabs(size.x);
            float x = TabGap + 1.0f;
            const float Top = 3.0f;
            float height = StripSize - Top;
            float cy = Top + TabStrip.AccentWidth + (height - TabStrip.AccentWidth) * 0.5f;
            for (int i = 0; i < Categories.Count; i++)
            {
                RibbonCategory category = Categories[i];
                float length = iconOnly ? height : TabLabelLength(category);
                Rect tab = Rect.FromXYWH(x, Top, length, height);
                Result hit;
                using (Gui.Id(i))
                    hit = Gui.HitArea("##tab", tab);
                TabTooltip(category, iconOnly);
                if (hit.Clicked())
                    SelectedIndex = i;

                bool selected = i == SelectedIndex;
                TabStrip.DrawShape(draw, tab, TabEdge.Top, selected, hit.Hovered());

                Color32 iconColor = category.IconColor ?? Style.Color(StyleColor.Icon);
                if (iconOnly)
                {
                    draw.Icon(category.Icon, new Vector2(x + length * 0.5f, cy), TabIconSize, iconColor);
                }
                else
                {
                    draw.Icon(category.Icon, new Vector2(x + 10.0f + TabIconSize * 0.5f, cy), TabIconSize, iconColor);
                    Color32 textColor = Style.Color(selected || hit.Hovered() ? StyleColor.TextSelected : StyleColor.Text);
                    draw.Text(new Vector2(x + 10.0f + TabIconSize + 5.0f, MathF.Round(cy - _lineHeight * 0.5f)), textColor, category.Name);
                }
                x += length + TabGap;
            }
        }

        // ------------------------------------------------------------
        // Content
        // ------------------------------------------------------------
        private void DrawContent(bool vertical)
        {
            using var area = Gui.ScrollArea("##ribbon_content", default, !vertical);
            if (!area.IsOpen)
                return;

            Vector2 avail = Gui.AvailableSize;
            RibbonCategory? category = SelectedCategory;
            Vector2 extent = default;
            _placed.Clear();
            if (category != null)
            {
                foreach (RibbonGroup group in category.Groups)
                    group.RefreshVisible();
                extent = vertical ? LayoutVertical(category, avail.x) : LayoutHorizontal(category, avail.y);
            }
            Vector2 canvasSize = new(MathF.Max(extent.x, MathF.Max(avail.x, 1.0f)), MathF.Max(extent.y, MathF.Max(avail.y, 1.0f)));
            using (var canvas = Gui.Canvas("##ribbon_items", canvasSize))
            {
                Painter draw = canvas.Draw;
                draw.RectFilled(new Rect(new Vector2(0.0f, 0.0f), canvas.Size), Style.Color(StyleColor.ToolbarBg));
                for (int i = 0; i < _placed.Count; i++)
                {
                    Placed placed = _placed[i];
                    using (Gui.Id(i))
                    {
                        switch (placed.Kind)
                        {
                            case PlacedKind.Card:
                                draw.RectFilled(placed.Rect, Style.Color(StyleColor.HeaderBg), CardRounding);
                                draw.Rect(placed.Rect, Style.Color(StyleColor.Border), CardRounding);
                                break;
                            case PlacedKind.Header:
                                DrawHeader(draw, placed);
                                break;
                            case PlacedKind.Segment:
                                draw.RectFilled(placed.Rect, Style.Color(StyleColor.SegmentBg), CardRounding);
                                break;
                            case PlacedKind.More:
                                DrawMore(draw, placed);
                                break;
                            case PlacedKind.Gui:
                                DrawGuiItem(placed);
                                break;
                            default:
                                DrawItem(draw, placed);
                                break;
                        }
                    }
                }
            }

            if (vertical)
                _overflowGroup = null;
            else
                DrawOverflowPopup();
        }

        private Vector2 LayoutVertical(RibbonCategory category, float width)
        {
            float cardWidth = MathF.Max(width - AreaPadding * 2.0f, 40.0f);
            float y = AreaPadding;
            foreach (RibbonGroup group in category.Groups)
                if (group.Visible.Count > 0)
                    y += LayoutGroup(group, AreaPadding, y, cardWidth, 0.0f) + CardGap;
            return new Vector2(width, y - CardGap + AreaPadding);
        }

        // Cards side by side, each RibbonGroup.Width wide and the full height of the area; items that don't fit go to the
        // group's overflow flyout.
        private Vector2 LayoutHorizontal(RibbonCategory category, float height)
        {
            float cardHeight = MathF.Max(height - AreaPadding * 2.0f, HeaderHeight + 2.0f);
            float x = AreaPadding;
            foreach (RibbonGroup group in category.Groups)
            {
                if (group.Visible.Count == 0)
                    continue;
                float width = MathF.Max(group.Width, 60.0f);
                LayoutGroup(group, x, AreaPadding, width, cardHeight);
                x += width + CardGap;
            }
            return new Vector2(x - CardGap + AreaPadding, height);
        }

        // Places one group card at (x, top) and returns its height. fixedHeight > 0 keeps the card at that height and puts
        // a "+N more" button at its bottom when the items don't fit.
        private float LayoutGroup(RibbonGroup group, float x, float top, float width, float fixedHeight)
        {
            int cardIndex = _placed.Count;
            _placed.Add(new Placed { Kind = PlacedKind.Card, Group = group });
            _placed.Add(new Placed { Kind = PlacedKind.Header, Rect = Rect.FromXYWH(x + 1.0f, top + 1.0f, width - 2.0f, HeaderHeight), Group = group });
            float y = top + 1.0f + HeaderHeight + 1.0f;
            if (!group.Collapsed)
            {
                float maxBottom = fixedHeight > 0.0f ? top + fixedHeight - 1.0f - Inset : float.MaxValue;
                int overflow = LayoutItems(_placed, group, 0, x + Inset, y, width - Inset * 2.0f, maxBottom, out y);
                if (overflow < group.Visible.Count)
                {
                    Rect more = Rect.FromXYWH(x + Inset, maxBottom - MoreHeight, width - Inset * 2.0f, MoreHeight);
                    _placed.Add(new Placed { Kind = PlacedKind.More, Rect = more, Group = group, Index = overflow });
                    if (_requestedOverflow == group.Name && _requestDelay == 0)
                    {
                        _requestedOverflow = null;
                        OpenOverflow(group, overflow, more);
                    }
                }
                y += Inset;
            }
            float height = fixedHeight > 0.0f ? fixedHeight : y - top + 1.0f;
            Placed card = _placed[cardIndex];
            card.Rect = Rect.FromXYWH(x, top, width, height);
            _placed[cardIndex] = card;
            return height;
        }

        // Height of the element starting at visible item index (a wide row, a GUI entry, or a whole run of icon items) and
        // the index after it.
        private static float ElementHeight(RibbonGroup group, int index, out int end)
        {
            end = index + 1;
            RibbonItem first = group.Visible[index];
            if (first.OnGUI != null)
                return RowGap + MathF.Max(first.GuiHeight, 4.0f);
            if (!first.IsCell)
                return RowGap + RowHeight;
            while (end < group.Visible.Count && group.Visible[end].IsCell)
                ++end;
            return Inset + SegmentHeight(group, end - index);
        }

        private static float SegmentHeight(RibbonGroup group, int count)
        {
            int columns = Math.Max(group.IconColumns, 1);
            int rows = (count + columns - 1) / columns;
            return SegmentPadding * 2.0f + rows * CellHeight + (rows - 1) * CellGap;
        }

        // Places wide rows and icon segments from item start down to maxBottom (segments never split). Leaves room for the
        // more button unless everything left fits. Returns the first item that wasn't placed.
        private int LayoutItems(List<Placed> target, RibbonGroup group, int start, float x, float y, float width, float maxBottom, out float bottom)
        {
            float remaining = 0.0f;
            for (int i = start; i < group.Visible.Count;)
                remaining += ElementHeight(group, i, out i);
            float limit = y + remaining <= maxBottom ? maxBottom : maxBottom - MoreHeight - RowGap;

            int index = start;
            while (index < group.Visible.Count)
            {
                float height = ElementHeight(group, index, out int end);
                if (y + height > limit)
                    break;
                RibbonItem item = group.Visible[index];
                if (item.OnGUI != null)
                    target.Add(new Placed { Kind = PlacedKind.Gui, Rect = Rect.FromXYWH(x, y + RowGap, width, height - RowGap), Group = group, Item = item });
                else if (!item.IsCell)
                    target.Add(new Placed { Kind = PlacedKind.Wide, Rect = Rect.FromXYWH(x, y + RowGap, width, RowHeight), Group = group, Item = item });
                else
                    LayoutSegment(target, group, index, end, x, y + Inset, width);
                y += height;
                index = end;
            }
            bottom = y;
            return index;
        }

        private static void LayoutSegment(List<Placed> target, RibbonGroup group, int begin, int end, float x, float y, float width)
        {
            int columns = Math.Max(group.IconColumns, 1);
            int count = end - begin;
            target.Add(new Placed { Kind = PlacedKind.Segment, Rect = Rect.FromXYWH(x, y, width, SegmentHeight(group, count)), Group = group });
            float inner = width - SegmentPadding * 2.0f;
            float cellWidth = (inner - CellGap * (columns - 1)) / columns;
            for (int n = 0; n < count; n++)
            {
                int row = n / columns;
                int column = n % columns;
                float left = MathF.Round(x + SegmentPadding + column * (cellWidth + CellGap));
                float right = MathF.Round(x + SegmentPadding + column * (cellWidth + CellGap) + cellWidth);
                float cellTop = y + SegmentPadding + row * (CellHeight + CellGap);
                target.Add(new Placed { Kind = PlacedKind.Cell, Rect = new Rect(new Vector2(left, cellTop), new Vector2(right, cellTop + CellHeight)), Group = group, Item = group.Visible[begin + n] });
            }
        }

        // ------------------------------------------------------------
        // Overflow flyout
        // ------------------------------------------------------------
        // Opens the overflow flyout of the named group in the selected category once it overflows, after delayFrames
        // frames (so focus changes while the editor starts don't close it).
        public void RequestOverflow(string groupName, int delayFrames = 0)
        {
            _requestedOverflow = groupName;
            _requestDelay = Math.Max(delayFrames, 0);
        }

        private void OpenOverflow(RibbonGroup group, int start, Rect button)
        {
            _overflowGroup = group;
            _overflowStart = start;
            _overflowButton = button;
            _openOverflow = true;
        }

        private void DrawMore(Painter draw, in Placed placed)
        {
            Rect rect = placed.Rect;
            Result hit = Gui.HitArea("##more", rect);
            bool open = _overflowGroup == placed.Group;
            draw.RectFilled(rect, Style.Color(hit.Hovered() || open ? StyleColor.PanelHeaderHover : StyleColor.PanelHeaderBg), ButtonRounding);
            draw.Rect(rect, Style.Color(StyleColor.Border), ButtonRounding);
            string label = $"+{placed.Group.Visible.Count - placed.Index} more";
            Vector2 size = Gui.MeasureText(label);
            float total = size.x + 4.0f + 7.0f;
            float x = MathF.Round(rect.Center.x - total * 0.5f);
            float cy = rect.min.y + rect.Height * 0.5f;
            Color32 color = Style.Color(StyleColor.Text);
            draw.Text(new Vector2(x, MathF.Round(cy - _lineHeight * 0.5f)), color, label);
            float ax = x + size.x + 4.0f;
            bool up = _resolved == RibbonSide.Bottom;
            if (up)
                draw.TriangleFilled(new Vector2(ax, cy + 2.0f), new Vector2(ax + 7.0f, cy + 2.0f), new Vector2(ax + 3.5f, cy - 2.0f), color);
            else
                draw.TriangleFilled(new Vector2(ax, cy - 2.0f), new Vector2(ax + 7.0f, cy - 2.0f), new Vector2(ax + 3.5f, cy + 2.0f), color);
            if (hit.Clicked())
                OpenOverflow(placed.Group, placed.Index, rect);
        }

        private void DrawOverflowPopup()
        {
            RibbonGroup? group = _overflowGroup;
            if (group == null)
                return;
            if (_overflowStart >= group.Visible.Count)
            {
                _overflowGroup = null;
                return;
            }
            if (_openOverflow)
            {
                Gui.OpenPopup(OverflowPopupId);
                _openOverflow = false;
            }

            float width = _overflowButton.Width + Inset * 2.0f;
            _popupPlaced.Clear();
            LayoutItems(_popupPlaced, group, _overflowStart, Inset, 0.0f, width - Inset * 2.0f, float.MaxValue, out float bottom);
            float height = bottom + Inset;
            float left = _overflowButton.min.x - Inset;
            float cardBottom = _overflowButton.max.y + Inset + 1.0f;
            Vector2 position = _resolved == RibbonSide.Bottom
                ? new Vector2(left, AreaPadding - height - 4.0f)
                : new Vector2(left, cardBottom + 4.0f);

            using var popup = Gui.Popup(OverflowPopupId, position, new Vector2(width, 0.0f));
            if (!popup.IsOpen)
            {
                _overflowGroup = null;
                return;
            }
            using var canvas = Gui.Canvas("##ribbon_more_items", new Vector2(width, height));
            Painter draw = canvas.Draw;
            bool clicked = false;
            for (int i = 0; i < _popupPlaced.Count; i++)
            {
                Placed placed = _popupPlaced[i];
                using (Gui.Id(i))
                {
                    if (placed.Kind == PlacedKind.Segment)
                        draw.RectFilled(placed.Rect, Style.Color(StyleColor.SegmentBg), CardRounding);
                    else if (placed.Kind == PlacedKind.Gui)
                        DrawGuiItem(placed);
                    else
                        clicked |= DrawItem(draw, placed);
                }
            }
            if (clicked)
            {
                Gui.ClosePopup();
                _overflowGroup = null;
            }
        }

        private void DrawHeader(Painter draw, in Placed placed)
        {
            Rect rect = placed.Rect;
            Result hit = Gui.HitArea("##header", rect);
            if (!string.IsNullOrEmpty(placed.Group.Tooltip))
                Gui.Tooltip(placed.Group.Tooltip);
            if (hit.Clicked())
                placed.Group.Collapsed = !placed.Group.Collapsed;
            Color32 fill = Style.Color(hit.Hovered() ? StyleColor.PanelHeaderHover : StyleColor.PanelHeaderBg);
            draw.RectFilled(rect, fill, CardRounding - 1.0f);
            if (!placed.Group.Collapsed)
            {
                draw.RectFilled(new Rect(new Vector2(rect.min.x, rect.min.y + rect.Height * 0.5f), rect.max), fill);
                draw.Line(new Vector2(rect.min.x, rect.max.y + 0.5f), new Vector2(rect.max.x, rect.max.y + 0.5f), Style.Color(StyleColor.SegmentBg));
            }

            float cy = rect.min.y + rect.Height * 0.5f;
            float ax = rect.min.x + 8.0f;
            Color32 arrow = Style.Color(StyleColor.TextDim);
            if (placed.Group.Collapsed)
                draw.TriangleFilled(new Vector2(ax + 1.0f, cy - 4.0f), new Vector2(ax + 6.0f, cy), new Vector2(ax + 1.0f, cy + 4.0f), arrow);
            else
                draw.TriangleFilled(new Vector2(ax - 1.0f, cy - 2.5f), new Vector2(ax + 7.0f, cy - 2.5f), new Vector2(ax + 3.0f, cy + 2.5f), arrow);
            using (Gui.Font(FontStyle.Bold))
                draw.Text(new Vector2(rect.min.x + 22.0f, MathF.Round(cy - _lineHeight * 0.5f)), Style.Color(StyleColor.Text), placed.Group.Name);
        }

        // Returns true when the item was clicked (and run).
        private bool DrawItem(Painter draw, in Placed placed)
        {
            RibbonItem item = placed.Item!;
            Rect rect = placed.Rect;
            bool enabled = item.IsEnabled?.Invoke() ?? true;
            bool toggled = item.IsToggled?.Invoke() ?? false;
            Result hit = Gui.HitArea("##item", rect);
            string? tip = item.Tooltip ?? (placed.Kind == PlacedKind.Cell ? item.Label : null);
            if (!string.IsNullOrEmpty(tip))
                Gui.Tooltip(tip);

            ToggleButton.Background(draw, rect, toggled, enabled, enabled && hit.Hovered(), enabled && (hit & Result.Active) != 0);
            ToggleButton.Colors(toggled, enabled, item.IconColor, out Color32 iconColor, out Color32 textColor);

            if (placed.Kind == PlacedKind.Cell)
            {
                DrawItemIcon(draw, item, Rect.FromCenterSize(rect.Center, new Vector2(16.0f, 16.0f)), iconColor);
            }
            else
            {
                float cy = rect.min.y + rect.Height * 0.5f;
                DrawItemIcon(draw, item, Rect.FromCenterSize(new Vector2(rect.min.x + 8.0f + 7.0f, cy), new Vector2(14.0f, 14.0f)), iconColor);
                draw.Text(new Vector2(rect.min.x + 8.0f + 14.0f + 8.0f, MathF.Round(cy - _lineHeight * 0.5f)), textColor, item.Label);
            }

            if (item.Dot != ToggleDot.None)
                ToggleButton.Dot(draw, rect, item.Dot, toggled, enabled);
            else if (item.HasOptions)
                draw.CircleFilled(new Vector2(rect.max.x - 5.0f, rect.min.y + 5.0f), 2.0f, toggled ? Style.Color(StyleColor.TextOnAccent) : textColor);

            if (!enabled || !hit.Clicked())
                return false;
            item.Clicked?.Invoke();
            return true;
        }

        private static void DrawGuiItem(in Placed placed)
        {
            RibbonItem item = placed.Item!;
            Vector2 content;
            using (var region = Gui.Region("##gui", placed.Rect, out content))
            {
                if (region.IsOpen)
                    item.OnGUI!();
            }
            if (content.y > 0.0f)
                item.GuiHeight = MathF.Ceiling(content.y);
        }

        private static void DrawItemIcon(Painter draw, RibbonItem item, Rect rect, Color32 color)
        {
            if (item.DrawIcon != null)
                item.DrawIcon(draw, rect, color);
            else if (item.Icon != Icon.None)
                draw.Icon(item.Icon, rect.Center, rect.Width, color);
        }
    }
}
