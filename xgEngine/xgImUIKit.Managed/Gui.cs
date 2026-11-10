// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.

using System.Runtime.CompilerServices;
using xg.math;

namespace xg.imui
{
    // Immediate-mode GUI. Valid only inside Panel.OnGUI (the active panel context).
    // Mirrors the native xg::imui::Gui; the Unity-style layout layer will be added on top of this class.
    public static unsafe class Gui
    {
        [ThreadStatic] private static nint _context;

        internal static nint Context
        {
            get
            {
                if (_context == 0)
                    throw new InvalidOperationException("xg.imui.Gui can only be used inside Panel.OnGUI.");
                return _context;
            }
        }

        internal static nint Enter(nint context)
        {
            nint previous = _context;
            _context = context;
            return previous;
        }

        internal static void Leave(nint previous) => _context = previous;

        public static bool IsAvailable => _context != 0;

        // ------------------------------------------------------------
        // Widgets
        // ------------------------------------------------------------
        public static Result Label(string text)
        {
            fixed (byte* p = Utf8.Encode(text.AsSpan()))
                return Native.Label(Context, p);
        }

        public static Result Label(ReadOnlySpan<byte> utf8)
        {
            fixed (byte* p = Utf8.Encode(utf8))
                return Native.Label(Context, p);
        }

        public static bool Button(string label) => (ButtonResult(label) & Result.Clicked) != 0;

        public static bool Button(ReadOnlySpan<byte> utf8Label)
        {
            fixed (byte* p = Utf8.Encode(utf8Label))
                return (Native.Button(Context, p) & Result.Clicked) != 0;
        }

        public static Result ButtonResult(string label)
        {
            fixed (byte* p = Utf8.Encode(label.AsSpan()))
                return Native.Button(Context, p);
        }

        public static bool Field(string label, ref float value, float speed = 0.1f)
        {
            fixed (byte* p = Utf8.Encode(label.AsSpan()))
            fixed (float* v = &value)
                return (Native.FieldFloat(Context, p, v, speed) & Result.Changed) != 0;
        }

        // ------------------------------------------------------------
        // Identity scopes: using (Gui.Id(item)) { ... }
        // ------------------------------------------------------------
        public static IdScope Id(string id)
        {
            nint ctx = Context;
            fixed (byte* p = Utf8.Encode(id.AsSpan()))
                Native.PushID(ctx, p);
            return new IdScope(ctx);
        }

        public static IdScope Id(int id)
        {
            nint ctx = Context;
            Native.PushIDInt(ctx, id);
            return new IdScope(ctx);
        }

        public static IdScope Id(object owner)
        {
            nint ctx = Context;
            Native.PushIDInt(ctx, RuntimeHelpers.GetHashCode(owner));
            return new IdScope(ctx);
        }

        // ------------------------------------------------------------
        // Immediate drawing: using (var canvas = Gui.Canvas("view", new Vector2(0, 160))) { canvas.Draw.Line(...); }
        // ------------------------------------------------------------
        public static CanvasScope Canvas(string id, Vector2 size = default)
        {
            nint ctx = Context;
            Rect rect;
            Result result;
            fixed (byte* p = Utf8.Encode(id.AsSpan()))
                result = Native.BeginCanvas(ctx, p, &size, &rect);
            return new CanvasScope(ctx, rect, result);
        }

        // Shows an externally rendered texture (state.TextureId) and reports pixel size and input into state.
        public static Result SceneView(string id, ref SceneViewState state, string? placeholder = null, Vector2 size = default)
        {
            nint ctx = Context;
            state.Size = (uint)sizeof(SceneViewState);
            fixed (byte* p = Utf8.Encode(id.AsSpan(), 0))
            fixed (byte* text = Utf8.Encode(placeholder, 1))
            fixed (SceneViewState* s = &state)
            {
                s->PlaceholderUtf8 = text;
                Result result = Native.SceneView(ctx, p, &size, s);
                s->PlaceholderUtf8 = null;
                return result;
            }
        }

        public static Painter Draw => new(Context);

        // ------------------------------------------------------------
        // Toggles, search and text
        // ------------------------------------------------------------
        public static bool Toggle(string label, ref bool on)
        {
            byte value = on ? (byte)1 : (byte)0;
            Result result;
            fixed (byte* p = Utf8.Encode(label.AsSpan()))
                result = Native.ToggleButton(Context, p, &value);
            on = value != 0;
            return (result & Result.Changed) != 0;
        }

        public static bool IconToggle(string id, Icon icon, ref bool on, Color32 color, string? text = null)
        {
            byte value = on ? (byte)1 : (byte)0;
            Result result;
            fixed (byte* p = Utf8.Encode(id.AsSpan(), 0))
            fixed (byte* t = Utf8.Encode(text, 1))
                result = Native.IconToggle(Context, p, icon, &value, color, t);
            on = value != 0;
            return (result & Result.Changed) != 0;
        }

