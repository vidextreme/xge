// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.

#pragma once

// xgVec3.h - 3D vector type
// POD-friendly, trivially copyable, STL-free

#include "xgMathCommon.h"
#include "xgVec2.h"

namespace xg
{
    // Forward declarations
    struct Vec2;
    struct Vec3;
    struct Vec4;
    struct Quat;
    struct Mat4x4;

    //============================================================
    // Vec3 - 3D Vector
    //============================================================
    struct alignas(16) Vec3
    {
        float x = 0.0f;
        float y = 0.0f;
        float z = 0.0f;

        //--------------------------------------------------------
        // Constructors
        //--------------------------------------------------------
        constexpr Vec3() noexcept = default;

        constexpr Vec3(float x_, float y_, float z_) noexcept
            : x(x_), y(y_), z(z_) {}

        constexpr explicit Vec3(float v) noexcept
            : x(v), y(v), z(v) {}

        constexpr Vec3(const Vec2& xy, float z_) noexcept
            : x(xy.x), y(xy.y), z(z_) {}

        constexpr Vec3(const Vec3&) noexcept = default;
        constexpr Vec3(Vec3&&) noexcept = default;
        constexpr Vec3& operator=(const Vec3&) noexcept = default;
        constexpr Vec3& operator=(Vec3&&) noexcept = default;

        // Construct from Vec4 (drops w)
        constexpr explicit Vec3(const Vec4& v) noexcept;

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
        constexpr bool operator==(const Vec3& other) const noexcept
        {
            return x == other.x && y == other.y && z == other.z;
        }

        constexpr bool operator!=(const Vec3& other) const noexcept
        {
            return !(*this == other);
        }

        //--------------------------------------------------------
        // Arithmetic operators
        //--------------------------------------------------------
        constexpr Vec3 operator+(const Vec3& other) const noexcept
        {
            return Vec3(x + other.x, y + other.y, z + other.z);
        }

        constexpr Vec3 operator-(const Vec3& other) const noexcept
        {
            return Vec3(x - other.x, y - other.y, z - other.z);
        }

        constexpr Vec3 operator*(const Vec3& other) const noexcept
        {
            return Vec3(x * other.x, y * other.y, z * other.z);
        }

        constexpr Vec3 operator/(const Vec3& other) const noexcept
        {
            return Vec3(x / other.x, y / other.y, z / other.z);
        }

        constexpr Vec3 operator*(float scalar) const noexcept
        {
            return Vec3(x * scalar, y * scalar, z * scalar);
        }

        constexpr Vec3 operator/(float scalar) const noexcept
        {
            float inv = 1.0f / scalar;
            return Vec3(x * inv, y * inv, z * inv);
        }

        constexpr Vec3& operator+=(const Vec3& other) noexcept
        {
            x += other.x; y += other.y; z += other.z; return *this;
        }

        constexpr Vec3& operator-=(const Vec3& other) noexcept
        {
            x -= other.x; y -= other.y; z -= other.z; return *this;
        }

        constexpr Vec3& operator*=(const Vec3& other) noexcept
        {
            x *= other.x; y *= other.y; z *= other.z; return *this;
        }

        constexpr Vec3& operator/=(const Vec3& other) noexcept
        {
            x /= other.x; y /= other.y; z /= other.z; return *this;
        }

        constexpr Vec3& operator*=(float scalar) noexcept
        {
            x *= scalar; y *= scalar; z *= scalar; return *this;
        }

        constexpr Vec3& operator/=(float scalar) noexcept
        {
            float inv = 1.0f / scalar;
            x *= inv; y *= inv; z *= inv; return *this;
        }

        //--------------------------------------------------------
        // Unary operators
        //--------------------------------------------------------
        constexpr Vec3 operator+() const noexcept { return *this; }
        constexpr Vec3 operator-() const noexcept { return Vec3(-x, -y, -z); }

        //--------------------------------------------------------
        // Vector operations
        //--------------------------------------------------------
        constexpr float LengthSq() const noexcept
        {
            return x * x + y * y + z * z;
        }

        float Length() const noexcept
        {
            return sqrtf(LengthSq());
        }

        Vec3 Normalized() const noexcept
        {
            float len = Length();
            if (len > constants::EPSILON_POS)
                return *this / len;
            return Vec3(0.0f, 0.0f, 0.0f);
        }

        void Normalize() noexcept
        {
            *this = Normalized();
        }

        constexpr float Dot(const Vec3& other) const noexcept
        {
            return x * other.x + y * other.y + z * other.z;
        }

