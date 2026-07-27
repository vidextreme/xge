// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.

#pragma once

// xgQuat.h - Quaternion type
// POD-friendly, trivially copyable, STL-free

#include "xgMathCommon.h"
#include "xgVec3.h"
#include "xgVec4.h"

namespace xg
{
    struct Vec3;
    struct Vec4;
    struct Mat4x4;

    //============================================================
    // Quat - Quaternion (x, y, z, w)
    //============================================================
    struct alignas(16) Quat
    {
        float x = 0.0f;
        float y = 0.0f;
        float z = 0.0f;
        float w = 1.0f;

        //--------------------------------------------------------
        // Constructors
        //--------------------------------------------------------
        constexpr Quat() noexcept = default;

        constexpr Quat(float x_, float y_, float z_, float w_) noexcept
            : x(x_), y(y_), z(z_), w(w_) {}

        constexpr explicit Quat(const Vec4& v) noexcept
            : x(v.x), y(v.y), z(v.z), w(v.w) {}

        Quat(const Vec3& axis, float angle) noexcept
        {
            *this = FromAxisAngle(axis, angle);
        }

        Quat(const Vec3& euler) noexcept
        {
            *this = FromEuler(euler);
        }

        constexpr Quat(const Quat&) noexcept = default;
        constexpr Quat(Quat&&) noexcept = default;
        constexpr Quat& operator=(const Quat&) noexcept = default;
        constexpr Quat& operator=(Quat&&) noexcept = default;

        //--------------------------------------------------------
        // Access
        //--------------------------------------------------------
        constexpr float operator[](size_t i) const noexcept
        {
            return (&x)[i];
        }

        constexpr float& operator[](size_t i) noexcept
        {
            return (&x)[i];
        }

        //--------------------------------------------------------
        // Comparison
        //--------------------------------------------------------
        constexpr bool operator==(const Quat& other) const noexcept
        {
            return IsEqual(x, other.x) && IsEqual(y, other.y) &&
                   IsEqual(z, other.z) && IsEqual(w, other.w);
        }

        constexpr bool operator!=(const Quat& other) const noexcept
        {
            return !(*this == other);
        }

        constexpr bool IsApproxEqual(const Quat& other, float epsilon = EPSILON_POS) const noexcept
        {
            return IsEqual(x, other.x, epsilon) &&
                   IsEqual(y, other.y, epsilon) &&
                   IsEqual(z, other.z, epsilon) &&
                   IsEqual(w, other.w, epsilon);
        }

        //--------------------------------------------------------
        // Arithmetic operators
        //--------------------------------------------------------
        Quat operator*(const Quat& other) const noexcept
        {
            return Multiply(*this, other);
        }

        Quat& operator*=(const Quat& other) noexcept
        {
            *this = Multiply(*this, other);
            return *this;
        }

        constexpr Quat operator*(float scalar) const noexcept
        {
            return Quat(x * scalar, y * scalar, z * scalar, w * scalar);
        }

        constexpr Quat& operator*=(float scalar) noexcept
        {
            x *= scalar; y *= scalar; z *= scalar; w *= scalar; return *this;
        }

        constexpr Quat operator/(float scalar) const noexcept
        {
            float inv = 1.0f / scalar;
            return Quat(x * inv, y * inv, z * inv, w * inv);
        }

        constexpr Quat& operator/=(float scalar) noexcept
        {
            float inv = 1.0f / scalar;
            x *= inv; y *= inv; z *= inv; w *= inv; return *this;
        }

        constexpr Quat operator+(const Quat& other) const noexcept
        {
            return Quat(x + other.x, y + other.y, z + other.z, w + other.w);
        }

        constexpr Quat operator-(const Quat& other) const noexcept
        {
            return Quat(x - other.x, y - other.y, z - other.z, w - other.w);
        }

        constexpr Quat& operator+=(const Quat& other) noexcept
        {
            x += other.x; y += other.y; z += other.z; w += other.w; return *this;
        }

        constexpr Quat& operator-=(const Quat& other) noexcept
        {
            x -= other.x; y -= other.y; z -= other.z; w -= other.w; return *this;
        }

        //--------------------------------------------------------
        // Unary operators
        //--------------------------------------------------------
        constexpr Quat operator+() const noexcept { return *this; }
        constexpr Quat operator-() const noexcept
        {
            return Quat(-x, -y, -z, -w);
        }

        //--------------------------------------------------------
        // Quaternion operations
        //--------------------------------------------------------
        constexpr float LengthSq() const noexcept
        {
            return x * x + y * y + z * z + w * w;
        }

        float Length() const noexcept
        {
            return sqrtf(LengthSq());
        }

        Quat Normalized() const noexcept
        {
            float len = Length();
            if (len > EPSILON_POS)
                return *this / len;
            return Identity();
        }

        void Normalize() noexcept
        {
            *this = Normalized();
        }

        Quat Conjugate() const noexcept
        {
            return Quat(-x, -y, -z, w);
        }

        Quat Inverse() const noexcept
        {
            float lenSq = LengthSq();
            if (lenSq > EPSILON_POS)
                return Conjugate() / lenSq;
            return Identity();
        }

