// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.

using xg.math;

namespace xg.imui
{
    // Immediate drawing in the current canvas (or panel content) space. Mirrors the native xg::imui::Painter.
    public readonly unsafe struct Painter
    {
        private static readonly Color32 White = new(255, 255, 255, 255);

        private readonly nint _ctx;

        internal Painter(nint ctx) => _ctx = ctx;

        public void Line(Vector2 from, Vector2 to, Color32 color, float thickness = 1.0f) =>
            Native.DrawLine(_ctx, &from, &to, color, thickness);

        public void Rect(Rect rect, Color32 color, float rounding = 0.0f, float thickness = 1.0f) =>
            Native.DrawRect(_ctx, &rect, color, rounding, thickness);

        public void RectFilled(Rect rect, Color32 color, float rounding = 0.0f) =>
            Native.DrawRectFilled(_ctx, &rect, color, rounding);

        public void Circle(Vector2 center, float radius, Color32 color, float thickness = 1.0f) =>
            Native.DrawCircle(_ctx, &center, radius, color, thickness);

        public void CircleFilled(Vector2 center, float radius, Color32 color) =>
            Native.DrawCircleFilled(_ctx, &center, radius, color);

        public void TriangleFilled(Vector2 a, Vector2 b, Vector2 c, Color32 color) =>
            Native.DrawTriangleFilled(_ctx, &a, &b, &c, color);

        public void Polyline(ReadOnlySpan<Vector2> points, Color32 color, bool closed = false, float thickness = 1.0f)
        {
            fixed (Vector2* p = points)
                Native.DrawPolyline(_ctx, p, points.Length, color, closed ? (byte)1 : (byte)0, thickness);
        }

        public void Text(Vector2 position, Color32 color, string text)
        {
            fixed (byte* p = Utf8.Encode(text.AsSpan()))
                Native.DrawText(_ctx, &position, color, p);
        }

        public void Text(Vector2 position, Color32 color, ReadOnlySpan<byte> utf8)
        {
            fixed (byte* p = Utf8.Encode(utf8))
                Native.DrawText(_ctx, &position, color, p);
        }

        public void Icon(xg.imui.Icon icon, Vector2 center, float size, Color32 color) =>
            Native.DrawIcon(_ctx, icon, &center, size, color);

        public void Image(Texture texture, Rect rect) =>
            Image(texture, rect, new Vector2(0.0f, 0.0f), new Vector2(1.0f, 1.0f), White);

        public void Image(Texture texture, Rect rect, Vector2 uv0, Vector2 uv1, Color32 tint) =>
            Native.DrawImage(_ctx, texture.Handle, &rect, &uv0, &uv1, tint);

        public void ExternalImage(ulong textureId, Rect rect) =>
            ExternalImage(textureId, rect, new Vector2(0.0f, 0.0f), new Vector2(1.0f, 1.0f), White);

        public void ExternalImage(ulong textureId, Rect rect, Vector2 uv0, Vector2 uv1, Color32 tint) =>
            Native.DrawExternalTexture(_ctx, textureId, &rect, &uv0, &uv1, tint);

        public void PushClip(Rect rect, bool intersect = true) =>
            Native.PushClipRect(_ctx, &rect, intersect ? (byte)1 : (byte)0);

        public void PopClip() => Native.PopClipRect(_ctx);

        public void Layer(DrawLayer layer) => Native.SetDrawLayer(_ctx, layer);
    }
}