        Vec3 Cross(const Vec3& other) const noexcept
        {
            return Vec3(
                y * other.z - z * other.y,
                z * other.x - x * other.z,
                x * other.y - y * other.x
            );
        }

        constexpr float DistanceSq(const Vec3& other) const noexcept
        {
            float dx = x - other.x;
            float dy = y - other.y;
            float dz = z - other.z;
            return dx * dx + dy * dy + dz * dz;
        }

        float Distance(const Vec3& other) const noexcept
        {
            return sqrtf(DistanceSq(other));
        }

        // Lerp
        static constexpr Vec3 Lerp(const Vec3& a, const Vec3& b, float t) noexcept
        {
            return Vec3(
                xg::Lerp(a.x, b.x, t),
                xg::Lerp(a.y, b.y, t),
                xg::Lerp(a.z, b.z, t)
            );
        }

        static Vec3 LerpClamped(const Vec3& a, const Vec3& b, float t) noexcept
        {
            t = xg::Clamp(t, 0.0f, 1.0f);
            return Vec3::Lerp(a, b, t);
        }

        // Min/Max component-wise
        static constexpr Vec3 Min(const Vec3& a, const Vec3& b) noexcept
        {
            return Vec3(xg::Min(a.x, b.x), xg::Min(a.y, b.y), xg::Min(a.z, b.z));
        }

        static constexpr Vec3 Max(const Vec3& a, const Vec3& b) noexcept
        {
            return Vec3(xg::Max(a.x, b.x), xg::Max(a.y, b.y), xg::Max(a.z, b.z));
        }

        static constexpr Vec3 Clamp(const Vec3& v, const Vec3& min, const Vec3& max) noexcept
        {
            return Vec3(
                xg::Clamp(v.x, min.x, max.x),
                xg::Clamp(v.y, min.y, max.y),
                xg::Clamp(v.z, min.z, max.z)
            );
        }

        static constexpr Vec3 Abs(const Vec3& v) noexcept
        {
            return Vec3(xg::Abs(v.x), xg::Abs(v.y), xg::Abs(v.z));
        }

        static constexpr Vec3 Sign(const Vec3& v) noexcept
        {
            return Vec3(xg::Sign(v.x), xg::Sign(v.y), xg::Sign(v.z));
        }

        static constexpr Vec3 Floor(const Vec3& v) noexcept
        {
            return Vec3(floorf(v.x), floorf(v.y), floorf(v.z));
        }

        static constexpr Vec3 Ceil(const Vec3& v) noexcept
        {
            return Vec3(ceilf(v.x), ceilf(v.y), ceilf(v.z));
        }

        static constexpr Vec3 Round(const Vec3& v) noexcept
        {
            return Vec3(roundf(v.x), roundf(v.y), roundf(v.z));
        }

        // Reflect
        static Vec3 Reflect(const Vec3& v, const Vec3& normal) noexcept
        {
            float dot = v.Dot(normal);
            return v - normal * (2.0f * dot);
        }

        // Refract
        static Vec3 Refract(const Vec3& v, const Vec3& normal, float eta) noexcept
        {
            float cosi = xg::Clamp(v.Dot(normal), -1.0f, 1.0f);
            float etai = 1.0f, etat = eta;
            Vec3 n = normal;
            if (cosi < 0.0f) { cosi = -cosi; }
            else { std::swap(etai, etat); n = -normal; }
            float etaRatio = etai / etat;
            float k = 1.0f - etaRatio * etaRatio * (1.0f - cosi * cosi);
            if (k < 0.0f)
                return Vec3(0.0f, 0.0f, 0.0f);
            return v * etaRatio + n * (etaRatio * cosi - sqrtf(k));
        }

        // Project
        static Vec3 Project(const Vec3& v, const Vec3& on) noexcept
        {
            float lenSq = on.LengthSq();
            if (lenSq > constants::EPSILON_POS)
                return on * (v.Dot(on) / lenSq);
            return Vec3(0.0f, 0.0f, 0.0f);
        }

        // Reject
        static Vec3 Reject(const Vec3& v, const Vec3& on) noexcept
        {
            return v - Project(v, on);
        }

        // Rotate around axis
        Vec3 RotateAroundAxis(const Vec3& axis, float angle) const noexcept
        {
            float s = sinf(angle);
            float c = cosf(angle);
            Vec3 u = axis.Normalized();
            return *this * c + u.Cross(*this) * s + u * (u.Dot(*this)) * (1.0f - c);
        }

        //--------------------------------------------------------
        // DirectXMath interop
        //--------------------------------------------------------
        detail::XGVector ToXM() const noexcept
        {
            return detail::Vec3ToXM(*this);
        }

