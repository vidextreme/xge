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
    }

    // Mirrors xg::imui::SceneViewFlags.
    [Flags]
    public enum SceneViewFlags : byte
    {
        None = 0,
        Hovered = 1 << 0,
        Active = 1 << 1,
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
