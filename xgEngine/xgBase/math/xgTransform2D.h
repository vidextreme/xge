// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.

#pragma once

// xgTransform2D.h - 2D affine transforms for 2D games and UI
// Mat3x2      : row-major 3x2 affine matrix, row-vector convention:
//               (x, y, 1) * | m11 m12 |
//                           | m21 m22 |
//                           | m31 m32 |   <- translation row
// Transform2D : position + rotation (radians, CCW) + scale.

#include "xgMathCommon.h"
#include "xgVec2.h"
#include "xgMotion.h"

namespace xg
{
    //============================================================
    // Mat3x2
    //============================================================
    struct Mat3x2
    {
        float m11 = 1.0f, m12 = 0.0f;
        float m21 = 0.0f, m22 = 1.0f;
        float m31 = 0.0f, m32 = 0.0f;

        //--------------------------------------------------------
        // Constructors
        //--------------------------------------------------------
        constexpr Mat3x2() noexcept = default;

        constexpr Mat3x2(float m11_, float m12_, float m21_, float m22_, float m31_, float m32_) noexcept
            : m11(m11_), m12(m12_), m21(m21_), m22(m22_), m31(m31_), m32(m32_) {}

        constexpr bool operator==(const Mat3x2& o) const noexcept
        {
            return m11 == o.m11 && m12 == o.m12 && m21 == o.m21 && m22 == o.m22 && m31 == o.m31 && m32 == o.m32;
        }
        constexpr bool operator!=(const Mat3x2& o) const noexcept { return !(*this == o); }

        constexpr bool IsApproxEqual(const Mat3x2& o, float epsilon = EPSILON_POS) const noexcept
        {
            return IsEqual(m11, o.m11, epsilon) && IsEqual(m12, o.m12, epsilon) &&
                   IsEqual(m21, o.m21, epsilon) && IsEqual(m22, o.m22, epsilon) &&
                   IsEqual(m31, o.m31, epsilon) && IsEqual(m32, o.m32, epsilon);
        }

        //--------------------------------------------------------
        // Operations
        //--------------------------------------------------------

        // a then b: p * (a * b) == (p * a) * b
        static constexpr Mat3x2 Multiply(const Mat3x2& a, const Mat3x2& b) noexcept
        {
            return Mat3x2(
                a.m11 * b.m11 + a.m12 * b.m21,
                a.m11 * b.m12 + a.m12 * b.m22,
                a.m21 * b.m11 + a.m22 * b.m21,
                a.m21 * b.m12 + a.m22 * b.m22,
                a.m31 * b.m11 + a.m32 * b.m21 + b.m31,
                a.m31 * b.m12 + a.m32 * b.m22 + b.m32);
        }

        constexpr Mat3x2 operator*(const Mat3x2& o) const noexcept { return Multiply(*this, o); }

        constexpr float Determinant() const noexcept { return m11 * m22 - m12 * m21; }

        constexpr bool Inverse(Mat3x2& out) const noexcept
        {
            float det = Determinant();
            if (Abs(det) < 1e-12f)
                return false;
            float inv = 1.0f / det;
            float i11 = m22 * inv;
            float i12 = -m12 * inv;
            float i21 = -m21 * inv;
            float i22 = m11 * inv;
            out = Mat3x2(i11, i12, i21, i22,
                -(m31 * i11 + m32 * i21),
                -(m31 * i12 + m32 * i22));
            return true;
        }

        // Returns identity if singular
        constexpr Mat3x2 Inverse() const noexcept
        {
            Mat3x2 r;
            Inverse(r);
            return r;
        }

        constexpr Vec2 TransformPoint(const Vec2& p) const noexcept
        {
            return Vec2(p.x * m11 + p.y * m21 + m31, p.x * m12 + p.y * m22 + m32);
        }

        // Ignores translation
        constexpr Vec2 TransformVector(const Vec2& v) const noexcept
        {
            return Vec2(v.x * m11 + v.y * m21, v.x * m12 + v.y * m22);
        }

        constexpr Vec2 GetTranslation() const noexcept { return Vec2(m31, m32); }

