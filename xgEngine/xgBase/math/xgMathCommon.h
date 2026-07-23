// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.

#pragma once

// xgMathCommon.h - DirectXMath integration layer
// Only this file may include DirectXMath.h directly.

// Ensure C++20
#if __cplusplus < 202002L
    #error "XGE Math requires C++20 or later"
#endif

// DirectXMath inclusion - ONLY in this header
#include <DirectXMath.h>

// Ensure DirectXMath is using SSE/SSE2/AVX intrinsics
#if !defined(_XM_NO_INTRINSICS_)
    #define _XM_NO_INTRINSICS_ 0
#endif

// Ensure SSE2 minimum
#if !defined(_XM_SSE2_)
    #define _XM_SSE2_ 1
#endif

// XGE-native alias names for internal use only
namespace xg::detail
{
    using namespace DirectX;

    // XGE-branded DirectXMath type aliases
    using XGVector     = XMVECTOR;
    using XGMatrix     = XMMATRIX;

    using XGFloat2     = XMFLOAT2;
    using XGFloat3     = XMFLOAT3;
    using XGFloat4     = XMFLOAT4;
    using XGFloat4x4   = XMFLOAT4X4;

    using XGQuat       = XMVECTOR;   // Quaternions are XMVECTOR in DirectXMath
}

// Mathematical constants
namespace xg::constants
{
    constexpr float PI = 3.14159265358979323846f;
    constexpr float TWO_PI = 2.0f * PI;
    constexpr float HALF_PI = PI * 0.5f;
    constexpr float QUARTER_PI = PI * 0.25f;
    constexpr float INV_PI = 1.0f / PI;
    constexpr float INV_TWO_PI = 1.0f / TWO_PI;
    constexpr float DEG_TO_RAD = PI / 180.0f;
    constexpr float RAD_TO_DEG = 180.0f / PI;
    constexpr float EPSILON = 1.192092896e-07f; // FLT_EPSILON
    constexpr float EPSILON_2 = 2.2204460492503131e-16; // DBL_EPSILON approx as float
    constexpr float INFINITY_F = 3.402823466e+38f; // FLT_MAX
    constexpr float SQRT_2 = 1.4142135623730951f;
    constexpr float INV_SQRT_2 = 0.7071067811865475f;
    constexpr float SQRT_3 = 1.7320508075688772f;
    constexpr float INV_SQRT_3 = 0.5773502691896258f;

    // Degree constants
    constexpr float DEG_90 = 90.0f;
    constexpr float DEG_180 = 180.0f;
    constexpr float DEG_270 = 270.0f;
    constexpr float DEG_360 = 360.0f;

    // Epsilon comparisons
    constexpr float EPSILON_POS = 1e-6f;
    constexpr float EPSILON_NEG = -1e-6f;

    // Float limits
    constexpr float MAX_FLOAT = 3.402823466e+38f;
    constexpr float MIN_FLOAT = -3.402823466e+38f;
    constexpr float SMALLEST_NORMAL = 1.175494351e-38f; // FLT_MIN
} // namespace xg::constants

// Common math utilities namespace
namespace xg
{
    using constants::PI;
    using constants::TWO_PI;
    using constants::HALF_PI;
    using constants::QUARTER_PI;
    using constants::INV_PI;
    using constants::INV_TWO_PI;
    using constants::DEG_TO_RAD;
    using constants::RAD_TO_DEG;
    using constants::EPSILON;
    using constants::EPSILON_POS;
    using constants::EPSILON_NEG;
    using constants::INFINITY_F;
    using constants::MAX_FLOAT;
    using constants::MIN_FLOAT;
    using constants::SMALLEST_NORMAL;

    // Degree/Radian conversion (constexpr)
    constexpr float DegToRad(float degrees) noexcept { return degrees * DEG_TO_RAD; }
    constexpr float RadToDeg(float radians) noexcept { return radians * RAD_TO_DEG; }

    // Fast approximations
    constexpr float FastSin(float x) noexcept;
    constexpr float FastCos(float x) noexcept;

    // Epsilon comparisons
    constexpr bool IsZero(float x, float epsilon = EPSILON_POS) noexcept { return x > -epsilon && x < epsilon; }
    constexpr bool IsEqual(float a, float b, float epsilon = EPSILON_POS) noexcept { return (a - b > -epsilon) && (a - b < epsilon); }
    constexpr bool IsZero(double x, double epsilon = 1e-12) noexcept { return x > -epsilon && x < epsilon; }
    constexpr bool IsEqual(double a, double b, double epsilon = 1e-12) noexcept { return (a - b > -epsilon) && (a - b < epsilon); }

    // Fast math approximations (constexpr where possible)
    constexpr float FastSin(float x) noexcept
    {
        // Fast sine approximation using Bhaskara I's approximation
        // Valid for [0, PI], extend using symmetry
        const float x2 = x * x;
        return (16.0f * x * (PI - x)) / (5.0f * PI * PI - 4.0f * x * (PI - x));
    }

