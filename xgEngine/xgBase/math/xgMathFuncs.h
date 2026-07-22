// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.

#pragma once

// xgMathFuncs.h - Math utility functions

#include "xgMathCommon.h"

namespace xg
{
    //============================================================
    // Scalar math functions (constexpr where possible)
    //============================================================

    // Basic operations
    constexpr float Abs(float x) noexcept { return x < 0 ? -x : x; }
    constexpr double Abs(double x) noexcept { return x < 0 ? -x : x; }
    constexpr int Abs(int x) noexcept { return x < 0 ? -x : x; }

    constexpr float Min(float a, float b) noexcept { return a < b ? a : b; }
    constexpr float Max(float a, float b) noexcept { return a > b ? a : b; }
    constexpr float Clamp(float x, float min, float max) noexcept { return Min(Max(x, min), max); }

    // Sign
    constexpr float Sign(float x) noexcept { return (x > 0) - (x < 0); }
    constexpr int Sign(int x) noexcept { return (x > 0) - (x < 0); }

    // Linear interpolation
    constexpr float Lerp(float a, float b, float t) noexcept { return a + (b - a) * t; }
    constexpr float LerpClamped(float a, float b, float t) noexcept { return Lerp(a, b, Clamp(t, 0.0f, 1.0f)); }

    // Smooth step
    constexpr float SmoothStep(float t) noexcept
    {
        t = Clamp(t, 0.0f, 1.0f);
        return t * t * (3.0f - 2.0f * t);
    }