        // Splits into scale (sign of det goes to y), rotation (radians) and translation
        void Decompose(Vec2& scale, float& rotation, Vec2& translation) const noexcept
        {
            translation = Vec2(m31, m32);
            float sx = sqrtf(m11 * m11 + m12 * m12);
            float sy = sqrtf(m21 * m21 + m22 * m22);
            if (Determinant() < 0.0f)
                sy = -sy;
            scale = Vec2(sx, sy);
            rotation = atan2f(m12, m11);
        }

        //--------------------------------------------------------
        // Factories
        //--------------------------------------------------------
        static constexpr Mat3x2 Identity() noexcept { return Mat3x2(); }

        static constexpr Mat3x2 Translation(const Vec2& t) noexcept
        {
            return Mat3x2(1.0f, 0.0f, 0.0f, 1.0f, t.x, t.y);
        }

        static constexpr Mat3x2 Scaling(const Vec2& s) noexcept
        {
            return Mat3x2(s.x, 0.0f, 0.0f, s.y, 0.0f, 0.0f);
        }

        // Counter-clockwise rotation (radians)
        static Mat3x2 Rotation(float radians) noexcept
        {
            float c = cosf(radians);
            float s = sinf(radians);
            return Mat3x2(c, s, -s, c, 0.0f, 0.0f);
        }

        // Scale, then rotate, then translate
        static Mat3x2 TRS(const Vec2& translation, float radians, const Vec2& scale) noexcept
        {
            float c = cosf(radians);
            float s = sinf(radians);
            return Mat3x2(
                scale.x * c, scale.x * s,
                -scale.y * s, scale.y * c,
                translation.x, translation.y);
        }
    };

    static_assert(std::is_standard_layout_v<Mat3x2>, "Mat3x2 must be standard layout");
    static_assert(std::is_trivially_copyable_v<Mat3x2>, "Mat3x2 must be trivially copyable");
    static_assert(sizeof(Mat3x2) == 6 * sizeof(float), "Mat3x2 must be 6 floats");

    //============================================================
    // Transform2D
    //============================================================
    struct Transform2D
    {
        Vec2 Position;
        float Rotation = 0.0f; // radians, counter-clockwise
        Vec2 Scale = Vec2(1.0f, 1.0f);

        constexpr Transform2D() noexcept = default;
        constexpr Transform2D(const Vec2& position, float rotation = 0.0f, const Vec2& scale = Vec2(1.0f, 1.0f)) noexcept
            : Position(position), Rotation(rotation), Scale(scale) {}

        Mat3x2 ToMatrix() const noexcept
        {
            return Mat3x2::TRS(Position, Rotation, Scale);
        }

        Mat3x2 ToInverseMatrix() const noexcept
        {
            return ToMatrix().Inverse();
        }

        Vec2 TransformPoint(const Vec2& p) const noexcept
        {
            return ToMatrix().TransformPoint(p);
        }

        Vec2 InverseTransformPoint(const Vec2& p) const noexcept
        {
            return ToInverseMatrix().TransformPoint(p);
        }

        Vec2 TransformVector(const Vec2& v) const noexcept
        {
            return ToMatrix().TransformVector(v);
        }

        // Child-to-world: (this * child) applies child first
        Transform2D Combine(const Transform2D& child) const noexcept
        {
            Transform2D r;
            Mat3x2 world = Mat3x2::Multiply(child.ToMatrix(), ToMatrix());
            world.Decompose(r.Scale, r.Rotation, r.Position);
            return r;
        }

        // Rotation interpolates along the shortest arc
        static Transform2D Lerp(const Transform2D& a, const Transform2D& b, float t) noexcept
        {
            return Transform2D(
                Vec2::Lerp(a.Position, b.Position, t),
                LerpAngle(a.Rotation, b.Rotation, t),
                Vec2::Lerp(a.Scale, b.Scale, t));
        }

        static constexpr Transform2D Identity() noexcept { return Transform2D(); }
    };

    static_assert(std::is_trivially_copyable_v<Transform2D>, "Transform2D must be trivially copyable");
    static_assert(sizeof(Transform2D) == 5 * sizeof(float), "Transform2D must be 5 floats");

    constexpr Vec2 operator*(const Vec2& p, const Mat3x2& m) noexcept { return m.TransformPoint(p); }
} // namespace xg
