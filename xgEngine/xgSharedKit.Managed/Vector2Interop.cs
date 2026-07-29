using System.Runtime.InteropServices;

namespace xg.math
{
    [StructLayout(LayoutKind.Sequential)]
    public struct Vector2
    {
        public float x;
        public float y;

        private float _pad1;
        private float _pad2;
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

    internal static class NativeVec2
    {
        private const string DLL = "xgScriptNative";

        // Arithmetic
        [DllImport(DLL, EntryPoint = "XG_Vec2_Add")]
        public static extern Vector2 Add(ref Vector2 a, ref Vector2 b);

        [DllImport(DLL, EntryPoint = "XG_Vec2_Sub")]
        public static extern Vector2 Sub(ref Vector2 a, ref Vector2 b);

        [DllImport(DLL, EntryPoint = "XG_Vec2_MulScalar")]
        public static extern Vector2 MulScalar(ref Vector2 v, float s);

        [DllImport(DLL, EntryPoint = "XG_Vec2_DivScalar")]
        public static extern Vector2 DivScalar(ref Vector2 v, float s);

        // Vector math
        [DllImport(DLL, EntryPoint = "XG_Vec2_Length")]
        public static extern float Length(ref Vector2 v);

        [DllImport(DLL, EntryPoint = "XG_Vec2_LengthSquared")]
        public static extern float LengthSquared(ref Vector2 v);

        [DllImport(DLL, EntryPoint = "XG_Vec2_Normalized")]
        public static extern Vector2 Normalized(ref Vector2 v);

        [DllImport(DLL, EntryPoint = "XG_Vec2_Dot")]
        public static extern float Dot(ref Vector2 a, ref Vector2 b);

        // Distance
        [DllImport(DLL, EntryPoint = "XG_Vec2_Distance")]
        public static extern float Distance(ref Vector2 a, ref Vector2 b);

        [DllImport(DLL, EntryPoint = "XG_Vec2_DistanceSquared")]
        public static extern float DistanceSquared(ref Vector2 a, ref Vector2 b);

        // Lerp / Min / Max / Clamp
        [DllImport(DLL, EntryPoint = "XG_Vec2_Lerp")]
        public static extern Vector2 Lerp(ref Vector2 a, ref Vector2 b, float t);

        [DllImport(DLL, EntryPoint = "XG_Vec2_Min")]
        public static extern Vector2 Min(ref Vector2 a, ref Vector2 b);

        [DllImport(DLL, EntryPoint = "XG_Vec2_Max")]
        public static extern Vector2 Max(ref Vector2 a, ref Vector2 b);

        [DllImport(DLL, EntryPoint = "XG_Vec2_Clamp")]
        public static extern Vector2 Clamp(ref Vector2 v, ref Vector2 min, ref Vector2 max);

        // Utility
        [DllImport(DLL, EntryPoint = "XG_Vec2_IsZero")]
        public static extern bool IsZero(ref Vector2 v);

        [DllImport(DLL, EntryPoint = "XG_Vec2_IsEqual")]
        public static extern bool IsEqual(ref Vector2 a, ref Vector2 b, float epsilon);
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
