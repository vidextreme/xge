using System.Runtime.InteropServices;

namespace xg.math
{
    [StructLayout(LayoutKind.Sequential)]
    public struct Vector3
    {
        public float x;
        public float y;
        public float z;

        public Vector3(float x, float y, float z)
        {
            this.x = x;
            this.y = y;
            this.z = z;
        }

        public override string ToString()
        {
            return $"Vector3({x:F4}, {y:F4}, {z:F4})";
        }


        // ------------------------------------------------------------
        // Standard static constructors
        // ------------------------------------------------------------
        public static Vector3 Zero()
            => new Vector3(0f, 0f, 0f);

        public static Vector3 One()
            => new Vector3(1f, 1f, 1f);

        public static Vector3 UnitX()
            => new Vector3(1f, 0f, 0f);

        public static Vector3 UnitY()
            => new Vector3(0f, 1f, 0f);

        public static Vector3 UnitZ()
            => new Vector3(0f, 0f, 1f);

        public static Vector3 Right()
            => new Vector3(1f, 0f, 0f);

        public static Vector3 Up()
            => new Vector3(0f, 1f, 0f);

        public static Vector3 Forward()
            => new Vector3(0f, 0f, 1f);

        // Spherical coordinates (θ = azimuth, φ = elevation)
        public static Vector3 FromSpherical(float theta, float phi)
        {
            float cosPhi = MathF.Cos(phi);
            return new Vector3(
                cosPhi * MathF.Cos(theta),
                MathF.Sin(phi),
                cosPhi * MathF.Sin(theta)
            );
        }

        // Yaw (Y), Pitch (X), Roll (Z)
        public static Vector3 FromYawPitchRoll(float yaw, float pitch, float roll)
        {
            // Forward vector from yaw/pitch
            float cy = MathF.Cos(yaw);
            float sy = MathF.Sin(yaw);
            float cp = MathF.Cos(pitch);
            float sp = MathF.Sin(pitch);

            return new Vector3(
                cy * cp,
                sp,
                sy * cp
            );
        }
    }

    internal static class NativeVec3
    {
        private const string DLL = "xgScriptNative";

        // Arithmetic
        [DllImport(DLL, EntryPoint = "XG_Vec3_Add")]
        public static extern Vector3 Add(ref Vector3 a, ref Vector3 b);

        [DllImport(DLL, EntryPoint = "XG_Vec3_Sub")]
        public static extern Vector3 Sub(ref Vector3 a, ref Vector3 b);

        [DllImport(DLL, EntryPoint = "XG_Vec3_MulScalar")]
        public static extern Vector3 MulScalar(ref Vector3 v, float s);

        [DllImport(DLL, EntryPoint = "XG_Vec3_DivScalar")]
        public static extern Vector3 DivScalar(ref Vector3 v, float s);

        // Vector math
        [DllImport(DLL, EntryPoint = "XG_Vec3_Length")]
        public static extern float Length(ref Vector3 v);

        [DllImport(DLL, EntryPoint = "XG_Vec3_LengthSquared")]
        public static extern float LengthSquared(ref Vector3 v);

        [DllImport(DLL, EntryPoint = "XG_Vec3_Normalized")]
        public static extern Vector3 Normalized(ref Vector3 v);

        [DllImport(DLL, EntryPoint = "XG_Vec3_Dot")]
        public static extern float Dot(ref Vector3 a, ref Vector3 b);

        [DllImport(DLL, EntryPoint = "XG_Vec3_Cross")]
        public static extern Vector3 Cross(ref Vector3 a, ref Vector3 b);

        // Distance
        [DllImport(DLL, EntryPoint = "XG_Vec3_Distance")]
        public static extern float Distance(ref Vector3 a, ref Vector3 b);

        [DllImport(DLL, EntryPoint = "XG_Vec3_DistanceSquared")]
        public static extern float DistanceSquared(ref Vector3 a, ref Vector3 b);

        // Lerp / Min / Max / Clamp
        [DllImport(DLL, EntryPoint = "XG_Vec3_Lerp")]
        public static extern Vector3 Lerp(ref Vector3 a, ref Vector3 b, float t);

