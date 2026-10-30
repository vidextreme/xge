// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.

using System.Runtime.InteropServices;

// Managed mirror of xgBase/math/xgTransform2D.h (pure C#, same layouts as xg::Mat3x2 / xg::Transform2D).
// Row-vector convention: (x, y, 1) * M, translation in the third row. Rotation is radians, CCW.

namespace xg.math
{
    [StructLayout(LayoutKind.Sequential)]
    public struct Matrix3x2
    {
        public float m11, m12;
        public float m21, m22;
        public float m31, m32;

        public Matrix3x2(float m11, float m12, float m21, float m22, float m31, float m32)
        {
            this.m11 = m11; this.m12 = m12;
            this.m21 = m21; this.m22 = m22;
            this.m31 = m31; this.m32 = m32;
        }

        public override string ToString() => $"Matrix3x2([{m11:F4}, {m12:F4}], [{m21:F4}, {m22:F4}], [{m31:F4}, {m32:F4}])";

        // a then b: p * (a * b) == (p * a) * b
        public static Matrix3x2 Multiply(in Matrix3x2 a, in Matrix3x2 b) => new(
            a.m11 * b.m11 + a.m12 * b.m21,
            a.m11 * b.m12 + a.m12 * b.m22,
            a.m21 * b.m11 + a.m22 * b.m21,
            a.m21 * b.m12 + a.m22 * b.m22,
            a.m31 * b.m11 + a.m32 * b.m21 + b.m31,
            a.m31 * b.m12 + a.m32 * b.m22 + b.m32);

        public static Matrix3x2 operator *(Matrix3x2 a, Matrix3x2 b) => Multiply(a, b);
        public static Vector2 operator *(Vector2 p, Matrix3x2 m) => m.TransformPoint(p);

        public float Determinant() => m11 * m22 - m12 * m21;

        public bool TryInverse(out Matrix3x2 result)
        {
            float det = Determinant();
            if (MathF.Abs(det) < 1e-12f)
            {
                result = Identity();
                return false;
            }
            float inv = 1.0f / det;
            float i11 = m22 * inv;
            float i12 = -m12 * inv;
            float i21 = -m21 * inv;
            float i22 = m11 * inv;
            result = new Matrix3x2(i11, i12, i21, i22, -(m31 * i11 + m32 * i21), -(m31 * i12 + m32 * i22));
            return true;
        }

        // Identity if singular
        public Matrix3x2 Inverse()
        {
            TryInverse(out Matrix3x2 r);
            return r;
        }

        public Vector2 TransformPoint(Vector2 p) => new(p.x * m11 + p.y * m21 + m31, p.x * m12 + p.y * m22 + m32);
        public Vector2 TransformVector(Vector2 v) => new(v.x * m11 + v.y * m21, v.x * m12 + v.y * m22);
        public Vector2 GetTranslation() => new(m31, m32);

        public void Decompose(out Vector2 scale, out float rotation, out Vector2 translation)
        {
            translation = new Vector2(m31, m32);
            float sx = MathF.Sqrt(m11 * m11 + m12 * m12);
            float sy = MathF.Sqrt(m21 * m21 + m22 * m22);
            if (Determinant() < 0.0f)
                sy = -sy;
            scale = new Vector2(sx, sy);
            rotation = MathF.Atan2(m12, m11);
        }

        public static Matrix3x2 Identity() => new(1f, 0f, 0f, 1f, 0f, 0f);
        public static Matrix3x2 Translation(Vector2 t) => new(1f, 0f, 0f, 1f, t.x, t.y);
        public static Matrix3x2 Scaling(Vector2 s) => new(s.x, 0f, 0f, s.y, 0f, 0f);

        public static Matrix3x2 Rotation(float radians)
        {
            float c = MathF.Cos(radians);
            float s = MathF.Sin(radians);
            return new Matrix3x2(c, s, -s, c, 0f, 0f);
        }

        // Scale, then rotate, then translate
        public static Matrix3x2 TRS(Vector2 translation, float radians, Vector2 scale)
        {
            float c = MathF.Cos(radians);
            float s = MathF.Sin(radians);
            return new Matrix3x2(scale.x * c, scale.x * s, -scale.y * s, scale.y * c, translation.x, translation.y);
        }
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct Transform2D
    {
        public Vector2 Position;
        public float Rotation; // radians, counter-clockwise
        public Vector2 Scale;

        public Transform2D(Vector2 position, float rotation = 0.0f)
            : this(position, rotation, new Vector2(1f, 1f)) { }

        public Transform2D(Vector2 position, float rotation, Vector2 scale)
        {
            Position = position;
            Rotation = rotation;
            Scale = scale;
        }

        public static Transform2D Identity() => new(new Vector2(0f, 0f), 0f, new Vector2(1f, 1f));

        public Matrix3x2 ToMatrix() => Matrix3x2.TRS(Position, Rotation, Scale);
        public Matrix3x2 ToInverseMatrix() => ToMatrix().Inverse();
        public Vector2 TransformPoint(Vector2 p) => ToMatrix().TransformPoint(p);
        public Vector2 InverseTransformPoint(Vector2 p) => ToInverseMatrix().TransformPoint(p);
        public Vector2 TransformVector(Vector2 v) => ToMatrix().TransformVector(v);

        // Child-to-world: applies child first
        public Transform2D Combine(Transform2D child)
        {
            Matrix3x2 world = Matrix3x2.Multiply(child.ToMatrix(), ToMatrix());
            world.Decompose(out Vector2 scale, out float rotation, out Vector2 position);
            return new Transform2D(position, rotation, scale);
        }

        // Rotation interpolates along the shortest arc
        public static Transform2D Lerp(Transform2D a, Transform2D b, float t)
            => new(VecMath.Lerp(a.Position, b.Position, t), Motion.LerpAngle(a.Rotation, b.Rotation, t), VecMath.Lerp(a.Scale, b.Scale, t));
    }
}
