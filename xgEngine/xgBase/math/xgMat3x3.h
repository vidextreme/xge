// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.

#pragma once

// xgMat3x3.h - 3x3 matrix (rotations, normal matrices, inertia tensors)
// Row-major storage, row-vector convention (v' = v * M), same as Mat4x4.
// POD-friendly, trivially copyable, STL-free. 36 bytes, unaligned.

#include "xgMathCommon.h"
#include "xgVec3.h"
#include "xgQuat.h"
#include "xgMat4x4.h"

namespace xg
{
    //============================================================
    // Mat3x3
    //============================================================
    struct Mat3x3
    {
        float m[3][3] = {
            { 1.0f, 0.0f, 0.0f },
            { 0.0f, 1.0f, 0.0f },
            { 0.0f, 0.0f, 1.0f }
        };

        //--------------------------------------------------------
        // Constructors
        //--------------------------------------------------------
        constexpr Mat3x3() noexcept = default;

        constexpr Mat3x3(
            float m00, float m01, float m02,
            float m10, float m11, float m12,
            float m20, float m21, float m22) noexcept
            : m{ { m00, m01, m02 }, { m10, m11, m12 }, { m20, m21, m22 } } {}

        // From three rows
        constexpr Mat3x3(const Vec3& r0, const Vec3& r1, const Vec3& r2) noexcept
            : m{ { r0.x, r0.y, r0.z }, { r1.x, r1.y, r1.z }, { r2.x, r2.y, r2.z } } {}

        //--------------------------------------------------------
        // Access
        //--------------------------------------------------------
        constexpr Vec3 Row(int i) const noexcept { return Vec3(m[i][0], m[i][1], m[i][2]); }
        constexpr Vec3 Column(int j) const noexcept { return Vec3(m[0][j], m[1][j], m[2][j]); }

        constexpr bool operator==(const Mat3x3& o) const noexcept
        {
            for (int i = 0; i < 3; ++i)
                for (int j = 0; j < 3; ++j)
                    if (m[i][j] != o.m[i][j])
                        return false;
            return true;
        }
        constexpr bool operator!=(const Mat3x3& o) const noexcept { return !(*this == o); }

        constexpr bool IsApproxEqual(const Mat3x3& o, float epsilon = EPSILON_POS) const noexcept
        {
            for (int i = 0; i < 3; ++i)
                for (int j = 0; j < 3; ++j)
                    if (!IsEqual(m[i][j], o.m[i][j], epsilon))
                        return false;
            return true;
        }

        //--------------------------------------------------------
        // Arithmetic
        //--------------------------------------------------------
        constexpr Mat3x3 operator*(const Mat3x3& o) const noexcept { return Multiply(*this, o); }

        constexpr Mat3x3 operator*(float s) const noexcept
        {
            Mat3x3 r;
            for (int i = 0; i < 3; ++i)
                for (int j = 0; j < 3; ++j)
                    r.m[i][j] = m[i][j] * s;
            return r;
        }

        constexpr Mat3x3 operator+(const Mat3x3& o) const noexcept
        {
            Mat3x3 r;
            for (int i = 0; i < 3; ++i)
                for (int j = 0; j < 3; ++j)
                    r.m[i][j] = m[i][j] + o.m[i][j];
            return r;
        }

        // a then b (row vectors): v * (a * b) == (v * a) * b
        static constexpr Mat3x3 Multiply(const Mat3x3& a, const Mat3x3& b) noexcept
        {
            Mat3x3 r;
            for (int i = 0; i < 3; ++i)
                for (int j = 0; j < 3; ++j)
                    r.m[i][j] = a.m[i][0] * b.m[0][j] + a.m[i][1] * b.m[1][j] + a.m[i][2] * b.m[2][j];
            return r;
        }

        // v * M
        constexpr Vec3 Transform(const Vec3& v) const noexcept
        {
            return Vec3(
                v.x * m[0][0] + v.y * m[1][0] + v.z * m[2][0],
                v.x * m[0][1] + v.y * m[1][1] + v.z * m[2][1],
                v.x * m[0][2] + v.y * m[1][2] + v.z * m[2][2]);
        }

        //--------------------------------------------------------
        // Matrix operations
        //--------------------------------------------------------
        constexpr Mat3x3 Transposed() const noexcept
        {
            return Mat3x3(
                m[0][0], m[1][0], m[2][0],
                m[0][1], m[1][1], m[2][1],
                m[0][2], m[1][2], m[2][2]);
        }

        constexpr float Determinant() const noexcept
        {
            return m[0][0] * (m[1][1] * m[2][2] - m[1][2] * m[2][1])
                 - m[0][1] * (m[1][0] * m[2][2] - m[1][2] * m[2][0])
                 + m[0][2] * (m[1][0] * m[2][1] - m[1][1] * m[2][0]);
        }

        // Returns false (and leaves out untouched) if the matrix is singular
        constexpr bool Inverse(Mat3x3& out) const noexcept
        {
            float det = Determinant();
            if (Abs(det) < 1e-12f)
                return false;
            float inv = 1.0f / det;
            out = Mat3x3(
                (m[1][1] * m[2][2] - m[1][2] * m[2][1]) * inv,
                (m[0][2] * m[2][1] - m[0][1] * m[2][2]) * inv,
                (m[0][1] * m[1][2] - m[0][2] * m[1][1]) * inv,
                (m[1][2] * m[2][0] - m[1][0] * m[2][2]) * inv,
                (m[0][0] * m[2][2] - m[0][2] * m[2][0]) * inv,
                (m[0][2] * m[1][0] - m[0][0] * m[1][2]) * inv,
                (m[1][0] * m[2][1] - m[1][1] * m[2][0]) * inv,
                (m[0][1] * m[2][0] - m[0][0] * m[2][1]) * inv,
                (m[0][0] * m[1][1] - m[0][1] * m[1][0]) * inv);
            return true;
        }

