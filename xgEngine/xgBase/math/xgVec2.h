// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.

#pragma once

// xgVec2.h - 2D vector type
// POD-friendly, trivially copyable, STL-free

#include "xgMathCommon.h"

namespace xg
{
    struct Vec2
    {
        union
        {
            struct { float x, y; };
            struct { float u, v; };
            struct { float r, g; };
            float data[2];
        };

        // Default constructor - zero initialized
        constexpr Vec2() noexcept : x(0.0f), y(0.0f) {}

        // Explicit constructor
        constexpr Vec2(float x_, float y_) noexcept : x(x_), y(y_) {}

        // Uniform constructor
        constexpr explicit Vec2(float v) noexcept : x(v), y(v) {}

        // Copy/move = default (trivially copyable)
        constexpr Vec2(const Vec2&) noexcept = default;
        constexpr Vec2(Vec2&&) noexcept = default;
        constexpr Vec2& operator=(const Vec2&) noexcept = default;
        constexpr Vec2& operator=(Vec2&&) noexcept = default;

        // Component access
        constexpr float operator[](size_t i) const noexcept
        {
            return data[i];
        }

        constexpr float& operator[](size_t i) noexcept
        {
            return data[i];
        }

        // Equality
        constexpr bool operator==(const Vec2& other) const noexcept
        {
            return IsEqual(x, other.x) && IsEqual(y, other.y);
        }

        constexpr bool operator!=(const Vec2& other) const noexcept
        {
            return !(*this == other);
        }

        // Negation
        constexpr Vec2 operator-() const noexcept
        {
            return Vec2(-x, -y);
        }

        // Addition
        constexpr Vec2 operator+(const Vec2& other) const noexcept
        {
            return Vec2(x + other.x, y + other.y);
        }

        constexpr Vec2& operator+=(const Vec2& other) noexcept
        {
            x += other.x; y += other.y; return *this;
        }

        // Subtraction
        constexpr Vec2 operator-(const Vec2& other) const noexcept
        {
            return Vec2(x - other.x, y - other.y);
        }

        constexpr Vec2& operator-=(const Vec2& other) noexcept
        {
            x -= other.x; y -= other.y; return *this;
        }

        // Multiplication (component-wise)
        constexpr Vec2 operator*(const Vec2& other) const noexcept
        {
            return Vec2(x * other.x, y * other.y);
        }

        constexpr Vec2& operator*=(const Vec2& other) noexcept
        {
            x *= other.x; y *= other.y; return *this;
        }

        // Division (component-wise)
        constexpr Vec2 operator/(const Vec2& other) const noexcept
        {
            return Vec2(x / other.x, y / other.y);
        }

        constexpr Vec2& operator/=(const Vec2& other) noexcept
        {
            x /= other.x; y /= other.y; return *this;
        }

        // Scalar multiplication
        constexpr Vec2 operator*(float scalar) const noexcept
        {
            return Vec2(x * scalar, y * scalar);
        }

        constexpr Vec2& operator*=(float scalar) noexcept
        {
            x *= scalar; y *= scalar; return *this;
        }

        // Scalar division
        constexpr Vec2 operator/(float scalar) const noexcept
        {
            float inv = 1.0f / scalar;
            return Vec2(x * inv, y * inv);
        }

        constexpr Vec2& operator/=(float scalar) noexcept
        {
            float inv = 1.0f / scalar;
            x *= inv; y *= inv; return *this;
        }

        // Scalar multiplication (float * Vec2)
        friend constexpr Vec2 operator*(float scalar, const Vec2& v) noexcept
        {
            return Vec2(v.x * scalar, v.y * scalar);
        }

        // Length
        constexpr float LengthSq() const noexcept
        {
            return x * x + y * y;
        }

        float Length() const noexcept
        {
            return sqrtf(LengthSq());
        }

        // Normalize
        Vec2 Normalized() const noexcept
        {
            float len = Length();
            if (len > EPSILON_POS)
                return *this / len;
            return Vec2(0.0f, 0.0f);
        }

        void Normalize() noexcept
        {
            *this = Normalized();
        }

        // Dot product
        constexpr float Dot(const Vec2& other) const noexcept
        {
            return x * other.x + y * other.y;
        }

        // Cross product (2D cross = scalar)
        constexpr float Cross(const Vec2& other) const noexcept
        {
            return x * other.y - y * other.x;
        }

        // Distance
        constexpr float DistanceSq(const Vec2& other) const noexcept
        {
            float dx = x - other.x;
            float dy = y - other.y;
            return dx * dx + dy * dy;
        }

        float Distance(const Vec2& other) const noexcept
        {
            return sqrtf(DistanceSq(other));
        }

        // Lerp
        static constexpr Vec2 Lerp(const Vec2& a, const Vec2& b, float t) noexcept
        {
            return Vec2(xg::Lerp(a.x, b.x, t), xg::Lerp(a.y, b.y, t));
        }

        static Vec2 LerpClamped(const Vec2& a, const Vec2& b, float t) noexcept
        {
            t = xg::Clamp(t, 0.0f, 1.0f);
            return Lerp(a, b, t);
        }