        [DllImport(DLL, EntryPoint = "XG_Vec3_Min")]
        public static extern Vector3 Min(ref Vector3 a, ref Vector3 b);

        [DllImport(DLL, EntryPoint = "XG_Vec3_Max")]
        public static extern Vector3 Max(ref Vector3 a, ref Vector3 b);

        [DllImport(DLL, EntryPoint = "XG_Vec3_Clamp")]
        public static extern Vector3 Clamp(ref Vector3 v, ref Vector3 min, ref Vector3 max);

        // Transform
        [DllImport(DLL, EntryPoint = "XG_Vec3_Transform")]
        public static extern Vector3 Transform(ref Vector3 v, ref Matrix4x4 m);

        [DllImport(DLL, EntryPoint = "XG_Vec3_TransformDirection")]
        public static extern Vector3 TransformDirection(ref Vector3 v, ref Matrix4x4 m);

        // Utility
        [DllImport(DLL, EntryPoint = "XG_Vec3_IsZero")]
        public static extern bool IsZero(ref Vector3 v);

        [DllImport(DLL, EntryPoint = "XG_Vec3_IsEqual")]
        public static extern bool IsEqual(ref Vector3 a, ref Vector3 b, float epsilon);
    }

    public static class Vec3Extensions
    {
        // Operators
        extension(Vector3 target)
        {
            public static Vector3 operator +(Vector3 a, Vector3 b) => NativeVec3.Add(ref a, ref b);
            public static Vector3 operator -(Vector3 a, Vector3 b) => NativeVec3.Sub(ref a, ref b);
            public static Vector3 operator *(Vector3 v, float s) => NativeVec3.MulScalar(ref v, s);
            public static Vector3 operator /(Vector3 v, float s) => NativeVec3.DivScalar(ref v, s);
        }

        // Arithmetic
        public static Vector3 Add(this Vector3 a, Vector3 b) => NativeVec3.Add(ref a, ref b);
        public static Vector3 Sub(this Vector3 a, Vector3 b) => NativeVec3.Sub(ref a, ref b);
        public static Vector3 Mul(this Vector3 v, float s) => NativeVec3.MulScalar(ref v, s);
        public static Vector3 Div(this Vector3 v, float s) => NativeVec3.DivScalar(ref v, s);

        // Vector math
        public static float Length(this Vector3 v) => NativeVec3.Length(ref v);
        public static float LengthSquared(this Vector3 v) => NativeVec3.LengthSquared(ref v);
        public static Vector3 Normalized(this Vector3 v) => NativeVec3.Normalized(ref v);
        public static float Dot(this Vector3 a, Vector3 b) => NativeVec3.Dot(ref a, ref b);
        public static Vector3 Cross(this Vector3 a, Vector3 b) => NativeVec3.Cross(ref a, ref b);

        // Distance
        public static float Distance(this Vector3 a, Vector3 b) => NativeVec3.Distance(ref a, ref b);
        public static float DistanceSquared(this Vector3 a, Vector3 b) => NativeVec3.DistanceSquared(ref a, ref b);

        // Lerp / Min / Max / Clamp
        public static Vector3 Lerp(this Vector3 a, Vector3 b, float t) => NativeVec3.Lerp(ref a, ref b, t);
        public static Vector3 Min(this Vector3 a, Vector3 b) => NativeVec3.Min(ref a, ref b);
        public static Vector3 Max(this Vector3 a, Vector3 b) => NativeVec3.Max(ref a, ref b);
        public static Vector3 Clamp(this Vector3 v, Vector3 min, Vector3 max) => NativeVec3.Clamp(ref v, ref min, ref max);

        // Transform
        public static Vector3 Transform(this Vector3 v, Matrix4x4 m) => NativeVec3.Transform(ref v, ref m);
        public static Vector3 TransformDirection(this Vector3 v, Matrix4x4 m) => NativeVec3.TransformDirection(ref v, ref m);

        // Utility
        public static bool IsZero(this Vector3 v) => NativeVec3.IsZero(ref v);
        public static bool IsEqual(this Vector3 a, Vector3 b, float epsilon = 1e-6f)
            => NativeVec3.IsEqual(ref a, ref b, epsilon);
    }
}