        // Returns identity if singular
        constexpr Mat3x3 Inverse() const noexcept
        {
            Mat3x3 r;
            Inverse(r);
            return r;
        }

        //--------------------------------------------------------
        // Rotation conversion
        //--------------------------------------------------------

        // Same convention as Mat4x4::RotationQuaternion: v * FromQuat(q) == q.RotateVector(v)
        static constexpr Mat3x3 FromQuat(const Quat& q) noexcept
        {
            float xx = q.x * q.x, yy = q.y * q.y, zz = q.z * q.z;
            float xy = q.x * q.y, xz = q.x * q.z, yz = q.y * q.z;
            float wx = q.w * q.x, wy = q.w * q.y, wz = q.w * q.z;
            return Mat3x3(
                1.0f - 2.0f * (yy + zz), 2.0f * (xy + wz),        2.0f * (xz - wy),
                2.0f * (xy - wz),        1.0f - 2.0f * (xx + zz), 2.0f * (yz + wx),
                2.0f * (xz + wy),        2.0f * (yz - wx),        1.0f - 2.0f * (xx + yy));
        }

        // Assumes a pure rotation (orthonormal) matrix
        Quat ToQuat() const noexcept
        {
            float trace = m[0][0] + m[1][1] + m[2][2];
            Quat q;
            if (trace > 0.0f)
            {
                float s = sqrtf(trace + 1.0f) * 2.0f;
                q = Quat((m[1][2] - m[2][1]) / s, (m[2][0] - m[0][2]) / s, (m[0][1] - m[1][0]) / s, 0.25f * s);
            }
            else if (m[0][0] > m[1][1] && m[0][0] > m[2][2])
            {
                float s = sqrtf(1.0f + m[0][0] - m[1][1] - m[2][2]) * 2.0f;
                q = Quat(0.25f * s, (m[0][1] + m[1][0]) / s, (m[0][2] + m[2][0]) / s, (m[1][2] - m[2][1]) / s);
            }
            else if (m[1][1] > m[2][2])
            {
                float s = sqrtf(1.0f + m[1][1] - m[0][0] - m[2][2]) * 2.0f;
                q = Quat((m[0][1] + m[1][0]) / s, 0.25f * s, (m[1][2] + m[2][1]) / s, (m[2][0] - m[0][2]) / s);
            }
            else
            {
                float s = sqrtf(1.0f + m[2][2] - m[0][0] - m[1][1]) * 2.0f;
                q = Quat((m[0][2] + m[2][0]) / s, (m[1][2] + m[2][1]) / s, 0.25f * s, (m[0][1] - m[1][0]) / s);
            }
            return q.Normalized();
        }

        // Upper-left 3x3 of a 4x4 (rotation + scale, no translation)
        static constexpr Mat3x3 FromMat4x4(const Mat4x4& mat) noexcept
        {
            return Mat3x3(
                mat.m[0][0], mat.m[0][1], mat.m[0][2],
                mat.m[1][0], mat.m[1][1], mat.m[1][2],
                mat.m[2][0], mat.m[2][1], mat.m[2][2]);
        }

        // Inverse-transpose of the upper 3x3; transforms normals correctly under non-uniform scale
        static constexpr Mat3x3 NormalMatrix(const Mat4x4& world) noexcept
        {
            return FromMat4x4(world).Inverse().Transposed();
        }

        //--------------------------------------------------------
        // Factories
        //--------------------------------------------------------
        static constexpr Mat3x3 Identity() noexcept { return Mat3x3(); }

        static constexpr Mat3x3 Zero() noexcept
        {
            return Mat3x3(0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);
        }

        static constexpr Mat3x3 Scaling(const Vec3& s) noexcept
        {
            return Mat3x3(s.x, 0.0f, 0.0f, 0.0f, s.y, 0.0f, 0.0f, 0.0f, s.z);
        }

        static Mat3x3 RotationAxis(const Vec3& axis, float radians) noexcept
        {
            return FromQuat(Quat::FromAxisAngle(axis, radians));
        }

        // Cross-product matrix: Skew(v).Transform(u) == Cross(v, u)
        static constexpr Mat3x3 Skew(const Vec3& v) noexcept
        {
            return Mat3x3(
                0.0f,  v.z, -v.y,
                -v.z, 0.0f,  v.x,
                v.y,  -v.x, 0.0f);
        }

        // M[i][j] = a[i] * b[j]
        static constexpr Mat3x3 OuterProduct(const Vec3& a, const Vec3& b) noexcept
        {
            return Mat3x3(
                a.x * b.x, a.x * b.y, a.x * b.z,
                a.y * b.x, a.y * b.y, a.y * b.z,
                a.z * b.x, a.z * b.y, a.z * b.z);
        }
    };

    static_assert(std::is_standard_layout_v<Mat3x3>, "Mat3x3 must be standard layout");
    static_assert(std::is_trivially_copyable_v<Mat3x3>, "Mat3x3 must be trivially copyable");
    static_assert(sizeof(Mat3x3) == 9 * sizeof(float), "Mat3x3 must be 9 floats");

    constexpr Vec3 operator*(const Vec3& v, const Mat3x3& m) noexcept { return m.Transform(v); }
} // namespace xg
