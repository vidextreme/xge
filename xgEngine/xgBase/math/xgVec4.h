// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.

#pragma once

// xgVec4.h - 4D vector type
// POD-friendly, trivially copyable, STL-free

#include "xgMathCommon.h"

namespace xg
{
    // Forward declarations
    struct Vec2;
    struct Vec3;
    struct Vec4;
    struct Quat;
    struct Mat4x4;

    //============================================================
    // Vec4 - 4D Vector
    //============================================================
    struct alignas(16) Vec4
    {
        float x = 0.0f;
        float y = 0.0f;
        float z = 0.0f;
        float w = 0.0f;

        //--------------------------------------------------------
        // Constructors
        //--------------------------------------------------------
        constexpr Vec4() noexcept = default;

        constexpr Vec4(float x_, float y_, float z_, float w_) noexcept
            : x(x_), y(y_), z(z_), w(w_) {}

        constexpr explicit Vec4(float v) noexcept
            : x(v), y(v), z(v), w(v) {}

        constexpr Vec4(const Vec3& xyz, float w_) noexcept
            : x(xyz.x), y(xyz.y), z(xyz.z), w(w_) {}

        constexpr Vec4(const Vec2& xy, float z_, float w_) noexcept
            : x(xy.x), y(xy.y), z(z_), w(w_) {}

        constexpr Vec4(const Vec4&) noexcept = default;
        constexpr Vec4(Vec4&&) noexcept = default;
        constexpr Vec4& operator=(const Vec4&) noexcept = default;
        constexpr Vec4& operator=(Vec4&&) noexcept = default;

        // Construct from Quat
        constexpr explicit Vec4(const Quat& q) noexcept;

        //--------------------------------------------------------
        // Access operators
        //--------------------------------------------------------
        constexpr float& operator[](size_t i) noexcept
        {
            return (&x)[i];
        }

        constexpr const float& operator[](size_t i) const noexcept
        {
            return (&x)[i];
        }

        //--------------------------------------------------------
        // Comparison operators
        //--------------------------------------------------------
        constexpr bool operator==(const Vec4& other) const noexcept
        {
            return x == other.x && y == other.y && z == other.z && w == other.w;
        }

        constexpr bool operator!=(const Vec4& other) const noexcept
        {
            return !(*this == other);
        }

        //--------------------------------------------------------
        // Arithmetic operators
        //--------------------------------------------------------
        constexpr Vec4 operator+(const Vec4& other) const noexcept
        {
            return Vec4(x + other.x, y + other.y, z + other.z, w + other.w);
        }

        constexpr Vec4 operator-(const Vec4& other) const noexcept
        {
            return Vec4(x - other.x, y - other.y, z - other.z, w - other.w);
        }

        constexpr Vec4 operator*(const Vec4& other) const noexcept
        {
            return Vec4(x * other.x, y * other.y, z * other.z, w * other.w);
        }

        constexpr Vec4 operator/(const Vec4& other) const noexcept
        {
            return Vec4(x / other.x, y / other.y, z / other.z, w / other.w);
        }

        constexpr Vec4 operator*(float scalar) const noexcept
        {
            return Vec4(x * scalar, y * scalar, z * scalar, w * scalar);
        }

        constexpr Vec4 operator/(float scalar) const noexcept
        {
            float inv = 1.0f / scalar;
            return Vec4(x * inv, y * inv, z * inv, w * inv);
        }

        constexpr Vec4& operator+=(const Vec4& other) noexcept
        {
            x += other.x; y += other.y; z += other.z; w += other.w; return *this;
        }

        constexpr Vec4& operator-=(const Vec4& other) noexcept
        {
            x -= other.x; y -= other.y; z -= other.z; w -= other.w; return *this;
        }

        constexpr Vec4& operator*=(const Vec4& other) noexcept
        {
            x *= other.x; y *= other.y; z *= other.z; w *= other.w; return *this;
        }

        constexpr Vec4& operator/=(const Vec4& other) noexcept
        {
            x /= other.x; y /= other.y; z /= other.z; w /= other.w; return *this;
        }

        constexpr Vec4& operator*=(float scalar) noexcept
        {
            x *= scalar; y *= scalar; z *= scalar; w *= scalar; return *this;
        }

        constexpr Vec4& operator/=(float scalar) noexcept
        {
            float inv = 1.0f / scalar;
            x *= inv; y *= inv; z *= inv; w *= inv; return *this;
        }

        //--------------------------------------------------------
        // Unary operators
        //--------------------------------------------------------
        constexpr Vec4 operator+() const noexcept { return *this; }
        constexpr Vec4 operator-() const noexcept { return Vec4(-x, -y, -z, -w); }

        //--------------------------------------------------------
        // Vector operations
        //--------------------------------------------------------
        constexpr float LengthSq() const noexcept
        {
            return x * x + y * y + z * z + w * w;
        }

        float Length() const noexcept
        {
            return sqrtf(LengthSq());
        }

        Vec4 Normalized() const noexcept
        {
            float len = Length();
            if (len > EPSILON_POS)
                return *this / len;
            return Vec4(0.0f, 0.0f, 0.0f, 0.0f);
        }

        void Normalize() noexcept
        {
            *this = Normalized();
        }

        constexpr float Dot(const Vec4& other) const noexcept
        {
            return x * other.x + y * other.y + z * other.z + w * other.w;
        }

