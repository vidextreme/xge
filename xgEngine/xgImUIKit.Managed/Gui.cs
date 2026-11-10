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