        public static bool SearchField(string id, TextBuffer text, float width = 200.0f)
        {
            Result result;
            fixed (byte* p = Utf8.Encode(id.AsSpan()))
            fixed (byte* buffer = text.Buffer)
                result = Native.SearchField(Context, p, buffer, text.Buffer.Length, width);
            bool changed = (result & Result.Changed) != 0;
            if (changed)
                text.MarkEdited();
            return changed;
        }

        // Read-only, selectable text; size <= 0 fills the available space.
        public static Result TextArea(string id, string text, Vector2 size = default)
        {
            fixed (byte* p = Utf8.Encode(id.AsSpan(), 0))
            fixed (byte* t = Utf8.Encode(text.AsSpan(), 1))
                return Native.TextArea(Context, p, t, &size);
        }

        // ------------------------------------------------------------
        // Layout
        // ------------------------------------------------------------
        // using (Gui.Font(FontStyle.Mono)) { ... }: widgets and Painter text use the font until disposed.
        public static FontScope Font(FontStyle style)
        {
            nint ctx = Context;
            Native.PushFont(ctx, style);
            return new FontScope(ctx);
        }

        public static ToolbarScope Toolbar()
        {
            nint ctx = Context;
            Native.BeginToolbar(ctx);
            return new ToolbarScope(ctx);
        }

        public static void SameLine(float spacing = -1.0f) => Native.SameLine(Context, spacing);

        public static void AlignRight(float width) => Native.AlignRight(Context, width);

        public static Vector2 AvailableSize => Native.AvailableSize(Context);

        // Horizontal bar; dragging it changes size (the height of the area below it).
        public static bool Splitter(string id, ref float size, float minSize, float maxSize)
        {
            Result result;
            fixed (byte* p = Utf8.Encode(id.AsSpan()))
            fixed (float* s = &size)
                result = Native.Splitter(Context, p, s, minSize, maxSize);
            return (result & Result.Changed) != 0;
        }

        // ------------------------------------------------------------
        // Virtualized lists: using (Gui.List("log", ref state)) for (int i = state.First; i < state.Last; ++i) { using var row = Gui.ListRow(i, sel); }
        // ------------------------------------------------------------
        public static ListScope List(string id, ref ListState state, Vector2 size = default)
        {
            nint ctx = Context;
            state.Size = (uint)sizeof(ListState);
            byte open;
            fixed (byte* p = Utf8.Encode(id.AsSpan()))
            fixed (ListState* s = &state)
                open = Native.BeginList(ctx, p, &size, s);
            return new ListScope(ctx, open != 0);
        }

        public static ListRowScope ListRow(int index, bool selected)
        {
            nint ctx = Context;
            Result result = Native.BeginListRow(ctx, index, selected ? (byte)1 : (byte)0);
            return new ListRowScope(ctx, result);
        }

        public static Vector2 MeasureText(string text)
        {
            fixed (byte* p = Utf8.Encode(text.AsSpan()))
                return Native.MeasureText(p);
        }
    }

    public readonly ref struct IdScope
    {
        private readonly nint _ctx;

        internal IdScope(nint ctx) => _ctx = ctx;

        public void Dispose()
        {
            if (_ctx != 0)
                Native.PopID(_ctx);
        }
    }

    public readonly ref struct FontScope
    {
        private readonly nint _ctx;

        internal FontScope(nint ctx) => _ctx = ctx;

        public void Dispose()
        {
            if (_ctx != 0)
                Native.PopFont(_ctx);
        }
    }

    public readonly ref struct ToolbarScope
    {
        private readonly nint _ctx;

        internal ToolbarScope(nint ctx) => _ctx = ctx;

        public void Dispose()
        {
            if (_ctx != 0)
                Native.EndToolbar(_ctx);
        }
    }

    public readonly ref struct ListScope
    {
        private readonly nint _ctx;

        internal ListScope(nint ctx, bool open)
        {
            _ctx = ctx;
            IsOpen = open;
        }

        public bool IsOpen { get; }

        public void Dispose()
        {
            if (_ctx != 0 && IsOpen)
                Native.EndList(_ctx);
        }
    }

    // One list row. Draw in row space: (0,0) is the row top-left.
    public readonly ref struct ListRowScope
    {
        private readonly nint _ctx;

        internal ListRowScope(nint ctx, Result interaction)
        {
            _ctx = ctx;
            Interaction = interaction;
        }

        public Result Interaction { get; }
        public bool Clicked => (Interaction & Result.Clicked) != 0;
        public Painter Draw => new(_ctx);

        public void Dispose()
        {
            if (_ctx != 0)
                Native.EndListRow(_ctx);
        }
    }

    public readonly ref struct CanvasScope
    {
        private readonly nint _ctx;

        internal CanvasScope(nint ctx, Rect bounds, Result interaction)
        {
            _ctx = ctx;
            Bounds = bounds;
            Interaction = interaction;
        }

        // Canvas bounds in panel content space. Draw in canvas space: (0,0) is the canvas top-left.
        public Rect Bounds { get; }
        public Vector2 Size => Bounds.Size;
        public Result Interaction { get; }
        public Painter Draw => new(_ctx);

        public void Dispose()
        {
            if (_ctx != 0)
                Native.EndCanvas(_ctx);
        }
    }
}
