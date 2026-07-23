// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.

#pragma once

// xgMat4x4.h - 4x4 Matrix type
// POD-friendly, trivially copyable, STL-free

#include "xgMathCommon.h"
#include "xgVec3.h"
#include "xgQuat.h"

namespace xg
{
    // Forward declarations
    struct Vec3;
    struct Vec4;
    struct Quat;
    struct Mat4x4;

    //============================================================
    // Mat4x4 - 4x4 Matrix (row-major storage, column-major SIMD)
    //============================================================
    struct alignas(16) Mat4x4
    {
        union
        {
            struct
            {
                float m11, m12, m13, m14;
                float m21, m22, m23, m24;
                float m31, m32, m33, m34;
                float m41, m42, m43, m44;
            };
            float m[4][4];
            float v[16];
            detail::XGFloat4x4 xmfloat4x4;
        };

        //--------------------------------------------------------
        // Constructors
        //--------------------------------------------------------
        constexpr Mat4x4() noexcept
            : m11(1), m12(0), m13(0), m14(0),
              m21(0), m22(1), m23(0), m24(0),
              m31(0), m32(0), m33(1), m34(0),
              m41(0), m42(0), m43(0), m44(1) {}

        constexpr Mat4x4(
            float m11_, float m12_, float m13_, float m14_,
            float m21_, float m22_, float m23_, float m24_,
            float m31_, float m32_, float m33_, float m34_,
            float m41_, float m42_, float m43_, float m44_
        ) noexcept
            : m11(m11_), m12(m12_), m13(m13_), m14(m14_),
              m21(m21_), m22(m22_), m23(m23_), m24(m24_),
              m31(m31_), m32(m32_), m33(m33_), m34(m34_),
              m41(m41_), m42(m42_), m43(m43_), m44(m44_) {}

        constexpr Mat4x4(const float* ptr) noexcept
        {
            for (int i = 0; i < 16; ++i)
                v[i] = ptr[i];
        }

        constexpr Mat4x4(const Mat4x4&) noexcept = default;
        constexpr Mat4x4(Mat4x4&&) noexcept = default;
        constexpr Mat4x4& operator=(const Mat4x4&) noexcept = default;
        constexpr Mat4x4& operator=(Mat4x4&&) noexcept = default;

        //--------------------------------------------------------
        // Access operators
        //--------------------------------------------------------
        constexpr float* operator[](size_t row) noexcept { return m[row]; }
        constexpr const float* operator[](size_t row) const noexcept { return m[row]; }
        constexpr float& operator()(size_t row, size_t col) noexcept { return m[row][col]; }
        constexpr float operator()(size_t row, size_t col) const noexcept { return m[row][col]; }

        //--------------------------------------------------------
        // Comparison
        //--------------------------------------------------------
        constexpr bool operator==(const Mat4x4& other) const noexcept
        {
            for (int i = 0; i < 16; ++i)
                if (v[i] != other.v[i]) return false;
            return true;
        }

        constexpr bool operator!=(const Mat4x4& other) const noexcept
        {
            return !(*this == other);
        }

        //--------------------------------------------------------
        // Arithmetic operators
        //--------------------------------------------------------
        constexpr Mat4x4 operator+(const Mat4x4& other) const noexcept
        {
            Mat4x4 result;
            for (int i = 0; i < 16; ++i) result.v[i] = v[i] + other.v[i];
            return result;
        }

        constexpr Mat4x4 operator-(const Mat4x4& other) const noexcept
        {
            Mat4x4 result;
            for (int i = 0; i < 16; ++i) result.v[i] = v[i] - other.v[i];
            return result;
        }

        constexpr Mat4x4 operator*(float scalar) const noexcept
        {
            Mat4x4 result;
            for (int i = 0; i < 16; ++i) result.v[i] = v[i] * scalar;
            return result;
        }

        constexpr Mat4x4 operator/(float scalar) const noexcept
        {
            float inv = 1.0f / scalar;
            Mat4x4 result;
            for (int i = 0; i < 16; ++i) result.v[i] = v[i] * inv;
            return result;
        }

        // Matrix multiplication (this * other)
        Mat4x4 operator*(const Mat4x4& other) const noexcept
        {
            Mat4x4 result;
            detail::XMToMat4x4(
                detail::Mat4x4ToXM(*this) * detail::Mat4x4ToXM(other),
                result
            );
            return result;
        }

        Mat4x4& operator*=(const Mat4x4& other) noexcept
        {
            *this = *this * other;
            return *this;
        }

        //--------------------------------------------------------
        // Matrix operations
        //--------------------------------------------------------

        // Transpose
        Mat4x4 Transposed() const noexcept
        {
            Mat4x4 result;
            detail::XMToMat4x4(
                detail::Mat4x4Transpose(detail::Mat4x4ToXM(*this)),
                result
            );
            return result;
        }

        void Transpose() noexcept
        {
            *this = Transposed();
        }

