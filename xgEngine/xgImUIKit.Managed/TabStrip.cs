// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.

using xg.math;

namespace xg.imui
{
    // Side of the dock area a panel sits on.
    public enum PanelSide
    {
        Left = 0,
        Right,
        Top,
        Bottom,
    }

    // Which edge of a tab faces away from the content (the rounded one).
    public enum TabEdge
    {
        Right = 0,
        Left,
        Top,
    }

    // Shared drawing for Ribbon-style tab strips (Ribbon, Inspector).
    public static class TabStrip
    {
        public const float Rounding = 6.0f;
        public const float AccentWidth = 4.0f;

        // Rounded on the outer edge only; the selected tab joins the content area (ToolbarBg) and shows a thick accent
        // band on its outer edge that follows the rounded corners.
        public static void DrawShape(Painter draw, Rect tab, TabEdge edge, bool selected, bool hovered)
        {
            Color32 fill = selected ? Style.Color(StyleColor.ToolbarBg)
                : hovered ? Style.Color(StyleColor.PanelHeaderHover) : Style.Color(StyleColor.HeaderBg);
            Rect inner = edge switch
            {
                TabEdge.Right => new Rect(tab.min, new Vector2(tab.min.x + tab.Width * 0.5f, tab.max.y)),
                TabEdge.Left => new Rect(new Vector2(tab.max.x - tab.Width * 0.5f, tab.min.y), tab.max),
                _ => new Rect(new Vector2(tab.min.x, tab.max.y - tab.Height * 0.5f), tab.max),
            };
            if (selected)
            {
                draw.RectFilled(tab, Style.Color(StyleColor.Accent), Rounding);
                Rect body = edge switch
                {
                    TabEdge.Right => new Rect(tab.min, new Vector2(tab.max.x - AccentWidth, tab.max.y)),
                    TabEdge.Left => new Rect(new Vector2(tab.min.x + AccentWidth, tab.min.y), tab.max),
                    _ => new Rect(new Vector2(tab.min.x, tab.min.y + AccentWidth), tab.max),
                };
                draw.RectFilled(body, fill, Rounding);
            }
            else
            {
                draw.RectFilled(tab, fill, Rounding);
            }
            draw.RectFilled(inner, fill);
        }

        // Dock side of a panel: a dock node touching only the left or right edge of its dock tree picks that side, only
        // the top or bottom edge likewise, both kinds by the node's shape (wider = top/bottom); otherwise (floating or
        // central) the window's aspect ratio decides.
        public static PanelSide Detect(in PanelPlacement placement)
        {
            const float Tolerance = 4.0f;
            if (placement.Docked)
            {
                Rect node = placement.DockNode;
                Rect space = placement.DockSpace;
                bool left = MathF.Abs(node.min.x - space.min.x) <= Tolerance;
                bool right = MathF.Abs(node.max.x - space.max.x) <= Tolerance;
                bool top = MathF.Abs(node.min.y - space.min.y) <= Tolerance;
                bool bottom = MathF.Abs(node.max.y - space.max.y) <= Tolerance;
                bool sideEdge = left != right;
                bool capEdge = top != bottom;
                if (sideEdge && capEdge)
                {
                    if (node.Width > node.Height)
                        sideEdge = false;
                    else
                        capEdge = false;
                }
                if (sideEdge)
                    return left ? PanelSide.Left : PanelSide.Right;
                if (capEdge)
                    return top ? PanelSide.Top : PanelSide.Bottom;
            }
            return placement.Window.Width > placement.Window.Height ? PanelSide.Top : PanelSide.Left;
        }
    }
}