    constexpr float SmootherStep(float t) noexcept
    {
        t = Clamp(t, 0.0f, 1.0f);
        return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f);
    }

    // Radians/Degrees conversion (inline constexpr)
    constexpr float DegToRad(float deg) noexcept { return deg * constants::DEG_TO_RAD; }
    constexpr float RadToDeg(float rad) noexcept { return rad * constants::RAD_TO_DEG; }

    // Fast math approximations
    float FastSin(float x) noexcept;
    float FastCos(float x) noexcept;
    float FastAtan2(float y, float x) noexcept;
    float FastSqrt(float x) noexcept;
    float FastInvSqrt(float x) noexcept;
    float FastPow(float x, float y) noexcept;
    float FastExp(float x) noexcept;
    float FastLog(float x) noexcept;

    // Epsilon comparisons
    constexpr bool IsZero(float x, float epsilon = constants::EPSILON_POS) noexcept
    {
        return x > -epsilon && x < epsilon;
    }

    constexpr bool IsEqual(float a, float b, float epsilon = constants::EPSILON_POS) noexcept
    {
        return (a - b > -epsilon) && (a - b < epsilon);
    }

    // Modulo that works with negatives
    constexpr float Mod(float x, float m) noexcept
    {
        float r = fmodf(x, m);
        return r < 0 ? r + m : r;
    }

    constexpr int Mod(int x, int m) noexcept
    {
        int r = x % m;
        return r < 0 ? r + m : r;
    }

    // Power of 2
    constexpr bool IsPowerOfTwo(unsigned x) noexcept
    {
        return x && !(x & (x - 1));
    }

    constexpr unsigned NextPowerOfTwo(unsigned x) noexcept
    {
        x--;
        x |= x >> 1;
        x |= x >> 2;
        x |= x >> 4;
        x |= x >> 8;
        x |= x >> 16;
        return x + 1;
    }

    constexpr unsigned PrevPowerOfTwo(unsigned x) noexcept
    {
        x |= x >> 1;
        x |= x >> 2;
        x |= x >> 4;
        x |= x >> 8;
        x |= x >> 16;
        return x - (x >> 1);
    }

    // Clamp01
    constexpr float Clamp01(float x) noexcept { return Clamp(x, 0.0f, 1.0f); }

    // Ping pong
    float PingPong(float t, float length) noexcept;

    // Repeat
    float Repeat(float t, float length) noexcept;

    // Move towards
    float MoveTowards(float current, float target, float maxDelta) noexcept;
    Vec2 MoveTowards(const Vec2& current, const Vec2& target, float maxDelta) noexcept;
    Vec3 MoveTowards(const Vec3& current, const Vec3& target, float maxDelta) noexcept;
    Vec4 MoveTowards(const Vec4& current, const Vec4& target, float maxDelta) noexcept;

    //============================================================
    // Vector math functions (delegated to DirectXMath)
    //============================================================

    // Dot product
    float Dot(const Vec2& a, const Vec2& b) noexcept;
    float Dot(const Vec3& a, const Vec3& b) noexcept;
    float Dot(const Vec4& a, const Vec4& b) noexcept;
    float Dot(const Quat& a, const Quat& b) noexcept;

    // Cross product
    Vec3 Cross(const Vec3& a, const Vec3& b) noexcept;

    // Length
    float Length(const Vec2& v) noexcept;
    float Length(const Vec3& v) noexcept;
    float Length(const Vec4& v) noexcept;
    float LengthSq(const Vec2& v) noexcept;
    float LengthSq(const Vec3& v) noexcept;
    float LengthSq(const Vec4& v) noexcept;

    // Normalize
    Vec2 Normalize(const Vec2& v) noexcept;
    Vec3 Normalize(const Vec3& v) noexcept;
    Vec4 Normalize(const Vec4& v) noexcept;
    Quat Normalize(const Quat& q) noexcept;

    // Distance
    float Distance(const Vec2& a, const Vec2& b) noexcept;
    float Distance(const Vec3& a, const Vec3& b) noexcept;
    float Distance(const Vec4& a, const Vec4& b) noexcept;
    float DistanceSq(const Vec2& a, const Vec2& b) noexcept;
    float DistanceSq(const Vec3& a, const Vec3& b) noexcept;
    float DistanceSq(const Vec4& a, const Vec4& b) noexcept;

    // Lerp
    Vec2 Lerp(const Vec2& a, const Vec2& b, float t) noexcept;
    Vec3 Lerp(const Vec3& a, const Vec3& b, float t) noexcept;
    Vec4 Lerp(const Vec4& a, const Vec4& b, float t) noexcept;
    Quat Lerp(const Quat& a, const Quat& b, float t) noexcept;

    // Slerp
    Quat Slerp(const Quat& a, const Quat& b, float t) noexcept;

    // Nlerp (normalized lerp)
    Quat Nlerp(const Quat& a, const Quat& b, float t) noexcept;

    // Reflect
    Vec2 Reflect(const Vec2& v, const Vec2& normal) noexcept;
    Vec3 Reflect(const Vec3& v, const Vec3& normal) noexcept;
    Vec4 Reflect(const Vec4& v, const Vec4& normal) noexcept;

    // Refract
    Vec3 Refract(const Vec3& v, const Vec3& normal, float eta) noexcept;

    // Project
    Vec3 Project(const Vec3& v, const Vec3& onNormal) noexcept;
    Vec3 ProjectOnPlane(const Vec3& v, const Vec3& planeNormal) noexcept;

    // Angle between vectors
    float Angle(const Vec2& a, const Vec2& b) noexcept;
    float Angle(const Vec3& a, const Vec3& b) noexcept;
    float SignedAngle(const Vec3& from, const Vec3& to, const Vec3& axis) noexcept;

    // Rotate
    Vec2 Rotate(const Vec2& v, float angle) noexcept;
    Vec3 Rotate(const Vec3& v, const Quat& q) noexcept;

    // Min/Max/Clamp
    Vec2 Min(const Vec2& a, const Vec2& b) noexcept;
    Vec3 Min(const Vec3& a, const Vec3& b) noexcept;
    Vec4 Min(const Vec4& a, const Vec4& b) noexcept;

    Vec2 Max(const Vec2& a, const Vec2& b) noexcept;
    Vec3 Max(const Vec3& a, const Vec3& b) noexcept;
    Vec4 Max(const Vec4& a, const Vec4& b) noexcept;

    Vec2 Clamp(const Vec2& v, const Vec2& min, const Vec2& max) noexcept;
    Vec3 Clamp(const Vec3& v, const Vec3& min, const Vec3& max) noexcept;
    Vec4 Clamp(const Vec4& v, const Vec4& min, const Vec4& max) noexcept;

    // Abs/Sign/Floor/Ceil/Round
    Vec2 Abs(const Vec2& v) noexcept;
    Vec3 Abs(const Vec3& v) noexcept;
    Vec4 Abs(const Vec4& v) noexcept;

    Vec2 Sign(const Vec2& v) noexcept;
    Vec3 Sign(const Vec3& v) noexcept;
    Vec4 Sign(const Vec4& v) noexcept;

    //============================================================
    // Matrix math functions
    //============================================================

    // Transpose
    Mat4x4 Transpose(const Mat4x4& m) noexcept;

    // Determinant
    float Determinant(const Mat4x4& m) noexcept;

    // Inverse
    Mat4x4 Inverse(const Mat4x4& m) noexcept;
    bool Inverse(const Mat4x4& m, Mat4x4& out) noexcept;

    // Multiply
    Mat4x4 Multiply(const Mat4x4& a, const Mat4x4& b) noexcept;

    // Transform
    Vec3 TransformPoint(const Mat4x4& m, const Vec3& v) noexcept;
    Vec3 TransformVector(const Mat4x4& m, const Vec3& v) noexcept;
    Vec3 TransformNormal(const Mat4x4& m, const Vec3& v) noexcept;
    Vec4 TransformVector(const Mat4x4& m, const Vec4& v) noexcept;

    // LookAt
    Mat4x4 LookAtLH(const Vec3& eye, const Vec3& at, const Vec3& up) noexcept;
    Mat4x4 LookAtRH(const Vec3& eye, const Vec3& at, const Vec3& up) noexcept;

    // Perspective
    Mat4x4 PerspectiveFovLH(float fovY, float aspect, float nearZ, float farZ) noexcept;
    Mat4x4 PerspectiveFovRH(float fovY, float aspect, float nearZ, float farZ) noexcept;

    // Orthographic
    Mat4x4 OrthographicLH(float width, float height, float nearZ, float farZ) noexcept;
    Mat4x4 OrthographicRH(float width, float height, float nearZ, float farZ) noexcept;
    Mat4x4 OrthographicOffCenterLH(float left, float right, float bottom, float top, float nearZ, float farZ) noexcept;
    Mat4x4 OrthographicOffCenterRH(float left, float right, float bottom, float top, float nearZ, float farZ) noexcept;

    // Decompose
    void Decompose(const Mat4x4& m, Vec3& scale, Quat& rotation, Vec3& translation) noexcept;

    //============================================================
    // Quaternion math functions
    //============================================================

    Quat Identity() noexcept;
    Quat Conjugate(const Quat& q) noexcept;
    Quat Inverse(const Quat& q) noexcept;
    float Angle(const Quat& a, const Quat& b) noexcept;

    Quat FromAxisAngle(const Vec3& axis, float angle) noexcept;
    Quat FromEuler(const Vec3& euler) noexcept; // pitch, yaw, roll
    Quat FromToRotation(const Vec3& from, const Vec3& to) noexcept;
    Quat LookRotation(const Vec3& forward, const Vec3& up = Vec3::Up()) noexcept;

    void ToAxisAngle(const Quat& q, Vec3& axis, float& angle) noexcept;
    Vec3 ToEuler(const Quat& q) noexcept; // Returns pitch, yaw, roll

    //============================================================
    // Color/Gamma math
    //============================================================

    constexpr float LinearToGamma(float linear) noexcept
    {
        return linear <= 0.0031308f ? 12.92f * linear : 1.055f * powf(linear, 1.0f / 2.4f) - 0.055f;
    }

    constexpr float GammaToLinear(float gamma) noexcept
    {
        return gamma <= 0.04045f ? gamma / 12.92f : powf((gamma + 0.055f) / 1.055f, 2.4f);
    }

    //============================================================
    // Random utilities
    //============================================================

    // Simple LCG for deterministic random
    struct Random
    {
        uint32_t seed;

        constexpr Random(uint32_t s = 1) noexcept : seed(s) {}

        uint32_t NextUInt() noexcept
        {
            seed = seed * 1664525 + 1013904223;
            return seed;
        }

        float NextFloat() noexcept
        {
            return NextUInt() * (1.0f / 4294967296.0f);
        }

        float NextFloat(float min, float max) noexcept
        {
            return min + NextFloat() * (max - min);
        }

        int NextInt(int min, int max) noexcept
        {
            return min + int(NextFloat() * (max - min));
        }

        Vec2 NextVec2() noexcept
        {
            return Vec2(NextFloat(), NextFloat());
        }

        Vec3 NextVec3() noexcept
        {
            return Vec3(NextFloat(), NextFloat(), NextFloat());
        }

        Vec3 NextVec3(float min, float max) noexcept
        {
            return Vec3(
                NextFloat(min, max),
                NextFloat(min, max),
                NextFloat(min, max)
            );
        }

        Vec3 NextUnitVector() noexcept
        {
            // Marsaglia method for random point on unit sphere
            float x1, x2, w;
            do
            {
                x1 = 2.0f * NextFloat() - 1.0f;
                x2 = 2.0f * NextFloat() - 1.0f;
                w = x1 * x1 + x2 * x2;
            } while (w >= 1.0f || w == 0.0f);

            float w2 = sqrtf((1.0f - w) / w);
            return Vec3(x1 * w2, x2 * w2, 1.0f - 2.0f * w);
        }
    };
} // namespace xg