        // Dot product
        constexpr float Dot(const Quat& other) const noexcept
        {
            return x * other.x + y * other.y + z * other.z + w * other.w;
        }

        // Multiply (q * p)
        static Quat Multiply(const Quat& a, const Quat& b) noexcept
        {
            Quat result;
            result.x = a.w * b.x + a.x * b.w + a.y * b.z - a.z * b.y;
            result.y = a.w * b.y - a.x * b.z + a.y * b.w + a.z * b.x;
            result.z = a.w * b.z + a.x * b.y - a.y * b.x + a.z * b.w;
            result.w = a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z;
            return result;
        }

        // Rotate vector by quaternion
        Vec3 RotateVector(const Vec3& v) const noexcept
        {
            // q * v * q^-1

            // Convert vector to quaternion (0, v)
            Quat qv{ v.x, v.y, v.z, 0.0f };

            // Compute q * qv
            Quat t;
            t.x = w * qv.x + x * qv.w + y * qv.z - z * qv.y;
            t.y = w * qv.y - x * qv.z + y * qv.w + z * qv.x;
            t.z = w * qv.z + x * qv.y - y * qv.x + z * qv.w;
            t.w = -x * qv.x - y * qv.y - z * qv.z + w * qv.w;

            // Compute (q * qv) * q^-1
            Quat qi = this->Inverse();

            Quat r;
            r.x = t.w * qi.x + t.x * qi.w + t.y * qi.z - t.z * qi.y;
            r.y = t.w * qi.y - t.x * qi.z + t.y * qi.w + t.z * qi.x;
            r.z = t.w * qi.z + t.x * qi.y - t.y * qi.x + t.z * qi.w;

            return Vec3{ r.x, r.y, r.z };
        }


        // Spherical linear interpolation
        static Quat Slerp(const Quat& a, const Quat& b, float t) noexcept
        {
            Quat result;
            float cosTheta = a.Dot(b);

            Quat end = b;
            if (cosTheta < 0.0f)
            {
                end = -b;
                cosTheta = -cosTheta;
            }

            const float epsilon = 1e-6f;
            if (cosTheta > 1.0f - epsilon)
            {
                // Linear interpolation for small angles
                result = Lerp(a, end, t);
            }
            else
            {
                float theta = acosf(cosTheta);
                float sinTheta = sinf(theta);
                float invSinTheta = 1.0f / sinTheta;
                float w1 = sinf((1.0f - t) * theta) * invSinTheta;
                float w2 = sinf(t * theta) * invSinTheta;
                result.x = a.x * w1 + end.x * w2;
                result.y = a.y * w1 + end.y * w2;
                result.z = a.z * w1 + end.z * w2;
                result.w = a.w * w1 + end.w * w2;

            }
            return result.Normalized();
        }

        // Normalized linear interpolation (nlerp)
        static Quat Nlerp(const Quat& a, const Quat& b, float t) noexcept
        {
            float cosTheta = a.Dot(b);
            Quat end = (cosTheta < 0.0f) ? -b : b;
            return Lerp(a, end, t).Normalized();
        }

        // Linear interpolation (unnormalized)
        static constexpr Quat Lerp(const Quat& a, const Quat& b, float t) noexcept
        {
            return Quat(
                xg::Lerp(a.x, b.x, t),
                xg::Lerp(a.y, b.y, t),
                xg::Lerp(a.z, b.z, t),
                xg::Lerp(a.w, b.w, t)
            );
        }

        // Create from axis-angle
        static Quat FromAxisAngle(const Vec3& axis, float angle) noexcept
        {
            float halfAngle = angle * 0.5f;
            float s = sinf(halfAngle);
            Vec3 normAxis = axis.Normalized();
            return Quat(normAxis.x * s, normAxis.y * s, normAxis.z * s, cosf(halfAngle));
        }

        // Create from Euler angles (pitch, yaw, roll in radians)
        static Quat FromEuler(const Vec3& euler) noexcept
        {
            // Order: Y (yaw) * X (pitch) * Z (roll)
            float cx = cosf(euler.x * 0.5f);
            float sx = sinf(euler.x * 0.5f);
            float cy = cosf(euler.y * 0.5f);
            float sy = sinf(euler.y * 0.5f);
            float cz = cosf(euler.z * 0.5f);
            float sz = sinf(euler.z * 0.5f);

            return Quat(
                sx * cy * cz - cx * sy * sz,  // x
                cx * sy * cz + sx * cy * sz,  // y
                cx * cy * sz - sx * sy * cz,  // z
                cx * cy * cz + sx * sy * sz   // w
            );
        }

