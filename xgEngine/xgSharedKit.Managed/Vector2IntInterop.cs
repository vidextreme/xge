// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.

using System.Runtime.InteropServices;

// Managed mirror of xgBase/math/xgVec2Int.h (pure C#, same 8-byte layout as xg::Vec2Int).

namespace xg.math
{
    [StructLayout(LayoutKind.Sequential)]
    public struct Vector2Int : IEquatable<Vector2Int>
    {
        public int x;
        public int y;

        public Vector2Int(int x, int y)
        {
            this.x = x;
            this.y = y;
        }

        public override string ToString() => $"Vector2Int({x}, {y})";

        // ------------------------------------------------------------
        // Operators
        // ------------------------------------------------------------
        public static Vector2Int operator +(Vector2Int a, Vector2Int b) => new(a.x + b.x, a.y + b.y);
        public static Vector2Int operator -(Vector2Int a, Vector2Int b) => new(a.x - b.x, a.y - b.y);
        public static Vector2Int operator *(Vector2Int a, Vector2Int b) => new(a.x * b.x, a.y * b.y);
        public static Vector2Int operator *(Vector2Int v, int s) => new(v.x * s, v.y * s);
        public static Vector2Int operator *(int s, Vector2Int v) => new(v.x * s, v.y * s);
        public static Vector2Int operator /(Vector2Int v, int s) => new(v.x / s, v.y / s);
        public static Vector2Int operator -(Vector2Int v) => new(-v.x, -v.y);
        public static bool operator ==(Vector2Int a, Vector2Int b) => a.x == b.x && a.y == b.y;
        public static bool operator !=(Vector2Int a, Vector2Int b) => !(a == b);

        public static implicit operator Vector2(Vector2Int v) => new(v.x, v.y);

        public bool Equals(Vector2Int other) => this == other;
        public override bool Equals(object? obj) => obj is Vector2Int other && this == other;
        public override int GetHashCode() => unchecked((int)Hash());

        // ------------------------------------------------------------
        // Operations
        // ------------------------------------------------------------
        public int LengthSquared() => x * x + y * y;
        public float Length() => MathF.Sqrt(LengthSquared());
        public Vector2 ToVector2() => new(x, y);

        // Same hash as xg::Vec2Int::Hash()
        public uint Hash() => unchecked((uint)x * 73856093u ^ (uint)y * 19349663u);

        // Floor division / modulo (world cell -> chunk, local cell)
        public static int FloorDiv(int a, int b)
        {
            int q = a / b;
            return (a % b != 0 && ((a < 0) != (b < 0))) ? q - 1 : q;
        }

        public static int FloorMod(int a, int b) => a - FloorDiv(a, b) * b;

        public Vector2Int FloorDiv(int d) => new(FloorDiv(x, d), FloorDiv(y, d));
        public Vector2Int FloorMod(int d) => new(FloorMod(x, d), FloorMod(y, d));

        public static Vector2Int Min(Vector2Int a, Vector2Int b) => new(Math.Min(a.x, b.x), Math.Min(a.y, b.y));
        public static Vector2Int Max(Vector2Int a, Vector2Int b) => new(Math.Max(a.x, b.x), Math.Max(a.y, b.y));
        public static Vector2Int Abs(Vector2Int v) => new(Math.Abs(v.x), Math.Abs(v.y));
        public static Vector2Int Clamp(Vector2Int v, Vector2Int lo, Vector2Int hi)
            => new(Math.Clamp(v.x, lo.x, hi.x), Math.Clamp(v.y, lo.y, hi.y));

        public static int ManhattanDistance(Vector2Int a, Vector2Int b) => Math.Abs(a.x - b.x) + Math.Abs(a.y - b.y);
        public static int ChebyshevDistance(Vector2Int a, Vector2Int b) => Math.Max(Math.Abs(a.x - b.x), Math.Abs(a.y - b.y));

        public static Vector2Int FloorToInt(Vector2 v) => new((int)MathF.Floor(v.x), (int)MathF.Floor(v.y));
        public static Vector2Int CeilToInt(Vector2 v) => new((int)MathF.Ceiling(v.x), (int)MathF.Ceiling(v.y));
        public static Vector2Int RoundToInt(Vector2 v)
            => new((int)MathF.Round(v.x, MidpointRounding.AwayFromZero), (int)MathF.Round(v.y, MidpointRounding.AwayFromZero));

        public static Vector2Int Zero() => new(0, 0);
        public static Vector2Int One() => new(1, 1);
        public static Vector2Int Up() => new(0, 1);
        public static Vector2Int Down() => new(0, -1);
        public static Vector2Int Left() => new(-1, 0);
        public static Vector2Int Right() => new(1, 0);
    }
}