        // Determinant
        float Determinant() const noexcept
        {
            return detail::DeterminantX(detail::Mat4x4ToXM(*this));
        }

        // Inverse
        Mat4x4 Inverse() const noexcept
        {
            Mat4x4 result;
            detail::XMToMat4x4(
                detail::Mat4x4Inverse(detail::Mat4x4ToXM(*this)),
                result
            );
            return result;
        }

        bool Inverse(Mat4x4& out) const noexcept
        {
            detail::XGVector det;
            detail::XMToMat4x4(
                detail::Mat4x4Inverse(detail::Mat4x4ToXM(*this), &det),
                out
            );
            return detail::Vec3Length(det) != 0.0f;
        }

        // Decompose into scale, rotation, translation
        void Decompose(Vec3& scale, Quat& rotation, Vec3& translation) const noexcept
        {
            detail::XGVector s, r, t;
            detail::Mat4x4Decompose(detail::Mat4x4ToXM(*this), s, r, t);
            detail::XMToVec3(s, scale);
            detail::XMToQuat(r, rotation);
            detail::XMToVec3(t, translation);
        }

        // Get translation component
        Vec3 GetTranslation() const noexcept
        {
            Vec3 result;
            detail::XMToVec3(
                detail::Mat4x4GetTranslation(detail::Mat4x4ToXM(*this)),
                result
            );
            return result;
        }

        // Get scale component
        Vec3 GetScale() const noexcept
        {
            Vec3 result;
            detail::XMToVec3(
                detail::Mat4x4GetScale(detail::Mat4x4ToXM(*this)),
                result
            );
            return result;
        }

        // Get rotation component
        Quat GetRotation() const noexcept
        {
            Quat result;
            detail::XMToQuat(
                detail::Mat4x4GetRotation(detail::Mat4x4ToXM(*this)),
                result
            );
            return result;
        }

        // Transform point (with translation)
        Vec3 TransformPoint(const Vec3& v) const noexcept
        {
            detail::XGVector result = detail::Vec4Transform(v.ToXM(), detail::Mat4x4ToXM(*this));
            return Vec3::FromXM(result);
        }

        // Transform vector (no translation)
        Vec3 TransformVector(const Vec3& v) const noexcept
        {
            detail::XGVector result = detail::Vec4Transform(Vec4(v.x, v.y, v.z, 0.0f).ToXM(), detail::Mat4x4ToXM(*this));
            return Vec3::FromXM(result);
        }

        // Transform normal (inverse transpose)
        Vec3 TransformNormal(const Vec3& v) const noexcept
        {
            Mat4x4 invTrans = Inverse().Transposed();
            return invTrans.TransformVector(v);
        }

        //--------------------------------------------------------
        // Static factory methods
        //--------------------------------------------------------
        static constexpr Mat4x4 Identity() noexcept
        {
            return Mat4x4();
        }

        static Mat4x4 Translation(float x, float y, float z) noexcept
        {
            Mat4x4 result;
            detail::XMToMat4x4(
                detail::Mat4x4Translation(x, y, z),
                result
            );
            return result;
        }

        static Mat4x4 Translation(const Vec3& v) noexcept
        {
            return Translation(v.x, v.y, v.z);
        }

        static Mat4x4 RotationX(float angle) noexcept
        {
            Mat4x4 result;
            detail::XMToMat4x4(
                detail::Mat4x4RotationX(angle),
                result
            );
            return result;
        }

        static Mat4x4 RotationY(float angle) noexcept
        {
            Mat4x4 result;
            detail::XMToMat4x4(
                detail::Mat4x4RotationY(angle),
                result
            );
            return result;
        }

        static Mat4x4 RotationZ(float angle) noexcept
        {
            Mat4x4 result;
            detail::XMToMat4x4(
                detail::Mat4x4RotationZ(angle),
                result
            );
            return result;
        }

        static Mat4x4 RotationRollPitchYaw(float pitch, float yaw, float roll) noexcept
        {
            Mat4x4 result;
            detail::XMToMat4x4(
                detail::Mat4x4RotationRollPitchYaw(pitch, yaw, roll),
                result
            );
            return result;
        }

        static Mat4x4 RotationAxis(const Vec3& axis, float angle) noexcept
        {
            Mat4x4 result;
            detail::XMToMat4x4(
                detail::Mat4x4RotationAxis(axis.ToXM(), angle),
                result
            );
            return result;
        }

        static Mat4x4 RotationQuaternion(const Quat& q) noexcept
        {
            Mat4x4 result;
            detail::XMToMat4x4(
                detail::Mat4x4RotationQuaternion(q.ToXM()),
                result
            );
            return result;
        }

        static Mat4x4 Scaling(float x, float y, float z) noexcept
        {
            Mat4x4 result;
            detail::XMToMat4x4(
                detail::Mat4x4Scaling(x, y, z),
                result
            );
            return result;
        }

