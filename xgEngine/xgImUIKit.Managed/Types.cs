// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.

using System.Runtime.InteropServices;
using xg.math;

namespace xg.imui
{
    // Mirrors xg::imui::DockSlot.
    public enum DockSlot
    {
        Floating = 0,
        Center,
        Left,
        Right,
        Bottom,
    }

    // Mirrors xg::imui::PanelFlags.
    [Flags]
    public enum PanelFlags : byte
    {
        None = 0,
        NoScrollbar = 1 << 0,
        NoPadding = 1 << 1,
        StartClosed = 1 << 2,
        // Covers the main window's work area: no title, tab, docking, moving or resizing.
        FillViewport = 1 << 3,
    }

    // Mirrors xg::imui::FontStyle. Order is part of the ABI: append only.
    public enum FontStyle
    {
        Default = 0,
        Bold,
        Mono,
    }

    // Mirrors xg::imui::DrawLayer.
    public enum DrawLayer
    {
        Panel = 0,
        Foreground,
    }

    // Mirrors xg::imui::Result bits.
    [Flags]
    public enum Result : byte
    {
        None = 0,
        Changed = 1 << 0,
        Clicked = 1 << 1,
        Hovered = 1 << 2,
        Active = 1 << 3,
        DoubleClicked = 1 << 4,
        RightClicked = 1 << 5,
    }

    // Mirrors xg::imui::Icon. Order is part of the ABI: append only.
    public enum Icon
    {
        None = 0,
        Hierarchy, SceneGrid, Game, Inspector, Project, Console, Lighting,
        Plus, Search, Star, Folder, Kebab, Account, Cloud, Branch,
        Play, Pause, Step,
        Hand, Move, Rotate, Scale, Rect, Transform,
        PivotCenter, PivotLocal, Globe, Magnet, Sphere, Wire, TwoD,
        Audio, Effects, EyeOff, Gizmo, Stats,
        Scene, Camera, Light, Mesh, Object, Component, Physics,
        Data, Material, Script,
        Info, Warning, Error,
        WinMinimize, WinMaximize, WinRestore, WinClose,
        Image, Trash,
        Cards,
        Tag, File, Filter, Sort, View,
        Lock, Unlock,
        Clock, Stack,
        PlayWindow,
        Settings, Code, Save,
        Eye, FolderOpen, Link,
        Group, GroupOpen,
    }

    // Mirrors xg::imui::Key. Order is part of the ABI: append only.
    public enum Key
    {
        None = 0,
        Tab, Left, Right, Up, Down, PageUp, PageDown, Home, End, Delete, Backspace, Enter, Escape,
    }

    // Mirrors xg::imui::TextFieldFlags.
    [Flags]
    public enum TextFieldFlags : byte
    {
        None = 0,
        // Replaces the text being edited with the buffer's content and puts the caret at its end.
        SetText = 1 << 0,
        // Tab, Up and Down stay with the caller (autocomplete) instead of moving focus.
        CaptureNavKeys = 1 << 1,
    }

    // Mirrors xg::imui::StyleColor. Order is part of the ABI: append only.
    public enum StyleColor
    {
        Text = 0,
        TextDim,
        TextSelected,
        Accent,
        AccentHover,
        Selection,
        ActiveObject,
        PanelBg,
        PanelHeaderBg,
        AreaBg,
        HeaderBg,
        ToolbarBg,
        ViewportBg,
        Outline,
        Icon,
        IconObject,
        IconData,
        IconComponent,
        IconMaterial,
        IconWarning,
        AxisX,
        AxisY,
        AxisZ,
        Grid,
        TextOnAccent,
        PanelHeaderHover,
        TopBarBg,
        SegmentBg,
        Button,
        ButtonHover,
        Border,
    }

    // Mirrors xg::imui::NumberFieldFlags (ApiVersion 21).
    [Flags]
    public enum NumberFieldFlags : byte
    {
        None = 0,
        Range = 1 << 0,
        Integer = 1 << 1,
        Color = 1 << 2,
    }

    // How Gui.Field(label, Span<float>, style) draws its elements: Range (Min/Max) clamps and draws a range line per
    // element, Integer shows whole numbers, Color puts a swatch before 3 or 4 elements. NotchTags: "x,y,z", "deg",
    // "near=#c8a040, far=#a060c8" (see NotchTagsAttribute); null draws no tags.
    public struct NumberFieldStyle
    {
        public NumberFieldFlags Flags;
        public float Min;
        public float Max;
        public string? NotchTags;

