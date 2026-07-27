using System.Runtime.InteropServices;

namespace xg.math
{
    // ------------------------------------------------------------
    // Native struct (matches Mat4x4 union in C++)
    // ------------------------------------------------------------
    [StructLayout(LayoutKind.Sequential)]
    public struct Matrix4x4
    {
        public float m00, m01, m02, m03;
        public float m10, m11, m12, m13;
        public float m20, m21, m22, m23;
        public float m30, m31, m32, m33;

        public float this[int index]
        {
            get => GetElement(index);
            set => SetElement(index, value);
        }

        public float GetElement(int index)
        {
            unsafe
            {
                fixed (Matrix4x4* ptr = &this)
                {
                    float* f = (float*)ptr;
                    return f[index];
                }
            }
        }

        public void SetElement(int index, float value)
        {
            unsafe
            {
                fixed (Matrix4x4* ptr = &this)
                {
                    float* f = (float*)ptr;
                    f[index] = value;
                }
            }
        }

        public override string ToString()
        {
            return
                $"{m00,8:F4} {m01,8:F4} {m02,8:F4} {m03,8:F4}\n" +
                $"{m10,8:F4} {m11,8:F4} {m12,8:F4} {m13,8:F4}\n" +
                $"{m20,8:F4} {m21,8:F4} {m22,8:F4} {m23,8:F4}\n" +
                $"{m30,8:F4} {m31,8:F4} {m32,8:F4} {m33,8:F4}";
        }

        public static Matrix4x4 Identity() 
            => NativeMatrix4x4.Identity();

        public static Matrix4x4 Translation(float x, float y, float z)
            => NativeMatrix4x4.Translation(x, y, z);

        public static Matrix4x4 Scaling(float x, float y, float z)
            => NativeMatrix4x4.Scaling(x, y, z);

        public static Matrix4x4 RotationX(float angle)
            => NativeMatrix4x4.RotationX(angle);

        public static Matrix4x4 RotationY(float angle)
            => NativeMatrix4x4.RotationY(angle);

        public static Matrix4x4 RotationZ(float angle)
            => NativeMatrix4x4.RotationZ(angle);

    }

    // ------------------------------------------------------------
    // Native P/Invoke bindings
    // ------------------------------------------------------------
    internal static class NativeMatrix4x4
    {
        private const string DLL = "xgScriptNative";

        // Creation
        [DllImport(DLL, EntryPoint = "XG_Mat4x4_Identity")]
        public static extern Matrix4x4 Identity();

        [DllImport(DLL, EntryPoint = "XG_Mat4x4_Translation")]
        public static extern Matrix4x4 Translation(float x, float y, float z);

        [DllImport(DLL, EntryPoint = "XG_Mat4x4_Scaling")]
        public static extern Matrix4x4 Scaling(float x, float y, float z);

        [DllImport(DLL, EntryPoint = "XG_Mat4x4_RotationX")]
        public static extern Matrix4x4 RotationX(float angle);

        [DllImport(DLL, EntryPoint = "XG_Mat4x4_RotationY")]
        public static extern Matrix4x4 RotationY(float angle);

        [DllImport(DLL, EntryPoint = "XG_Mat4x4_RotationZ")]
        public static extern Matrix4x4 RotationZ(float angle);

        // Arithmetic
        [DllImport(DLL, EntryPoint = "XG_Mat4x4_Multiply")]
        public static extern Matrix4x4 Multiply(ref Matrix4x4 a, ref Matrix4x4 b);

        [DllImport(DLL, EntryPoint = "XG_Mat4x4_Transposed")]
        public static extern Matrix4x4 Transposed(ref Matrix4x4 m);

        [DllImport(DLL, EntryPoint = "XG_Mat4x4_Inverse")]
        public static extern Matrix4x4 Inverse(ref Matrix4x4 m);

        // Determinant
        [DllImport(DLL, EntryPoint = "XG_Mat4x4_Determinant")]
        public static extern float Determinant(ref Matrix4x4 m);

        // Transform helpers
        [DllImport(DLL, EntryPoint = "XG_Mat4x4_TransformPoint")]
        public static extern Vector3 TransformPoint(ref Matrix4x4 m, ref Vector3 v);

        [DllImport(DLL, EntryPoint = "XG_Mat4x4_TransformDirection")]
        public static extern Vector3 TransformDirection(ref Matrix4x4 m, ref Vector3 v);

        // Decompose
        [DllImport(DLL, EntryPoint = "XG_Mat4x4_Decompose")]
        public static extern void Decompose(
            ref Matrix4x4 m,
            out Vector3 scale,
            out Quaternion rotation,
            out Vector3 translation);
    }

    // ------------------------------------------------------------
    // Extension operators + extension methods
    // ------------------------------------------------------------
    public static class Matrix4x4Extensions
    {
        // Operators (same style as Vec3 and Quat)
        extension(Matrix4x4 target)
        {
            // Matrix * Matrix
            public static Matrix4x4 operator *(Matrix4x4 a, Matrix4x4 b)
                => NativeMatrix4x4.Multiply(ref a, ref b);

            // Matrix * Vector3 (TransformPoint)
            public static Vector3 operator *(Matrix4x4 m, Vector3 v)
                => NativeMatrix4x4.TransformPoint(ref m, ref v);
        }

        // Creation
        public static Matrix4x4 Identity() => NativeMatrix4x4.Identity();
        public static Matrix4x4 Translation(float x, float y, float z) => NativeMatrix4x4.Translation(x, y, z);
        public static Matrix4x4 Scaling(float x, float y, float z) => NativeMatrix4x4.Scaling(x, y, z);
        public static Matrix4x4 RotationX(float angle) => NativeMatrix4x4.RotationX(angle);
        public static Matrix4x4 RotationY(float angle) => NativeMatrix4x4.RotationY(angle);
        public static Matrix4x4 RotationZ(float angle) => NativeMatrix4x4.RotationZ(angle);

        // Arithmetic
        public static Matrix4x4 Mul(this Matrix4x4 a, Matrix4x4 b)
            => NativeMatrix4x4.Multiply(ref a, ref b);

        public static Matrix4x4 Transposed(this Matrix4x4 m)
            => NativeMatrix4x4.Transposed(ref m);

        public static Matrix4x4 Inverse(this Matrix4x4 m)
            => NativeMatrix4x4.Inverse(ref m);

        // Determinant
        public static float Determinant(this Matrix4x4 m)
            => NativeMatrix4x4.Determinant(ref m);

        // Transform helpers
        public static Vector3 TransformPoint(this Matrix4x4 m, Vector3 v)
            => NativeMatrix4x4.TransformPoint(ref m, ref v);

        public static Vector3 TransformDirection(this Matrix4x4 m, Vector3 v)
            => NativeMatrix4x4.TransformDirection(ref m, ref v);

        // Decompose
        public static void Decompose(this Matrix4x4 m,
            out Vector3 scale,
            out Quaternion rotation,
            out Vector3 translation)
            => NativeMatrix4x4.Decompose(ref m, out scale, out rotation, out translation);
    }
}