        static Mat4x4 Scaling(const Vec3& v) noexcept
        {
            return Scaling(v.x, v.y, v.z);
        }

        static Mat4x4 AffineTransformation(
            const Vec3& scaling,
            const Vec3& rotationOrigin,
            const Quat& rotation,
            const Vec3& translation
        ) noexcept
        {
            Mat4x4 result;
            detail::XMToMat4x4(
                detail::Mat4x4AffineTransformation(
                    scaling.ToXM(), rotationOrigin.ToXM(), rotation.ToXM(), translation.ToXM()
                ),
                result
            );
            return result;
        }

        static Mat4x4 LookAtLH(const Vec3& eye, const Vec3& at, const Vec3& up) noexcept
        {
            Mat4x4 result;
            detail::XMToMat4x4(
                detail::Mat4x4LookAtLH(eye.ToXM(), at.ToXM(), up.ToXM()),
                result
            );
            return result;
        }

        static Mat4x4 LookAtRH(const Vec3& eye, const Vec3& at, const Vec3& up) noexcept
        {
            Mat4x4 result;
            detail::XMToMat4x4(
                detail::Mat4x4LookAtRH(eye.ToXM(), at.ToXM(), up.ToXM()),
                result
            );
            return result;
        }

        static Mat4x4 PerspectiveFovLH(float fovY, float aspect, float nearZ, float farZ) noexcept
        {
            Mat4x4 result;
            detail::XMToMat4x4(
                detail::Mat4x4PerspectiveFovLH(fovY, aspect, nearZ, farZ),
                result
            );
            return result;
        }

        static Mat4x4 PerspectiveFovRH(float fovY, float aspect, float nearZ, float farZ) noexcept
        {
            Mat4x4 result;
            detail::XMToMat4x4(
                detail::Mat4x4PerspectiveFovRH(fovY, aspect, nearZ, farZ),
                result
            );
            return result;
        }

        static Mat4x4 OrthographicLH(float width, float height, float nearZ, float farZ) noexcept
        {
            Mat4x4 result;
            detail::XMToMat4x4(
                detail::Mat4x4OrthographicLH(width, height, nearZ, farZ),
                result
            );
            return result;
        }

        static Mat4x4 OrthographicRH(float width, float height, float nearZ, float farZ) noexcept
        {
            Mat4x4 result;
            detail::XMToMat4x4(
                detail::Mat4x4OrthographicRH(width, height, nearZ, farZ),
                result
            );
            return result;
        }

        static Mat4x4 OrthographicOffCenterLH(float left, float right, float bottom, float top, float nearZ, float farZ) noexcept
        {
            Mat4x4 result;
            detail::XMToMat4x4(
                detail::Mat4x4OrthographicOffCenterLH(left, right, bottom, top, nearZ, farZ),
                result
            );
            return result;
        }

        static Mat4x4 OrthographicOffCenterRH(float left, float right, float bottom, float top, float nearZ, float farZ) noexcept
        {
            Mat4x4 result;
            detail::XMToMat4x4(
                detail::Mat4x4OrthographicOffCenterRH(left, right, bottom, top, nearZ, farZ),
                result
            );
            return result;
        }

        //--------------------------------------------------------
        // DirectXMath interop
        //--------------------------------------------------------
        detail::XGMatrix ToXM() const noexcept
        {
            return detail::Mat4x4ToXM(*this);
        }

        static Mat4x4 FromXM(detail::XGMatrix m) noexcept
        {
            Mat4x4 result;
            detail::XMToMat4x4(m, result);
            return result;
        }
    };

    // Static assertions
    //static_assert(std::is_trivial_v<Mat4x4>, "Mat4x4 must be trivial");
    //static_assert(std::is_standard_layout_v<Mat4x4>, "Mat4x4 must be standard layout");
    static_assert(sizeof(Mat4x4) == 16 * sizeof(float), "Mat4x4 size mismatch");
    //static_assert(std::is_trivially_copyable_v<Mat4x4>, "Mat4x4 must be trivially copyable");
} // namespace xg

// Non-member operators
namespace xg
{
    constexpr Mat4x4 operator*(float scalar, const Mat4x4& m) noexcept
    {
        return m * scalar;
    }

    // Multiply
    inline Mat4x4 Multiply(const Mat4x4& a, const Mat4x4& b) noexcept
    {
        return a * b;
    }

    // Transpose
    inline Mat4x4 Transpose(const Mat4x4& m) noexcept
    {
        return m.Transposed();
    }

    // Determinant
    inline float Determinant(const Mat4x4& m) noexcept
    {
        return m.Determinant();
    }

    // Inverse
    inline Mat4x4 Inverse(const Mat4x4& m) noexcept
    {
        return m.Inverse();
    }

    // Decompose
    inline void Decompose(const Mat4x4& m, Vec3& scale, Quat& rotation, Vec3& translation) noexcept
    {
        m.Decompose(scale, rotation, translation);
    }
} // namespace xg