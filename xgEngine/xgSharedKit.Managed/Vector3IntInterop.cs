// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.

using System.Runtime.InteropServices;

// Managed mirror of xgBase/math/xgVec3Int.h (pure C#, same 12-byte layout as xg::Vec3Int).

namespace xg.math
{
    [StructLayout(LayoutKind.Sequential)]
    public struct Vector3Int : IEquatable<Vector3Int>
    {
        public int x;
        public int y;
        public int z;

        public Vector3Int(int x, int y, int z)
        {
            this.x = x;
            this.y = y;
            this.z = z;
        }

        public Vector3Int(Vector2Int xy, int z) : this(xy.x, xy.y, z) { }

        public override string ToString() => $"Vector3Int({x}, {y}, {z})";

        // ------------------------------------------------------------
        // Operators
        // ------------------------------------------------------------
        public static Vector3Int operator +(Vector3Int a, Vector3Int b) => new(a.x + b.x, a.y + b.y, a.z + b.z);
        public static Vector3Int operator -(Vector3Int a, Vector3Int b) => new(a.x - b.x, a.y - b.y, a.z - b.z);
        public static Vector3Int operator *(Vector3Int a, Vector3Int b) => new(a.x * b.x, a.y * b.y, a.z * b.z);
        public static Vector3Int operator *(Vector3Int v, int s) => new(v.x * s, v.y * s, v.z * s);
        public static Vector3Int operator *(int s, Vector3Int v) => new(v.x * s, v.y * s, v.z * s);
        public static Vector3Int operator /(Vector3Int v, int s) => new(v.x / s, v.y / s, v.z / s);
        public static Vector3Int operator -(Vector3Int v) => new(-v.x, -v.y, -v.z);
        public static bool operator ==(Vector3Int a, Vector3Int b) => a.x == b.x && a.y == b.y && a.z == b.z;
        public static bool operator !=(Vector3Int a, Vector3Int b) => !(a == b);

        public static implicit operator Vector3(Vector3Int v) => new(v.x, v.y, v.z);

        public bool Equals(Vector3Int other) => this == other;
        public override bool Equals(object? obj) => obj is Vector3Int other && this == other;
        public override int GetHashCode() => unchecked((int)Hash());

        // ------------------------------------------------------------
        // Operations
        // ------------------------------------------------------------
        public int LengthSquared() => x * x + y * y + z * z;
        public float Length() => MathF.Sqrt(LengthSquared());
        public Vector3 ToVector3() => new(x, y, z);
        public Vector2Int XY() => new(x, y);

        // Same hash as xg::Vec3Int::Hash()
        public uint Hash() => unchecked((uint)x * 73856093u ^ (uint)y * 19349663u ^ (uint)z * 83492791u);

        public Vector3Int FloorDiv(int d) => new(Vector2Int.FloorDiv(x, d), Vector2Int.FloorDiv(y, d), Vector2Int.FloorDiv(z, d));
        public Vector3Int FloorMod(int d) => new(Vector2Int.FloorMod(x, d), Vector2Int.FloorMod(y, d), Vector2Int.FloorMod(z, d));

        public static Vector3Int Min(Vector3Int a, Vector3Int b) => new(Math.Min(a.x, b.x), Math.Min(a.y, b.y), Math.Min(a.z, b.z));
        public static Vector3Int Max(Vector3Int a, Vector3Int b) => new(Math.Max(a.x, b.x), Math.Max(a.y, b.y), Math.Max(a.z, b.z));
        public static Vector3Int Abs(Vector3Int v) => new(Math.Abs(v.x), Math.Abs(v.y), Math.Abs(v.z));
        public static Vector3Int Clamp(Vector3Int v, Vector3Int lo, Vector3Int hi)
            => new(Math.Clamp(v.x, lo.x, hi.x), Math.Clamp(v.y, lo.y, hi.y), Math.Clamp(v.z, lo.z, hi.z));

        public static int ManhattanDistance(Vector3Int a, Vector3Int b)
            => Math.Abs(a.x - b.x) + Math.Abs(a.y - b.y) + Math.Abs(a.z - b.z);
        public static int ChebyshevDistance(Vector3Int a, Vector3Int b)
            => Math.Max(Math.Max(Math.Abs(a.x - b.x), Math.Abs(a.y - b.y)), Math.Abs(a.z - b.z));

        public static Vector3Int FloorToInt(Vector3 v) => new((int)MathF.Floor(v.x), (int)MathF.Floor(v.y), (int)MathF.Floor(v.z));
        public static Vector3Int CeilToInt(Vector3 v) => new((int)MathF.Ceiling(v.x), (int)MathF.Ceiling(v.y), (int)MathF.Ceiling(v.z));
        public static Vector3Int RoundToInt(Vector3 v) => new(
            (int)MathF.Round(v.x, MidpointRounding.AwayFromZero),
            (int)MathF.Round(v.y, MidpointRounding.AwayFromZero),
            (int)MathF.Round(v.z, MidpointRounding.AwayFromZero));

        public static Vector3Int Zero() => new(0, 0, 0);
        public static Vector3Int One() => new(1, 1, 1);
        public static Vector3Int Up() => new(0, 1, 0);
        public static Vector3Int Down() => new(0, -1, 0);
        public static Vector3Int Left() => new(-1, 0, 0);
        public static Vector3Int Right() => new(1, 0, 0);
        public static Vector3Int Forward() => new(0, 0, 1);
        public static Vector3Int Back() => new(0, 0, -1);
    }
}
