// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.

#pragma once

// xgTransform.h - Transform component (position, rotation, scale)
// POD-friendly, trivially copyable, STL-free

#include "xgMathCommon.h"

namespace xg
{
    // Forward declarations
    struct Vec3;
    struct Quat;
    struct Mat4x4;

    //============================================================
    // Transform - Position, Rotation, Scale
    //============================================================
    struct alignas(16) Transform
    {
        Vec3 Position;
        Quat Rotation;
        Vec3 Scale;

        //--------------------------------------------------------
        // Constructors
        //--------------------------------------------------------
        constexpr Transform() noexcept
            : Position(), Rotation(Quat::Identity()), Scale(Vec3::One()) {}

        constexpr Transform(const Vec3& pos, const Quat& rot, const Vec3& scl) noexcept
            : Position(pos), Rotation(rot), Scale(scl) {}

        constexpr Transform(const Vec3& pos, const Quat& rot) noexcept
            : Position(pos), Rotation(rot), Scale(Vec3::One()) {}

        constexpr Transform(const Vec3& pos) noexcept
            : Position(pos), Rotation(Quat::Identity()), Scale(Vec3::One()) {}

        constexpr Transform(const Transform&) noexcept = default;
        constexpr Transform(Transform&&) noexcept = default;
        constexpr Transform& operator=(const Transform&) noexcept = default;
        constexpr Transform& operator=(Transform&&) noexcept = default;

        //--------------------------------------------------------
        // Comparison
        //--------------------------------------------------------
        constexpr bool operator==(const Transform& other) const noexcept
        {
            return Position == other.Position && Rotation == other.Rotation && Scale == other.Scale;
        }

        constexpr bool operator!=(const Transform& other) const noexcept
        {
            return !(*this == other);
        }

        //--------------------------------------------------------
        // Transform operations
        //--------------------------------------------------------

        // Convert to matrix (T * R * S)
        Mat4x4 ToMatrix() const noexcept;

        // Convert to matrix (no allocation)
        void ToMatrix(Mat4x4& out) const noexcept
        {
            out = ToMatrix();
        }

        // Inverse transform
        Transform Inverse() const noexcept;

        // Transform a point (position + rotation + scale + translation)
        Vec3 TransformPoint(const Vec3& point) const noexcept;

        // Transform a vector (rotation + scale only, no translation)
        Vec3 TransformVector(const Vec3& vector) const noexcept;

        // Transform a normal (inverse transpose for non-uniform scale)
        Vec3 TransformNormal(const Vec3& normal) const noexcept;

        // Interpolate (linear for position/scale, nlerp for rotation)
        static Transform Lerp(const Transform& a, const Transform& b, float t) noexcept;

        // Interpolate (linear for position/scale, slerp for rotation)
        static Transform Slerp(const Transform& a, const Transform& b, float t) noexcept;

        // Look at target
        void LookAt(const Vec3& target, const Vec3& up = Vec3::Up()) noexcept;

        // Get direction vectors
        Vec3 Forward() const noexcept;
        Vec3 Up() const noexcept;
        Vec3 Right() const noexcept;

        // Set from matrix (decompose)
        void FromMatrix(const Mat4x4& m) noexcept;

        // Static constants
        static constexpr Transform Identity() noexcept
        {
            return Transform(Vec3::Zero(), Quat::Identity(), Vec3::One());
        }
    };

    // Static assertions
    //static_assert(std::is_trivial_v<Transform>, "Transform must be trivial");
    //static_assert(std::is_standard_layout_v<Transform>, "Transform must be standard layout");
    //static_assert(std::is_trivially_copyable_v<Transform>, "Transform must be trivially copyable");
} // namespace xg

// Non-member functions
namespace xg
{
    // Lerp
    inline Transform Lerp(const Transform& a, const Transform& b, float t) noexcept
    {
        return Transform::Lerp(a, b, t);
    }

    // Slerp
    inline Transform Slerp(const Transform& a, const Transform& b, float t) noexcept
    {
        return Transform::Slerp(a, b, t);
    }

    // Inverse
    inline Transform Inverse(const Transform& t) noexcept
    {
        return t.Inverse();
    }

    // Multiply (compose): a * b = apply b then a
    inline Transform operator*(const Transform& a, const Transform& b) noexcept
    {
        Transform result;
        result.Scale = a.Scale * b.Scale;
        result.Rotation = a.Rotation * b.Rotation;
        result.Position = a.Rotation.RotateVector(a.Scale * b.Position) + a.Position;
        return result;
    }
} // namespace xg