        // Min/Max (component-wise)
        static constexpr Vec2 Min(const Vec2& a, const Vec2& b) noexcept
        {
            return Vec2(xg::Min(a.x, b.x), xg::Min(a.y, b.y));
        }

        static constexpr Vec2 Max(const Vec2& a, const Vec2& b) noexcept
        {
            return Vec2(xg::Max(a.x, b.x), xg::Max(a.y, b.y));
        }

        // Clamp
        static constexpr Vec2 Clamp(const Vec2& v, const Vec2& min, const Vec2& max) noexcept
        {
            return Vec2(xg::Clamp(v.x, min.x, max.x), xg::Clamp(v.y, min.y, max.y));
        }

        // Abs
        static constexpr Vec2 Abs(const Vec2& v) noexcept
        {
            return Vec2(xg::Abs(v.x), xg::Abs(v.y));
        }

        // Sign
        static constexpr Vec2 Sign(const Vec2& v) noexcept
        {
            return Vec2(xg::Sign(v.x), xg::Sign(v.y));
        }

        // Floor/Ceil/Round
        static constexpr Vec2 Floor(const Vec2& v) noexcept
        {
            return Vec2(floorf(v.x), floorf(v.y));
        }

        static constexpr Vec2 Ceil(const Vec2& v) noexcept
        {
            return Vec2(ceilf(v.x), ceilf(v.y));
        }

        static constexpr Vec2 Round(const Vec2& v) noexcept
        {
            return Vec2(roundf(v.x), roundf(v.y));
        }

        // Perpendicular (90 degree CCW rotation)
        constexpr Vec2 Perpendicular() const noexcept
        {
            return Vec2(-y, x);
        }

        // Perpendicular CW
        constexpr Vec2 PerpendicularCW() const noexcept
        {
            return Vec2(y, -x);
        }

        // Rotate by angle (radians)
        Vec2 Rotate(float angle) const noexcept
        {
            float c = cosf(angle);
            float s = sinf(angle);
            return Vec2(x * c - y * s, x * s + y * c);
        }

        // Reflect
        static Vec2 Reflect(const Vec2& v, const Vec2& normal) noexcept
        {
            float dot = v.Dot(normal);
            return v - normal * (2.0f * dot);
        }

        // Projection
        static Vec2 Project(const Vec2& v, const Vec2& on) noexcept
        {
            float dot = v.Dot(on);
            float onLenSq = on.LengthSq();
            if (onLenSq > EPSILON_POS)
                return on * (dot / onLenSq);
            return Vec2(0.0f, 0.0f);
        }

        // Rejection
        static Vec2 Reject(const Vec2& v, const Vec2& on) noexcept
        {
            return v - Project(v, on);
        }

        // Common vectors
        static constexpr Vec2 Zero() noexcept { return Vec2(0.0f, 0.0f); }
        static constexpr Vec2 One() noexcept { return Vec2(1.0f, 1.0f); }
        static constexpr Vec2 UnitX() noexcept { return Vec2(1.0f, 0.0f); }
        static constexpr Vec2 UnitY() noexcept { return Vec2(0.0f, 1.0f); }
        static constexpr Vec2 Left() noexcept { return Vec2(-1.0f, 0.0f); }
        static constexpr Vec2 Right() noexcept { return Vec2(1.0f, 0.0f); }
        static constexpr Vec2 Up() noexcept { return Vec2(0.0f, 1.0f); }
        static constexpr Vec2 Down() noexcept { return Vec2(0.0f, -1.0f); }

        // DirectXMath interop
        detail::XGVector ToXM() const noexcept
        {
            return detail::Vec2ToXM(*this);
        }

        static Vec2 FromXM(detail::XGVector v) noexcept
        {
            Vec2 result;
            detail::XMToVec2(v, result);
            return result;
        }
    };

    // Static assertions for POD/trivial properties
    //static_assert(std::is_trivial_v<Vec2>, "Vec2 must be trivial");
    static_assert(std::is_standard_layout_v<Vec2>, "Vec2 must be standard layout");
    static_assert(sizeof(Vec2) == 2 * sizeof(float), "Vec2 must be 2 floats");
    static_assert(std::is_trivially_copyable_v<Vec2>, "Vec2 must be trivially copyable");
} // namespace xg

// Non-member operators
namespace xg
{
    // Lerp free function
    constexpr Vec2 Lerp(const Vec2& a, const Vec2& b, float t) noexcept
    {
        return Vec2::Lerp(a, b, t);
    }

    Vec2 LerpClamped(const Vec2& a, const Vec2& b, float t) noexcept
    {
        return Vec2::LerpClamped(a, b, t);
    }

    // Distance free functions
    constexpr float DistanceSq(const Vec2& a, const Vec2& b) noexcept
    {
        return a.DistanceSq(b);
    }

    float Distance(const Vec2& a, const Vec2& b) noexcept
    {
        return a.Distance(b);
    }

    // Dot free function
    constexpr float Dot(const Vec2& a, const Vec2& b) noexcept
    {
        return a.Dot(b);
    }

    // Cross free function
    constexpr float Cross(const Vec2& a, const Vec2& b) noexcept
    {
        return a.Cross(b);
    }
} // namespace xg