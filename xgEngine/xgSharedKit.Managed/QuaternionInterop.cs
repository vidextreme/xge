using System.Runtime.InteropServices;

namespace xg.math
{
    [StructLayout(LayoutKind.Sequential)]
    public struct Quaternion
    {
        public float x;
        public float y;
        public float z;
        public float w;

        public Quaternion(float x, float y, float z, float w)
        {
            this.x = x;
            this.y = y;
            this.z = z;
            this.w = w;
        }
    }

    internal static partial class NativeQuat
    {
        private const string DLL = "xgScriptNative";

        // Arithmetic
        [LibraryImport(DLL, EntryPoint = "XG_Quat_Add")]
        [SuppressGCTransition]
        public static partial Quaternion Add(ref Quaternion a, ref Quaternion b);

        [LibraryImport(DLL, EntryPoint = "XG_Quat_Sub")]
        [SuppressGCTransition]
        public static partial Quaternion Sub(ref Quaternion a, ref Quaternion b);

        [LibraryImport(DLL, EntryPoint = "XG_Quat_MulScalar")]
        [SuppressGCTransition]
        public static partial Quaternion MulScalar(ref Quaternion q, float s);

        // Quaternion math
        [LibraryImport(DLL, EntryPoint = "XG_Quat_Dot")]
        [SuppressGCTransition]
        public static partial float Dot(ref Quaternion a, ref Quaternion b);

        [LibraryImport(DLL, EntryPoint = "XG_Quat_Length")]
        [SuppressGCTransition]
        public static partial float Length(ref Quaternion q);

        [LibraryImport(DLL, EntryPoint = "XG_Quat_LengthSquared")]
        [SuppressGCTransition]
        public static partial float LengthSquared(ref Quaternion q);

        [LibraryImport(DLL, EntryPoint = "XG_Quat_Normalized")]
        [SuppressGCTransition]
        public static partial Quaternion Normalized(ref Quaternion q);

        [LibraryImport(DLL, EntryPoint = "XG_Quat_Slerp")]
        [SuppressGCTransition]
        public static partial Quaternion Slerp(ref Quaternion a, ref Quaternion b, float t);

        [LibraryImport(DLL, EntryPoint = "XG_Quat_Inverse")]
        [SuppressGCTransition]
        public static partial Quaternion Inverse(ref Quaternion q);

        [LibraryImport(DLL, EntryPoint = "XG_Quat_Conjugate")]
        [SuppressGCTransition]
        public static partial Quaternion Conjugate(ref Quaternion q);

        [LibraryImport(DLL, EntryPoint = "XG_Quat_MulQuat")]
        [SuppressGCTransition]
        public static partial Quaternion MulQuat(ref Quaternion a, ref Quaternion b);

        // Transform helpers
        [LibraryImport(DLL, EntryPoint = "XG_Quat_RotateVec3")]
        [SuppressGCTransition]
        public static partial Vector3 RotateVec3(ref Quaternion q, ref Vector3 v);

        // Utility
        [LibraryImport(DLL, EntryPoint = "XG_Quat_IsIdentity")]
        [SuppressGCTransition]
        [return: MarshalAs(UnmanagedType.U1)]
        public static partial bool IsIdentity(ref Quaternion q);

        [LibraryImport(DLL, EntryPoint = "XG_Quat_IsEqual")]
        [SuppressGCTransition]
        [return: MarshalAs(UnmanagedType.U1)]
        public static partial bool IsEqual(ref Quaternion a, ref Quaternion b, float epsilon);
    }

    public static class QuatExtensions
    {
        // Operators
        extension(Quaternion target)
        {
            public static Quaternion operator +(Quaternion a, Quaternion b) => NativeQuat.Add(ref a, ref b);
            public static Quaternion operator -(Quaternion a, Quaternion b) => NativeQuat.Sub(ref a, ref b);
            public static Quaternion operator *(Quaternion q, float s) => NativeQuat.MulScalar(ref q, s);
            public static Quaternion operator *(float s, Quaternion q) => NativeQuat.MulScalar(ref q, s);
            public static Quaternion operator *(Quaternion a, Quaternion b) => NativeQuat.MulQuat(ref a, ref b);
            public static Vector3 operator *(Quaternion q, Vector3 v) => NativeQuat.RotateVec3(ref q, ref v);
        }

        // Arithmetic
        public static Quaternion Add(this Quaternion a, Quaternion b) => NativeQuat.Add(ref a, ref b);
        public static Quaternion Sub(this Quaternion a, Quaternion b) => NativeQuat.Sub(ref a, ref b);
        public static Quaternion Mul(this Quaternion q, float s) => NativeQuat.MulScalar(ref q, s);

        // Quaternion math
        public static float Dot(this Quaternion a, Quaternion b) => NativeQuat.Dot(ref a, ref b);
        public static float Length(this Quaternion q) => NativeQuat.Length(ref q);
        public static float LengthSquared(this Quaternion q) => NativeQuat.LengthSquared(ref q);
        public static Quaternion Normalized(this Quaternion q) => NativeQuat.Normalized(ref q);
        public static Quaternion Slerp(this Quaternion a, Quaternion b, float t) => NativeQuat.Slerp(ref a, ref b, t);
        public static Quaternion Inverse(this Quaternion q) => NativeQuat.Inverse(ref q);
        public static Quaternion Conjugate(this Quaternion q) => NativeQuat.Conjugate(ref q);
        public static Quaternion Mul(this Quaternion a, Quaternion b) => NativeQuat.MulQuat(ref a, ref b);

        // Transform helpers
        public static Vector3 RotateVec3(this Quaternion q, Vector3 v) => NativeQuat.RotateVec3(ref q, ref v);

        // Utility
        public static bool IsIdentity(this Quaternion q) => NativeQuat.IsIdentity(ref q);
        public static bool IsEqual(this Quaternion a, Quaternion b, float epsilon = 1e-6f)
            => NativeQuat.IsEqual(ref a, ref b, epsilon);
    }
}