        public static NumberFieldStyle Ranged(float min, float max, string? tags = null) =>
            new() { Flags = NumberFieldFlags.Range, Min = min, Max = max, NotchTags = tags };

        public static NumberFieldStyle Tagged(string? tags) => new() { NotchTags = tags };
    }

    // Mirrors xg::imui::ScrollAreaFlags.
    [Flags]
    public enum ScrollAreaFlags : byte
    {
        None = 0,
        Horizontal = 1 << 0,
        Padded = 1 << 1,
    }

    // Mirrors xg::imui::PanelPlacementFlags.
    [Flags]
    public enum PanelPlacementFlags : byte
    {
        None = 0,
        Docked = 1 << 0,
        MainViewport = 1 << 1,
    }

    // Mirrors xg::imui::PanelPlacement: where the current panel is, in screen coordinates. DockNode is the panel's
    // dock node and DockSpace the root of its dock tree (both equal Window while floating).
    [StructLayout(LayoutKind.Sequential)]
    public struct PanelPlacement
    {
        public uint Size;
        public Rect Window;
        public Rect DockNode;
        public Rect DockSpace;
        public PanelPlacementFlags Flags;

        public readonly bool Docked => (Flags & PanelPlacementFlags.Docked) != 0;
    }

    // Mirrors xg::imui::SceneViewFlags.
    [Flags]
    public enum SceneViewFlags : byte
    {
        None = 0,
        Hovered = 1 << 0,
        Active = 1 << 1,
        LeftDown = 1 << 2,
        RightDown = 1 << 3,
        MiddleDown = 1 << 4,
    }

    // Mirrors xg::imui::ShellBar.
    public enum ShellBar
    {
        Toolbar = 0,
        StatusBar,
    }

    // Mirrors xg::imui::SceneViewState. Set TextureId (and optionally Placeholder text) before Gui.SceneView; read the rest after.
    [StructLayout(LayoutKind.Sequential)]
    public unsafe struct SceneViewState
    {
        public uint Size;
        public ulong TextureId;
        internal byte* PlaceholderUtf8;
        public Vector2 PixelSize;
        public Vector2 MousePosition;
        public Vector2 DragLeft;
        public Vector2 DragRight;
        public Vector2 DragMiddle;
        public float Wheel;
        public SceneViewFlags Flags;

        public readonly bool Hovered => (Flags & SceneViewFlags.Hovered) != 0;
        public readonly bool Active => (Flags & SceneViewFlags.Active) != 0;
        public readonly int PixelWidth => (int)PixelSize.x;
        public readonly int PixelHeight => (int)PixelSize.y;
    }

    // Mirrors xg::imui::ListFlags.
    [Flags]
    public enum ListFlags : byte
    {
        None = 0,
        ScrollToBottom = 1 << 0,
        AtBottom = 1 << 1,
        // Rows draw no alternating background (the caller tints rows itself).
        NoStripes = 1 << 2,
    }

    // Mirrors xg::imui::ListState. Set Count and RowHeight (and ScrollToBottom) before Gui.List; read First, Last and AtBottom after.
    [StructLayout(LayoutKind.Sequential)]
    public struct ListState
    {
        public uint Size;
        public int Count;
        public float RowHeight;
        public int First;
        public int Last;
        public ListFlags Flags;

        public readonly bool AtBottom => (Flags & ListFlags.AtBottom) != 0;
    }

    // Mirrors xg::imui::TreeRowFlags.
    [Flags]
    public enum TreeRowFlags : byte
    {
        None = 0,
        HasChildren = 1 << 0,
        Expanded = 1 << 1,
        Selected = 1 << 2,
    }

    // Theme colors resolved by the native kit.
    public static unsafe class Style
    {
        public static Color32 Color(StyleColor token)
        {
            Color32 color;
            Native.StyleToColor(token, &color);
            return color;
        }
    }

    public static class ResultExtensions
    {
        public static bool Clicked(this Result result) => (result & Result.Clicked) != 0;
        public static bool Changed(this Result result) => (result & Result.Changed) != 0;
        public static bool Hovered(this Result result) => (result & Result.Hovered) != 0;
    }
}
