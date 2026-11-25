// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.

using xg.imui;
using xg.math;

namespace xgEditor.CoreCLR
{
    // Small drawing helpers shared by the official editor panels, matching the Ribbon and Inspector style.
    internal static class EditorStyle
    {
        public const float SelectionBarWidth = 3.0f;
        // Panel bars and rows: 6 px from the panel edge, 4 px between bars and between items in a bar.
        public const float BarPadding = 6.0f;
        public const float BarGap = 4.0f;
        public const float BarEdge = BarPadding - BarGap;

        // Accent bar on the left edge of a selected list or tree row; x is the row's left edge in the row's draw space.
        public static void SelectionBar(Painter draw, float x, float height) =>
            draw.RectFilled(Rect.FromXYWH(x, 0.0f, SelectionBarWidth, height), Style.Color(StyleColor.Accent));
    }
}
