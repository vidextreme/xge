// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.

using System.Runtime.InteropServices;

// Managed mirror of xgBase/math/xgMat3x3.h (pure C#, same 36-byte layout as xg::Mat3x3).
// Row-major, row-vector convention (v' = v * M), same as Matrix4x4.

namespace xg.math
{
    [StructLayout(LayoutKind.Sequential)]
    public struct Matrix3x3
    {
        public float m00, m01, m02;
        public float m10, m11, m12;
        public float m20, m21, m22;

        public Matrix3x3(
            float m00, float m01, float m02,
            float m10, float m11, float m12,
            float m20, float m21, float m22)
        {
            this.m00 = m00; this.m01 = m01; this.m02 = m02;
            this.m10 = m10; this.m11 = m11; this.m12 = m12;
            this.m20 = m20; this.m21 = m21; this.m22 = m22;
        }

        // From three rows
        public Matrix3x3(Vector3 row0, Vector3 row1, Vector3 row2)
            : this(row0.x, row0.y, row0.z, row1.x, row1.y, row1.z, row2.x, row2.y, row2.z) { }

        public override string ToString()
            => $"Matrix3x3([{m00:F4}, {m01:F4}, {m02:F4}], [{m10:F4}, {m11:F4}, {m12:F4}], [{m20:F4}, {m21:F4}, {m22:F4}])";

        public Vector3 Row(int i) => i switch
        {
            0 => new Vector3(m00, m01, m02),
            1 => new Vector3(m10, m11, m12),
            _ => new Vector3(m20, m21, m22),
        };

        public Vector3 Column(int j) => j switch
        {
            0 => new Vector3(m00, m10, m20),
            1 => new Vector3(m01, m11, m21),
            _ => new Vector3(m02, m12, m22),
        };

        // ------------------------------------------------------------
        // Operations
        // ------------------------------------------------------------

        // a then b: v * (a * b) == (v * a) * b
        public static Matrix3x3 Multiply(in Matrix3x3 a, in Matrix3x3 b) => new(
            a.m00 * b.m00 + a.m01 * b.m10 + a.m02 * b.m20,
            a.m00 * b.m01 + a.m01 * b.m11 + a.m02 * b.m21,
            a.m00 * b.m02 + a.m01 * b.m12 + a.m02 * b.m22,
            a.m10 * b.m00 + a.m11 * b.m10 + a.m12 * b.m20,
            a.m10 * b.m01 + a.m11 * b.m11 + a.m12 * b.m21,
            a.m10 * b.m02 + a.m11 * b.m12 + a.m12 * b.m22,
            a.m20 * b.m00 + a.m21 * b.m10 + a.m22 * b.m20,
            a.m20 * b.m01 + a.m21 * b.m11 + a.m22 * b.m21,
            a.m20 * b.m02 + a.m21 * b.m12 + a.m22 * b.m22);

        public static Matrix3x3 operator *(Matrix3x3 a, Matrix3x3 b) => Multiply(a, b);
        public static Vector3 operator *(Vector3 v, Matrix3x3 m) => m.Transform(v);

        // v * M
        public Vector3 Transform(Vector3 v) => new(
            v.x * m00 + v.y * m10 + v.z * m20,
            v.x * m01 + v.y * m11 + v.z * m21,
            v.x * m02 + v.y * m12 + v.z * m22);

        public Matrix3x3 Transposed() => new(m00, m10, m20, m01, m11, m21, m02, m12, m22);

        public float Determinant()
            => m00 * (m11 * m22 - m12 * m21)
             - m01 * (m10 * m22 - m12 * m20)
             + m02 * (m10 * m21 - m11 * m20);

        public bool TryInverse(out Matrix3x3 result)
        {
            float det = Determinant();
            if (MathF.Abs(det) < 1e-12f)
            {
                result = Identity();
                return false;
            }
            float inv = 1.0f / det;
            result = new Matrix3x3(
                (m11 * m22 - m12 * m21) * inv,
                (m02 * m21 - m01 * m22) * inv,
                (m01 * m12 - m02 * m11) * inv,
                (m12 * m20 - m10 * m22) * inv,
                (m00 * m22 - m02 * m20) * inv,
                (m02 * m10 - m00 * m12) * inv,
                (m10 * m21 - m11 * m20) * inv,
                (m01 * m20 - m00 * m21) * inv,
                (m00 * m11 - m01 * m10) * inv);
            return true;
        }