        static Vec3 FromXM(detail::XGVector v) noexcept
        {
            Vec3 result;
            detail::XMToVec3(v, result);
            return result;
        }

        //--------------------------------------------------------
        // Static constants
        //--------------------------------------------------------
        static constexpr Vec3 Zero() noexcept { return Vec3(0.0f, 0.0f, 0.0f); }
        static constexpr Vec3 One() noexcept { return Vec3(1.0f, 1.0f, 1.0f); }
        static constexpr Vec3 UnitX() noexcept { return Vec3(1.0f, 0.0f, 0.0f); }
        static constexpr Vec3 UnitY() noexcept { return Vec3(0.0f, 1.0f, 0.0f); }
        static constexpr Vec3 UnitZ() noexcept { return Vec3(0.0f, 0.0f, 1.0f); }
        static constexpr Vec3 Up() noexcept { return Vec3(0.0f, 1.0f, 0.0f); }
        static constexpr Vec3 Down() noexcept { return Vec3(0.0f, -1.0f, 0.0f); }
        static constexpr Vec3 Left() noexcept { return Vec3(-1.0f, 0.0f, 0.0f); }
        static constexpr Vec3 Right() noexcept { return Vec3(1.0f, 0.0f, 0.0f); }
        static constexpr Vec3 Forward() noexcept { return Vec3(0.0f, 0.0f, 1.0f); }
        static constexpr Vec3 Back() noexcept { return Vec3(0.0f, 0.0f, -1.0f); }
        static constexpr Vec3 PositiveInfinity() noexcept { return Vec3(constants::INFINITY_F, constants::INFINITY_F, constants::INFINITY_F); }
        static constexpr Vec3 NegativeInfinity() noexcept { return Vec3(-constants::INFINITY_F, -constants::INFINITY_F, -constants::INFINITY_F); }
    };

    // Static assertions for POD/trivial properties
    //static_assert(std::is_trivial_v<Vec3>, "Vec3 must be trivial");
    static_assert(std::is_standard_layout_v<Vec3>, "Vec3 must be standard layout");
    //static_assert(sizeof(Vec3) == 3 * sizeof(float), "Vec3 must be 3 floats");
    //static_assert(std::is_trivially_copyable_v<Vec3>, "Vec3 must be trivially copyable");
} // namespace xg

// Non-member operators and functions
namespace xg
{
    // Scalar multiplication (float * Vec3)
    constexpr Vec3 operator*(float scalar, const Vec3& v) noexcept
    {
        return v * scalar;
    }

    // Lerp free function
    constexpr Vec3 Lerp(const Vec3& a, const Vec3& b, float t) noexcept
    {
        return Vec3::Lerp(a, b, t);
    }

     Vec3 LerpClamped(const Vec3& a, const Vec3& b, float t) noexcept
    {
        return Vec3::LerpClamped(a, b, t);
    }

    // Distance free functions
    constexpr float DistanceSq(const Vec3& a, const Vec3& b) noexcept
    {
        return a.DistanceSq(b);
    }

    float Distance(const Vec3& a, const Vec3& b) noexcept
    {
        return a.Distance(b);
    }

    // Dot free function
    constexpr float Dot(const Vec3& a, const Vec3& b) noexcept
    {
        return a.Dot(b);
    }

    // Cross free function
    Vec3 Cross(const Vec3& a, const Vec3& b) noexcept
    {
        return a.Cross(b);
    }

    // Reflect free function
    Vec3 Reflect(const Vec3& v, const Vec3& normal) noexcept
    {
        return Vec3::Reflect(v, normal);
    }

    // Refract free function
    Vec3 Refract(const Vec3& v, const Vec3& normal, float eta) noexcept
    {
        return Vec3::Refract(v, normal, eta);
    }

    // Project free function
    Vec3 Project(const Vec3& v, const Vec3& on) noexcept
    {
        return Vec3::Project(v, on);
    }

    // Reject free function
    Vec3 Reject(const Vec3& v, const Vec3& on) noexcept
    {
        return Vec3::Reject(v, on);
    }

    // Normalize free function
    Vec3 Normalize(const Vec3& v) noexcept
    {
        return v.Normalized();
    }

    // Min/Max free functions
    constexpr Vec3 Min(const Vec3& a, const Vec3& b) noexcept
    {
        return Vec3::Min(a, b);
    }

    constexpr Vec3 Max(const Vec3& a, const Vec3& b) noexcept
    {
        return Vec3::Max(a, b);
    }

    constexpr Vec3 Clamp(const Vec3& v, const Vec3& min, const Vec3& max) noexcept
    {
        return Vec3::Clamp(v, min, max);
    }
} // namespace xg