        // Create from look direction
        static Quat LookRotation(const Vec3& forward, const Vec3& up = Vec3::Up()) noexcept
        {
            Vec3 f = forward.Normalized();
            Vec3 u = up.Normalized();
            Vec3 r = f.Cross(u).Normalized();
            u = r.Cross(f);

            float trace = r.x + u.y + f.z;
            if (trace > 0.0f)
            {
                float s = sqrtf(trace + 1.0f) * 2.0f;
                return Quat(
                    (u.z - f.y) / s,
                    (f.x - r.z) / s,
                    (r.y - u.x) / s,
                    0.25f * s
                );
            }
            else if (r.x > u.y && r.x > f.z)
            {
                float s = sqrtf(1.0f + r.x - u.y - f.z) * 2.0f;
                return Quat(
                    0.25f * s,
                    (r.y + u.x) / s,
                    (f.x + r.z) / s,
                    (u.z - f.y) / s
                );
            }
            else if (u.y > f.z)
            {
                float s = sqrtf(1.0f + u.y - r.x - f.z) * 2.0f;
                return Quat(
                    (r.y + u.x) / s,
                    0.25f * s,
                    (f.y + u.z) / s,
                    (f.x - r.z) / s
                );
            }
            else
            {
                float s = sqrtf(1.0f + f.z - r.x - u.y) * 2.0f;
                return Quat(
                    (f.x + r.z) / s,
                    (f.y + u.z) / s,
                    0.25f * s,
                    (r.y - u.x) / s
                );
            }
        }

        // Create from rotation matrix
        static Quat FromRotationMatrix(const Mat4x4& m) noexcept;

        // To axis-angle
        void ToAxisAngle(Vec3& axis, float& angle) const noexcept;

        // To Euler angles (pitch, yaw, roll)
        Vec3 ToEuler() const noexcept
        {
            Vec3 euler;
            // Roll (x-axis rotation)
            float sinr_cosp = 2.0f * (w * x + y * z);
            float cosr_cosp = 1.0f - 2.0f * (x * x + y * y);
            euler.x = atan2f(sinr_cosp, cosr_cosp);

            // Pitch (y-axis rotation)
            float sinp = 2.0f * (w * y - z * x);
            if (fabsf(sinp) >= 1.0f)
                euler.y = HALF_PI * Sign(sinp); // Use 90 degrees if out of range
            else
                euler.y = asinf(sinp);

            // Yaw (z-axis rotation)
            float siny_cosp = 2.0f * (w * z + x * y);
            float cosy_cosp = 1.0f - 2.0f * (y * y + z * z);
            euler.z = atan2f(siny_cosp, cosy_cosp);

            return euler;
        }

        // Get forward/up/right vectors
        Vec3 Forward() const noexcept;
        Vec3 Up() const noexcept;
        Vec3 Right() const noexcept;

        // Angle between quaternions (radians)
        inline float Angle(const Quat& other) const noexcept
        {
            float dot = Dot(other);
            dot = Clamp(dot, -1.0f, 1.0f);
            return 2.0f * acosf(fabsf(dot));
        }

        //--------------------------------------------------------
        // DirectXMath interop
        //--------------------------------------------------------
        detail::XGQuat ToXM() const noexcept
        {
            return detail::QuatToXM(*this);
        }

        static Quat FromXM(detail::XGQuat v) noexcept
        {
            Quat result;
            detail::XMToQuat(v, result);
            return result;
        }

        //--------------------------------------------------------
        // Static constants
        //--------------------------------------------------------
        static constexpr Quat Identity() noexcept { return Quat(0.0f, 0.0f, 0.0f, 1.0f); }
    };

    // Static assertions
    //static_assert(std::is_trivial_v<Quat>, "Quat must be trivial");
    //static_assert(std::is_standard_layout_v<Quat>, "Quat must be standard layout");
    static_assert(sizeof(Quat) == 4 * sizeof(float), "Quat must be 4 floats");
    //static_assert(std::is_trivially_copyable_v<Quat>, "Quat must be trivially copyable");
} // namespace xg

// Non-member operators and functions
namespace xg
{
    constexpr Quat operator*(float scalar, const Quat& q) noexcept
    {
        return q * scalar;
    }

    // Slerp
    inline Quat Slerp(const Quat& a, const Quat& b, float t) noexcept
    {
        return Quat::Slerp(a, b, t);
    }

    // Nlerp
    inline Quat Nlerp(const Quat& a, const Quat& b, float t) noexcept
    {
        return Quat::Nlerp(a, b, t);
    }

    // Lerp
    constexpr Quat Lerp(const Quat& a, const Quat& b, float t) noexcept
    {
        return Quat::Lerp(a, b, t);
    }

    // Dot
    constexpr float Dot(const Quat& a, const Quat& b) noexcept
    {
        return a.Dot(b);
    }

    // Conjugate
    inline Quat Conjugate(const Quat& q) noexcept
    {
        return q.Conjugate();
    }

    // Inverse
    inline Quat Inverse(const Quat& q) noexcept
    {
        return q.Inverse();
    }

    // Normalize
    inline Quat Normalize(const Quat& q) noexcept
    {
        return q.Normalized();
    }

    // Rotate vector by quaternion
    inline Vec3 RotateVector(const Quat& q, const Vec3& v) noexcept
    {
        return q.RotateVector(v);
    }

    // Angle between quaternions
    inline float Angle(const Quat& a, const Quat& b) noexcept
    {
        return a.Angle(b);
    }
} // namespace xg