    constexpr float FastCos(float x) noexcept
    {
        return FastSin(x + HALF_PI);
    }

    // Fast inverse square root (Quake-style, constexpr-compatible version)
    constexpr float FastInvSqrt(float x) noexcept
    {
        if (x <= 0.0f) return INFINITY_F;
        const float xhalf = 0.5f * x;
        int i = *reinterpret_cast<const int*>(&x); // Evil floating point bit level hacking
        i = 0x5f3759df - (i >> 1); // What the fuck?
        float y = *reinterpret_cast<float*>(&i);
        y = y * (1.5f - xhalf * y * y); // 1st iteration
        // y = y * (1.5f - xhalf * y * y); // 2nd iteration (optional)
        return y;
    }

    // Branchless min/max/clamp
    constexpr float Min(float a, float b) noexcept { return (a < b) ? a : b; }
    constexpr float Max(float a, float b) noexcept { return (a > b) ? a : b; }
    constexpr float Clamp(float x, float min, float max) noexcept { return Min(Max(x, min), max); }

    // Integer versions
    constexpr int Min(int a, int b) noexcept { return (a < b) ? a : b; }
    constexpr int Max(int a, int b) noexcept { return (a > b) ? a : b; }
    constexpr int Clamp(int x, int min, int max) noexcept { return Min(Max(x, min), max); }

    // Unsigned versions
    constexpr unsigned Min(unsigned a, unsigned b) noexcept { return (a < b) ? a : b; }
    constexpr unsigned Max(unsigned a, unsigned b) noexcept { return (a > b) ? a : b; }
    constexpr unsigned Clamp(unsigned x, unsigned min, unsigned max) noexcept { return Min(Max(x, min), max); }

    // Size_t versions
    constexpr size_t Min(size_t a, size_t b) noexcept { return (a < b) ? a : b; }
    constexpr size_t Max(size_t a, size_t b) noexcept { return (a > b) ? a : b; }
    constexpr size_t Clamp(size_t x, size_t min, size_t max) noexcept { return Min(Max(x, min), max); }

    // Lerp (linear interpolation)
    constexpr float Lerp(float a, float b, float t) noexcept { return a + (b - a) * t; }
    constexpr float LerpClamped(float a, float b, float t) noexcept { return Lerp(a, b, Clamp(t, 0.0f, 1.0f)); }

    // Smoothstep
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

    // Sign function
    constexpr float Sign(float x) noexcept { return (x > 0.0f) - (x < 0.0f); }
    constexpr int Sign(int x) noexcept { return (x > 0) - (x < 0); }

    // Absolute value
    constexpr float Abs(float x) noexcept { return x < 0.0f ? -x : x; }
    constexpr int Abs(int x) noexcept { return x < 0 ? -x : x; }
    constexpr double Abs(double x) noexcept { return x < 0.0 ? -x : x; }

    // Floor/Ceil/Round (constexpr approximations)
    constexpr int FloorToInt(float x) noexcept { return static_cast<int>(x >= 0.0f ? x : x - 0.9999999f); }
    constexpr int CeilToInt(float x) noexcept { return static_cast<int>(x >= 0.0f ? x + 0.9999999f : x); }
    constexpr int RoundToInt(float x) noexcept { return static_cast<int>(x >= 0.0f ? x + 0.5f : x - 0.5f); }

    // Power of 2 utilities
    constexpr bool IsPowerOfTwo(unsigned x) noexcept { return x && !(x & (x - 1)); }
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

    // Swap
    template<typename T>
    constexpr void Swap(T& a, T& b) noexcept
    {
        T tmp = std::move(a);
        a = std::move(b);
        b = std::move(tmp);
    }

    // Modulo that works correctly for negative numbers
    constexpr int Mod(int x, int m) noexcept { return (x % m + m) % m; }
    float FMod(float x, float m) noexcept { return fmodf(fmodf(x, m) + m, m); }

    // Linear to gamma and gamma to linear (sRGB)
    constexpr float LinearToGamma(float linear) noexcept { return linear <= 0.0031308f ? 12.92f * linear : 1.055f * powf(linear, 1.0f / 2.4f) - 0.055f; }
    constexpr float GammaToLinear(float gamma) noexcept { return gamma <= 0.04045f ? gamma / 12.92f : powf((gamma + 0.055f) / 1.055f, 2.4f); }

    // Radians/degrees constants
    constexpr float Deg90 = constants::DEG_90;
    constexpr float Deg180 = constants::DEG_180;
    constexpr float Deg270 = constants::DEG_270;
    constexpr float Deg360 = constants::DEG_360;
} // namespace xg

// Forward declarations of math types (defined in their own headers)
namespace xg
{
    struct Vec2;
    struct Vec3;
    struct Vec4;
    struct Quat;
    struct Mat4x4;
    struct Transform;
    struct AABB;
    struct Sphere;
    struct Ray;
}

// Include the DirectXMath implementation inline file
// This must be included AFTER forward declarations
#include "__xgDirectXMath.inl"