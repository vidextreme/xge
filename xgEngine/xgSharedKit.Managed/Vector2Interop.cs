// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.

using System.Runtime.InteropServices;

namespace xg.math
{
    // 8 bytes, matches C++ Vec2 (alignas(4)). No padding: an 8-byte struct is returned
    // in a register by native code, a 16-byte one through a hidden pointer.
    [StructLayout(LayoutKind.Sequential)]
    public struct Vector2
    {
        public float x;
        public float y;

        public Vector2(float x, float y)
        {
            this.x = x;
            this.y = y;
        }

        public override string ToString()
        {
            return $"Vector2({x:F4}, {y:F4})";
        }

        public static implicit operator Vector2(Vector3 v)
        {
            return new Vector2(v.x, v.y);
        }

        // ------------------------------------------------------------
        // Standard static constructors
        // ------------------------------------------------------------
        public static Vector2 Zero()
            => new Vector2(0f, 0f);

        public static Vector2 One()
            => new Vector2(1f, 1f);

        public static Vector2 UnitX()
            => new Vector2(1f, 0f);

        public static Vector2 UnitY()
            => new Vector2(0f, 1f);

        public static Vector2 FromAngle(float radians)
            => new Vector2(MathF.Cos(radians), MathF.Sin(radians));

    }

    internal static partial class NativeVec2
    {
        private const string DLL = "xgScriptNative";

        // Arithmetic
        [LibraryImport(DLL, EntryPoint = "XG_Vec2_Add")]
        [SuppressGCTransition]
        public static partial Vector2 Add(ref Vector2 a, ref Vector2 b);

        [LibraryImport(DLL, EntryPoint = "XG_Vec2_Sub")]
        [SuppressGCTransition]
        public static partial Vector2 Sub(ref Vector2 a, ref Vector2 b);

        [LibraryImport(DLL, EntryPoint = "XG_Vec2_MulScalar")]
        [SuppressGCTransition]
        public static partial Vector2 MulScalar(ref Vector2 v, float s);

        [LibraryImport(DLL, EntryPoint = "XG_Vec2_DivScalar")]
        [SuppressGCTransition]
        public static partial Vector2 DivScalar(ref Vector2 v, float s);

        // Vector math
        [LibraryImport(DLL, EntryPoint = "XG_Vec2_Length")]
        [SuppressGCTransition]
        public static partial float Length(ref Vector2 v);

        [LibraryImport(DLL, EntryPoint = "XG_Vec2_LengthSquared")]
        [SuppressGCTransition]
        public static partial float LengthSquared(ref Vector2 v);

        [LibraryImport(DLL, EntryPoint = "XG_Vec2_Normalized")]
        [SuppressGCTransition]
        public static partial Vector2 Normalized(ref Vector2 v);

        [LibraryImport(DLL, EntryPoint = "XG_Vec2_Dot")]
        [SuppressGCTransition]
        public static partial float Dot(ref Vector2 a, ref Vector2 b);

        // Distance
        [LibraryImport(DLL, EntryPoint = "XG_Vec2_Distance")]
        [SuppressGCTransition]
        public static partial float Distance(ref Vector2 a, ref Vector2 b);

        [LibraryImport(DLL, EntryPoint = "XG_Vec2_DistanceSquared")]
        [SuppressGCTransition]
        public static partial float DistanceSquared(ref Vector2 a, ref Vector2 b);

        // Lerp / Min / Max / Clamp
        [LibraryImport(DLL, EntryPoint = "XG_Vec2_Lerp")]
        [SuppressGCTransition]
        public static partial Vector2 Lerp(ref Vector2 a, ref Vector2 b, float t);

        [LibraryImport(DLL, EntryPoint = "XG_Vec2_Min")]
        [SuppressGCTransition]
        public static partial Vector2 Min(ref Vector2 a, ref Vector2 b);

        [LibraryImport(DLL, EntryPoint = "XG_Vec2_Max")]
        [SuppressGCTransition]
        public static partial Vector2 Max(ref Vector2 a, ref Vector2 b);

        [LibraryImport(DLL, EntryPoint = "XG_Vec2_Clamp")]
        [SuppressGCTransition]
        public static partial Vector2 Clamp(ref Vector2 v, ref Vector2 min, ref Vector2 max);

        // Utility
        [LibraryImport(DLL, EntryPoint = "XG_Vec2_IsZero")]
        [SuppressGCTransition]
        [return: MarshalAs(UnmanagedType.U1)]
        public static partial bool IsZero(ref Vector2 v);

        [LibraryImport(DLL, EntryPoint = "XG_Vec2_IsEqual")]
        [SuppressGCTransition]
        [return: MarshalAs(UnmanagedType.U1)]
        public static partial bool IsEqual(ref Vector2 a, ref Vector2 b, float epsilon);
    }

    public static class Vec2Extensions
    {
        // Arithmetic
        public static Vector2 Add(this Vector2 a, Vector2 b) => NativeVec2.Add(ref a, ref b);
        public static Vector2 Sub(this Vector2 a, Vector2 b) => NativeVec2.Sub(ref a, ref b);
        public static Vector2 Mul(this Vector2 v, float s) => NativeVec2.MulScalar(ref v, s);
        public static Vector2 Div(this Vector2 v, float s) => NativeVec2.DivScalar(ref v, s);

        // Vector math
        public static float Length(this Vector2 v) => NativeVec2.Length(ref v);
        public static float LengthSquared(this Vector2 v) => NativeVec2.LengthSquared(ref v);
        public static Vector2 Normalized(this Vector2 v) => NativeVec2.Normalized(ref v);
        public static float Dot(this Vector2 a, Vector2 b) => NativeVec2.Dot(ref a, ref b);

        // Distance
        public static float Distance(this Vector2 a, Vector2 b) => NativeVec2.Distance(ref a, ref b);
        public static float DistanceSquared(this Vector2 a, Vector2 b) => NativeVec2.DistanceSquared(ref a, ref b);

        // Lerp / Min / Max / Clamp
        public static Vector2 Lerp(this Vector2 a, Vector2 b, float t) => NativeVec2.Lerp(ref a, ref b, t);
        public static Vector2 Min(this Vector2 a, Vector2 b) => NativeVec2.Min(ref a, ref b);
        public static Vector2 Max(this Vector2 a, Vector2 b) => NativeVec2.Max(ref a, ref b);
        public static Vector2 Clamp(this Vector2 v, Vector2 min, Vector2 max) => NativeVec2.Clamp(ref v, ref min, ref max);

        // Utility
        public static bool IsZero(this Vector2 v) => NativeVec2.IsZero(ref v);
        public static bool IsEqual(this Vector2 a, Vector2 b, float epsilon = 1e-6f)
            => NativeVec2.IsEqual(ref a, ref b, epsilon);
    }
}
