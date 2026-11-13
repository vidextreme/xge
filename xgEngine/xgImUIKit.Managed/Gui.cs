// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.

using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;
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
        // Property rows: a label column plus the value filling the rest (inspector building blocks)
        // ------------------------------------------------------------
        public static bool Field(string label, ref int value, float speed = 0.2f)
        {
            fixed (byte* p = Utf8.Encode(label.AsSpan()))
            fixed (int* v = &value)
                return (Native.FieldInt(Context, p, v, speed) & Result.Changed) != 0;
        }

        public static bool Field(string label, ref Vector2 value, float speed = 0.1f)
        {
            fixed (byte* p = Utf8.Encode(label.AsSpan()))
            fixed (Vector2* v = &value)
                return (Native.FieldVector(Context, p, (float*)v, 2, speed) & Result.Changed) != 0;
        }

        public static bool Field(string label, ref Vector3 value, float speed = 0.1f)
        {
            fixed (byte* p = Utf8.Encode(label.AsSpan()))
            fixed (Vector3* v = &value)
                return (Native.FieldVector(Context, p, (float*)v, 3, speed) & Result.Changed) != 0;
        }

        public static bool Field(string label, TextBuffer text)
        {
            Result result;
            fixed (byte* p = Utf8.Encode(label.AsSpan()))
            fixed (byte* buffer = text.Buffer)
                result = Native.FieldText(Context, p, buffer, text.Buffer.Length);
            bool changed = (result & Result.Changed) != 0;
            if (changed)
                text.MarkEdited();
            return changed;
        }

        public static bool Slider(string label, ref float value, float min, float max)
        {
            fixed (byte* p = Utf8.Encode(label.AsSpan()))
            fixed (float* v = &value)
                return (Native.SliderFloat(Context, p, v, min, max) & Result.Changed) != 0;
        }

        public static bool Slider(string label, ref int value, int min, int max)
        {
            fixed (byte* p = Utf8.Encode(label.AsSpan()))
            fixed (int* v = &value)
                return (Native.SliderInt(Context, p, v, min, max) & Result.Changed) != 0;
        }

        public static bool Checkbox(string label, ref bool value)
        {
            byte on = value ? (byte)1 : (byte)0;
            Result result;
            fixed (byte* p = Utf8.Encode(label.AsSpan()))
                result = Native.Checkbox(Context, p, &on);
            value = on != 0;
            return (result & Result.Changed) != 0;
        }

        public static bool Combo(string label, ref int index, IReadOnlyList<string> items)
        {
            byte[][] encoded = new byte[items.Count][];
            GCHandle[] pins = new GCHandle[items.Count];
            nint* pointers = stackalloc nint[Math.Max(items.Count, 1)];
            try
            {
                for (int i = 0; i < items.Count; i++)
                {
                    encoded[i] = System.Text.Encoding.UTF8.GetBytes(items[i] + "\0");
                    pins[i] = GCHandle.Alloc(encoded[i], GCHandleType.Pinned);
                    pointers[i] = pins[i].AddrOfPinnedObject();
                }
                fixed (byte* p = Utf8.Encode(label.AsSpan()))
                fixed (int* v = &index)
                    return (Native.Combo(Context, p, v, (byte**)pointers, items.Count) & Result.Changed) != 0;
            }
            finally
            {
                for (int i = 0; i < pins.Length; i++)
                    if (pins[i].IsAllocated)
                        pins[i].Free();
            }
        }

        // Full-width collapsible header. Returns whether it is open (after this frame's click).
        public static bool Foldout(string label, ref bool open)
        {
            byte value = open ? (byte)1 : (byte)0;
            fixed (byte* p = Utf8.Encode(label.AsSpan()))
                Native.Foldout(Context, p, &value);
            open = value != 0;
            return open;
        }

        public static Result PropertyLabel(string label, string value)
        {
            fixed (byte* p = Utf8.Encode(label.AsSpan(), 0))
            fixed (byte* v = Utf8.Encode(value.AsSpan(), 1))
                return Native.PropertyLabel(Context, p, v);
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

        // Single-line text box; hint shows while empty. width <= 0 fills the line.
        public static bool TextField(string id, TextBuffer text, string? hint = null, float width = 0.0f)
        {
            Result result;
            fixed (byte* p = Utf8.Encode(id.AsSpan(), 0))
            fixed (byte* h = Utf8.Encode(hint, 1))
            fixed (byte* buffer = text.Buffer)
                result = Native.TextField(Context, p, h, buffer, text.Buffer.Length, width);
            bool changed = (result & Result.Changed) != 0;
            if (changed)
                text.MarkEdited();
            return changed;
        }

        // Square flat icon button laid out inline. size <= 0 uses the frame height.
        public static bool IconButton(string id, Icon icon, float size, Color32 color)
        {
            fixed (byte* p = Utf8.Encode(id.AsSpan()))
                return (Native.IconButton(Context, p, icon, null, size, color) & Result.Clicked) != 0;
        }

        // Icon button placed at position in the current draw space (row or canvas space, like Painter), over the row.
        public static bool IconButton(string id, Icon icon, Vector2 position, float size, Color32 color)
        {
            fixed (byte* p = Utf8.Encode(id.AsSpan()))
                return (Native.IconButton(Context, p, icon, &position, size, color) & Result.Clicked) != 0;
        }

        // Invisible interactive rectangle in the current draw space: Hovered, Clicked, DoubleClicked, RightClicked.
        public static Result HitArea(string id, Rect rect)
        {
            fixed (byte* p = Utf8.Encode(id.AsSpan()))
                return Native.HitArea(Context, p, &rect);
        }

        // Tooltip shown while the previous item is hovered.
        public static void Tooltip(string text)
        {
            fixed (byte* p = Utf8.Encode(text.AsSpan()))
                Native.ItemTooltip(Context, p);
        }

        // Modals: Gui.OpenPopup(id) once, then every frame: using (var m = Gui.Modal(id, title)) { if (m.IsOpen) { ... } }.
        public static void OpenPopup(string id)
        {
            fixed (byte* p = Utf8.Encode(id.AsSpan()))
                Native.OpenPopup(Context, p);
        }

        // Centered on the main window, dims it; size.y <= 0 fits the content. Escape closes it.
        public static ModalScope Modal(string id, string title, Vector2 size = default)
        {
            nint ctx = Context;
            byte open;
            fixed (byte* p = Utf8.Encode(id.AsSpan(), 0))
            fixed (byte* t = Utf8.Encode(title.AsSpan(), 1))
                open = Native.BeginModal(ctx, p, t, &size);
            return new ModalScope(ctx, open != 0);
        }

        public static void CloseModal() => Native.CloseModal(Context);

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

        // Toolbars are rows: items go left to right without SameLine.
        public static ToolbarScope Toolbar()
        {
            nint ctx = Context;
            Native.BeginToolbar(ctx);
            return new ToolbarScope(ctx);
        }

        // Layout groups. The panel body (and each modal) is already a column.
        //   using (Gui.Horizontal()) { Gui.Label("Name"); Gui.TextField("##name", name); }   // field fills the rest
        //   using (Gui.Horizontal()) { Gui.FlexibleSpace(); Gui.Button("OK"); Gui.Button("Cancel"); }  // right-aligned
        // Size 0 on TextField, SearchField, TextArea, Canvas and List fills: in a row (width) or column (height) the item
        // shares the space left after the fixed items with FlexibleSpace. Shares use the previous frame's measurements.
        public static LayoutScope Horizontal(float spacing = -1.0f)
        {
            nint ctx = Context;
            Native.BeginHorizontal(ctx, spacing);
            return new LayoutScope(ctx, true);
        }

        public static LayoutScope Vertical()
        {
            nint ctx = Context;
            Native.BeginVertical(ctx);
            return new LayoutScope(ctx, false);
        }

        // Takes the leftover space of the current row or column; several share it equally.
        public static void FlexibleSpace() => Native.FlexibleSpace(Context);

        // Extra gap before the next item in a row, or a vertical gap in a column.
        public static void Space(float pixels) => Native.Space(Context, pixels);

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

        // Tree row inside Gui.List: indented by depth, expand arrow when HasChildren. Toggled means the arrow was clicked.
        public static TreeRowScope TreeRow(int index, int depth, TreeRowFlags flags)
        {
            nint ctx = Context;
            Result result = Native.BeginTreeRow(ctx, index, depth, flags);
            return new TreeRowScope(ctx, result);
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

    public readonly ref struct LayoutScope
    {
        private readonly nint _ctx;
        private readonly bool _horizontal;

        internal LayoutScope(nint ctx, bool horizontal)
        {
            _ctx = ctx;
            _horizontal = horizontal;
        }

        public void Dispose()
        {
            if (_ctx == 0)
                return;
            if (_horizontal)
                Native.EndHorizontal(_ctx);
            else
                Native.EndVertical(_ctx);
        }
    }

    public readonly ref struct ModalScope
    {
        private readonly nint _ctx;

        internal ModalScope(nint ctx, bool open)
        {
            _ctx = ctx;
            IsOpen = open;
        }

        public bool IsOpen { get; }

        public void Dispose()
        {
            if (_ctx != 0 && IsOpen)
                Native.EndModal(_ctx);
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

    // One tree row. Draw in row space: (0,0) is just after the expand arrow.
    public readonly ref struct TreeRowScope
    {
        private readonly nint _ctx;

        internal TreeRowScope(nint ctx, Result interaction)
        {
            _ctx = ctx;
            Interaction = interaction;
        }

        public Result Interaction { get; }
        public bool Clicked => (Interaction & Result.Clicked) != 0;
        public bool DoubleClicked => (Interaction & Result.DoubleClicked) != 0;
        public bool Toggled => (Interaction & Result.Changed) != 0;
        public Painter Draw => new(_ctx);

        public void Dispose()
        {
            if (_ctx != 0)
                Native.EndTreeRow(_ctx);
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
