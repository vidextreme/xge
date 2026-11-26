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

    // Hierarchy state colours: lock is amber, hidden is violet; muted variants when a parent overrides the row's own flag.
    internal static class HierarchyStyle
    {
        public static readonly Color32 Lock = new(240, 184, 74, 255);
        public static readonly Color32 LockLine = new(224, 168, 60, 255);
        public static readonly Color32 LockCell = new(224, 168, 60, 56);
        public static readonly Color32 LockMuted = new(138, 115, 72, 255);
        public static readonly Color32 LockMutedCell = new(224, 168, 60, 20);
        public static readonly Color32 Hidden = new(196, 162, 240, 255);
        public static readonly Color32 HiddenLine = new(160, 120, 220, 255);
        public static readonly Color32 HiddenCell = new(160, 120, 220, 56);
        public static readonly Color32 HiddenMuted = new(122, 104, 152, 255);
        public static readonly Color32 HiddenMutedCell = new(160, 120, 220, 20);
        public static readonly Color32 Unset = new(92, 96, 104, 255);
        public static readonly Color32 UnsetHover = new(150, 154, 162, 255);
        public static readonly Color32 Overridden = new(66, 69, 76, 255);
        public static readonly Color32 Composed = new(143, 209, 158, 255);
        public static readonly Color32 FolderIcon = new(201, 168, 106, 255);
        public static readonly Color32 FolderRow = new(110, 140, 200, 18);
        public static readonly Color32 DescendantSelected = new(61, 139, 253, 28);
        public static readonly Color32 DescendantBar = new(61, 139, 253, 90);
        public static readonly Color32 DescendantHover = new(255, 255, 255, 8);
        public static readonly Color32 CellDivider = new(0, 0, 0, 64);
    }
}
