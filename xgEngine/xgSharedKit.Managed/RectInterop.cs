// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.

using System.Runtime.InteropServices;

// Managed mirror of xgBase/math/xgRect.h (pure C#, same layouts as xg::Rect / xg::RectInt).

namespace xg.math
{
    [StructLayout(LayoutKind.Sequential)]
    public struct Rect
    {
        public Vector2 min;
        public Vector2 max;

        public Rect(Vector2 min, Vector2 max)
        {
            this.min = min;
            this.max = max;
        }

        public override string ToString() => $"Rect(min: ({min.x:F4}, {min.y:F4}), max: ({max.x:F4}, {max.y:F4}))";

        public static Rect FromXYWH(float x, float y, float width, float height) => new(new Vector2(x, y), new Vector2(x + width, y + height));

        public static Rect FromCenterSize(Vector2 center, Vector2 size)
        {
            Vector2 half = VecMath.Scale(size, 0.5f);
            return new Rect(VecMath.Sub(center, half), VecMath.Add(center, half));
        }

        public static Rect FromPoints(Vector2 a, Vector2 b)
            => new(new Vector2(MathF.Min(a.x, b.x), MathF.Min(a.y, b.y)), new Vector2(MathF.Max(a.x, b.x), MathF.Max(a.y, b.y)));

        public float X => min.x;
        public float Y => min.y;
        public float Width => max.x - min.x;
        public float Height => max.y - min.y;
        public Vector2 Size => VecMath.Sub(max, min);
        public Vector2 Center => VecMath.Scale(VecMath.Add(min, max), 0.5f);
        public float Area => Width * Height;
        public bool IsValid => min.x <= max.x && min.y <= max.y;

        public bool Contains(Vector2 p) => p.x >= min.x && p.x <= max.x && p.y >= min.y && p.y <= max.y;
        public bool Contains(Rect r) => r.min.x >= min.x && r.max.x <= max.x && r.min.y >= min.y && r.max.y <= max.y;
        public bool Overlaps(Rect r) => min.x <= r.max.x && max.x >= r.min.x && min.y <= r.max.y && max.y >= r.min.y;

        // IsValid is false when they don't overlap
        public Rect Intersection(Rect r)
            => new(new Vector2(MathF.Max(min.x, r.min.x), MathF.Max(min.y, r.min.y)), new Vector2(MathF.Min(max.x, r.max.x), MathF.Min(max.y, r.max.y)));

        public Rect Union(Rect r)
            => new(new Vector2(MathF.Min(min.x, r.min.x), MathF.Min(min.y, r.min.y)), new Vector2(MathF.Max(max.x, r.max.x), MathF.Max(max.y, r.max.y)));

        public Vector2 ClosestPoint(Vector2 p) => new(Math.Clamp(p.x, min.x, max.x), Math.Clamp(p.y, min.y, max.y));

        public void Encapsulate(Vector2 p)
        {
            min = new Vector2(MathF.Min(min.x, p.x), MathF.Min(min.y, p.y));
            max = new Vector2(MathF.Max(max.x, p.x), MathF.Max(max.y, p.y));
        }

        public void Encapsulate(Rect r) => this = Union(r);

        public Rect Expanded(float amount) => new(new Vector2(min.x - amount, min.y - amount), new Vector2(max.x + amount, max.y + amount));

        // (0,0) = min, (1,1) = max
        public Vector2 NormalizedToPoint(Vector2 n) => new(min.x + (max.x - min.x) * n.x, min.y + (max.y - min.y) * n.y);

        public Vector2 PointToNormalized(Vector2 p)
        {
            float w = Width;
            float h = Height;
            return new Vector2(w != 0f ? (p.x - min.x) / w : 0f, h != 0f ? (p.y - min.y) / h : 0f);
        }

        public static Rect Zero() => new(new Vector2(0f, 0f), new Vector2(0f, 0f));
        public static Rect Unit() => new(new Vector2(0f, 0f), new Vector2(1f, 1f));
    }

    // min inclusive, max exclusive (pixels, tiles, scissor rects)
    [StructLayout(LayoutKind.Sequential)]
    public struct RectInt
    {
        public Vector2Int min;
        public Vector2Int max;

        public RectInt(Vector2Int min, Vector2Int max)
        {
            this.min = min;
            this.max = max;
        }

        public override string ToString() => $"RectInt(min: {min}, max: {max})";

        public static RectInt FromXYWH(int x, int y, int width, int height) => new(new Vector2Int(x, y), new Vector2Int(x + width, y + height));

        public int X => min.x;
        public int Y => min.y;
        public int Width => max.x - min.x;
        public int Height => max.y - min.y;
        public Vector2Int Size => max - min;
        public int Area => Width * Height;
        public bool IsEmpty => max.x <= min.x || max.y <= min.y;

        public bool Contains(Vector2Int p) => p.x >= min.x && p.x < max.x && p.y >= min.y && p.y < max.y;
        public bool Overlaps(RectInt r) => min.x < r.max.x && max.x > r.min.x && min.y < r.max.y && max.y > r.min.y;
        public RectInt Intersection(RectInt r) => new(Vector2Int.Max(min, r.min), Vector2Int.Min(max, r.max));
        public RectInt Union(RectInt r) => new(Vector2Int.Min(min, r.min), Vector2Int.Max(max, r.max));
        public Vector2Int ClampPoint(Vector2Int p) => Vector2Int.Clamp(p, min, max - new Vector2Int(1, 1));
        public Rect ToRect() => new(min.ToVector2(), max.ToVector2());
    }
}