        constexpr float DistanceSq(const Vec4& other) const noexcept
        {
            float dx = x - other.x;
            float dy = y - other.y;
            float dz = z - other.z;
            float dw = w - other.w;
            return dx * dx + dy * dy + dz * dz + dw * dw;
        }

        float Distance(const Vec4& other) const noexcept
        {
            return sqrtf(DistanceSq(other));
        }

        // Lerp
        static constexpr Vec4 Lerp(const Vec4& a, const Vec4& b, float t) noexcept
        {
            return Vec4(
                xg::Lerp(a.x, b.x, t),
                xg::Lerp(a.y, b.y, t),
                xg::Lerp(a.z, b.z, t),
                xg::Lerp(a.w, b.w, t)
            );
        }

        static constexpr Vec4 LerpClamped(const Vec4& a, const Vec4& b, float t) noexcept
        {
            t = xg::Clamp(t, 0.0f, 1.0f);
            return Lerp(a, b, t);
        }

        // Min/Max component-wise
        static constexpr Vec4 Min(const Vec4& a, const Vec4& b) noexcept
        {
            return Vec4(xg::Min(a.x, b.x), xg::Min(a.y, b.y), xg::Min(a.z, b.z), xg::Min(a.w, b.w));
        }

        static constexpr Vec4 Max(const Vec4& a, const Vec4& b) noexcept
        {
            return Vec4(xg::Max(a.x, b.x), xg::Max(a.y, b.y), xg::Max(a.z, b.z), xg::Max(a.w, b.w));
        }

        static constexpr Vec4 Clamp(const Vec4& v, const Vec4& min, const Vec4& max) noexcept
        {
            return Vec4(
                xg::Clamp(v.x, min.x, max.x),
                xg::Clamp(v.y, min.y, max.y),
                xg::Clamp(v.z, min.z, max.z),
                xg::Clamp(v.w, min.w, max.w)
            );
        }

        static constexpr Vec4 Abs(const Vec4& v) noexcept
        {
            return Vec4(xg::Abs(v.x), xg::Abs(v.y), xg::Abs(v.z), xg::Abs(v.w));
        }

        static constexpr Vec4 Sign(const Vec4& v) noexcept
        {
            return Vec4(xg::Sign(v.x), xg::Sign(v.y), xg::Sign(v.z), xg::Sign(v.w));
        }

        //--------------------------------------------------------
        // DirectXMath interop
        //--------------------------------------------------------
        detail::XGVector ToXM() const noexcept
        {
            return detail::Vec4ToXM(*this);
        }

        static Vec4 FromXM(detail::XGVector v) noexcept
        {
            Vec4 result;
            detail::XMToVec4(v, result);
            return result;
        }

        //--------------------------------------------------------
        // Static constants
        //--------------------------------------------------------
        static constexpr Vec4 Zero() noexcept { return Vec4(0.0f, 0.0f, 0.0f, 0.0f); }
        static constexpr Vec4 One() noexcept { return Vec4(1.0f, 1.0f, 1.0f, 1.0f); }
        static constexpr Vec4 UnitX() noexcept { return Vec4(1.0f, 0.0f, 0.0f, 0.0f); }
        static constexpr Vec4 UnitY() noexcept { return Vec4(0.0f, 1.0f, 0.0f, 0.0f); }
        static constexpr Vec4 UnitZ() noexcept { return Vec4(0.0f, 0.0f, 1.0f, 0.0f); }
        static constexpr Vec4 UnitW() noexcept { return Vec4(0.0f, 0.0f, 0.0f, 1.0f); }
    };

    // Static assertions for POD/trivial properties
    //static_assert(std::is_trivial_v<Vec4>, "Vec4 must be trivial");
    //static_assert(std::is_standard_layout_v<Vec4>, "Vec4 must be standard layout");
    static_assert(sizeof(Vec4) == 4 * sizeof(float), "Vec4 must be 4 floats");
    //static_assert(std::is_trivially_copyable_v<Vec4>, "Vec4 must be trivially copyable");
} // namespace xg

// Non-member operators and functions
namespace xg
{
    // Scalar multiplication (float * Vec4)
    constexpr Vec4 operator*(float scalar, const Vec4& v) noexcept
    {
        return v * scalar;
    }

    // Lerp
    constexpr Vec4 Lerp(const Vec4& a, const Vec4& b, float t) noexcept
    {
        return Vec4::Lerp(a, b, t);
    }

    constexpr Vec4 LerpClamped(const Vec4& a, const Vec4& b, float t) noexcept
    {
        return Vec4::LerpClamped(a, b, t);
    }

    // Distance
    constexpr float DistanceSq(const Vec4& a, const Vec4& b) noexcept
    {
        return a.DistanceSq(b);
    }

    inline float Distance(const Vec4& a, const Vec4& b) noexcept
    {
        return a.Distance(b);
    }

    // Dot
    constexpr float Dot(const Vec4& a, const Vec4& b) noexcept
    {
        return a.Dot(b);
    }

    // Normalize
    inline Vec4 Normalize(const Vec4& v) noexcept
    {
        return v.Normalized();
    }

    // Min/Max
    constexpr Vec4 Min(const Vec4& a, const Vec4& b) noexcept
    {
        return Vec4::Min(a, b);
    }

    constexpr Vec4 Max(const Vec4& a, const Vec4& b) noexcept
    {
        return Vec4::Max(a, b);
    }

    constexpr Vec4 Clamp(const Vec4& v, const Vec4& min, const Vec4& max) noexcept
    {
        return Vec4::Clamp(v, min, max);
    }
} // namespace xg