        // Identity if singular
        public Matrix3x3 Inverse()
        {
            TryInverse(out Matrix3x3 r);
            return r;
        }

        // ------------------------------------------------------------
        // Rotation conversion
        // ------------------------------------------------------------

        // v * FromQuaternion(q) == q rotating v (same convention as Matrix4x4 rotation)
        public static Matrix3x3 FromQuaternion(Quaternion q)
        {
            float xx = q.x * q.x, yy = q.y * q.y, zz = q.z * q.z;
            float xy = q.x * q.y, xz = q.x * q.z, yz = q.y * q.z;
            float wx = q.w * q.x, wy = q.w * q.y, wz = q.w * q.z;
            return new Matrix3x3(
                1.0f - 2.0f * (yy + zz), 2.0f * (xy + wz), 2.0f * (xz - wy),
                2.0f * (xy - wz), 1.0f - 2.0f * (xx + zz), 2.0f * (yz + wx),
                2.0f * (xz + wy), 2.0f * (yz - wx), 1.0f - 2.0f * (xx + yy));
        }

        // Assumes a pure rotation (orthonormal) matrix
        public Quaternion ToQuaternion()
        {
            float trace = m00 + m11 + m22;
            Quaternion q;
            if (trace > 0.0f)
            {
                float s = MathF.Sqrt(trace + 1.0f) * 2.0f;
                q = new Quaternion((m12 - m21) / s, (m20 - m02) / s, (m01 - m10) / s, 0.25f * s);
            }
            else if (m00 > m11 && m00 > m22)
            {
                float s = MathF.Sqrt(1.0f + m00 - m11 - m22) * 2.0f;
                q = new Quaternion(0.25f * s, (m01 + m10) / s, (m02 + m20) / s, (m12 - m21) / s);
            }
            else if (m11 > m22)
            {
                float s = MathF.Sqrt(1.0f + m11 - m00 - m22) * 2.0f;
                q = new Quaternion((m01 + m10) / s, 0.25f * s, (m12 + m21) / s, (m20 - m02) / s);
            }
            else
            {
                float s = MathF.Sqrt(1.0f + m22 - m00 - m11) * 2.0f;
                q = new Quaternion((m02 + m20) / s, (m12 + m21) / s, 0.25f * s, (m01 - m10) / s);
            }
            return VecMath.QuatNormalize(q);
        }

        // Upper-left 3x3 (rotation + scale)
        public static Matrix3x3 FromMatrix4x4(in Matrix4x4 m) => new(
            m.m00, m.m01, m.m02,
            m.m10, m.m11, m.m12,
            m.m20, m.m21, m.m22);

        // Inverse-transpose of the upper 3x3, for transforming normals
        public static Matrix3x3 NormalMatrix(in Matrix4x4 world) => FromMatrix4x4(world).Inverse().Transposed();

        // ------------------------------------------------------------
        // Factories
        // ------------------------------------------------------------
        public static Matrix3x3 Identity() => new(1f, 0f, 0f, 0f, 1f, 0f, 0f, 0f, 1f);
        public static Matrix3x3 Zero() => new(0f, 0f, 0f, 0f, 0f, 0f, 0f, 0f, 0f);
        public static Matrix3x3 Scaling(Vector3 s) => new(s.x, 0f, 0f, 0f, s.y, 0f, 0f, 0f, s.z);
        public static Matrix3x3 RotationAxis(Vector3 axis, float radians) => FromQuaternion(VecMath.FromAxisAngle(axis, radians));

        // Skew(v).Transform(u) == Cross(v, u)
        public static Matrix3x3 Skew(Vector3 v) => new(
            0f, v.z, -v.y,
            -v.z, 0f, v.x,
            v.y, -v.x, 0f);

        public static Matrix3x3 OuterProduct(Vector3 a, Vector3 b) => new(
            a.x * b.x, a.x * b.y, a.x * b.z,
            a.y * b.x, a.y * b.y, a.y * b.z,
            a.z * b.x, a.z * b.y, a.z * b.z);
    }
}
