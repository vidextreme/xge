// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.

#pragma once

// xgRandom.h - Deterministic random numbers and hashing
// Pcg32 is PCG-XSH-RR 64/32 (O'Neill, pcg-random.org): small, fast,
// statistically strong, seedable, with independent streams.
// The C# Pcg32 in xgSharedKit.Managed produces the exact same sequence.

#include <cstdint>
#include "xgMathCommon.h"
#include "xgVec2.h"
#include "xgVec3.h"
#include "xgQuat.h"

namespace xg
{
    //============================================================
    // Pcg32
    //============================================================
    struct Pcg32
    {
        static constexpr uint64_t Multiplier = 6364136223846793005ULL;
        static constexpr uint64_t DefaultStream = 0xda3e39cb94b95bdbULL;

        uint64_t State = 0x853c49e6748fea9bULL;
        uint64_t Increment = DefaultStream;

        constexpr Pcg32() noexcept = default;

        // Same seeding as the reference pcg32_srandom_r(seed, stream)
        constexpr explicit Pcg32(uint64_t seed, uint64_t stream = 54u) noexcept
        {
            Seed(seed, stream);
        }

        constexpr void Seed(uint64_t seed, uint64_t stream = 54u) noexcept
        {
            State = 0u;
            Increment = (stream << 1u) | 1u;
            NextUInt();
            State += seed;
            NextUInt();
        }

        //--------------------------------------------------------
        // Integers
        //--------------------------------------------------------
        constexpr uint32_t NextUInt() noexcept
        {
            uint64_t old = State;
            State = old * Multiplier + Increment;
            uint32_t xorShifted = static_cast<uint32_t>(((old >> 18u) ^ old) >> 27u);
            uint32_t rot = static_cast<uint32_t>(old >> 59u);
            return (xorShifted >> rot) | (xorShifted << ((0u - rot) & 31u));
        }

        // Unbiased value in [0, bound) (Lemire's multiply-shift with rejection)
        constexpr uint32_t NextUInt(uint32_t bound) noexcept
        {
            if (bound == 0u)
                return 0u;
            uint64_t m = static_cast<uint64_t>(NextUInt()) * bound;
            uint32_t low = static_cast<uint32_t>(m);
            if (low < bound)
            {
                uint32_t threshold = (0u - bound) % bound;
                while (low < threshold)
                {
                    m = static_cast<uint64_t>(NextUInt()) * bound;
                    low = static_cast<uint32_t>(m);
                }
            }
            return static_cast<uint32_t>(m >> 32u);
        }

        // Integer in [min, maxExclusive)
        constexpr int Range(int min, int maxExclusive) noexcept
        {
            if (maxExclusive <= min)
                return min;
            return min + static_cast<int>(NextUInt(static_cast<uint32_t>(maxExclusive - min)));
        }

        //--------------------------------------------------------
        // Floats
        //--------------------------------------------------------

        // Uniform float in [0, 1) using the top 24 bits
        constexpr float NextFloat() noexcept
        {
            return static_cast<float>(NextUInt() >> 8u) * (1.0f / 16777216.0f);
        }

        // Uniform float in [min, max)
        constexpr float Range(float min, float max) noexcept
        {
            return min + NextFloat() * (max - min);
        }

        constexpr bool Chance(float probability) noexcept
        {
            return NextFloat() < probability;
        }

        constexpr float Sign() noexcept
        {
            return (NextUInt() & 1u) ? 1.0f : -1.0f;
        }

        // Normal distribution (Marsaglia polar method)
        float Gaussian(float mean = 0.0f, float stdDev = 1.0f) noexcept
        {
            float u, v, s;
            do
            {
                u = NextFloat() * 2.0f - 1.0f;
                v = NextFloat() * 2.0f - 1.0f;
                s = u * u + v * v;
            } while (s >= 1.0f || s == 0.0f);
            return mean + stdDev * u * sqrtf(-2.0f * logf(s) / s);
        }

        //--------------------------------------------------------
        // Geometric samples
        //--------------------------------------------------------
        Vec2 InsideUnitCircle() noexcept
        {
            Vec2 p;
            do
            {
                p = Vec2(NextFloat() * 2.0f - 1.0f, NextFloat() * 2.0f - 1.0f);
            } while (p.LengthSq() > 1.0f);
            return p;
        }

        Vec2 OnUnitCircle() noexcept
        {
            float a = NextFloat() * TWO_PI;
            return Vec2(cosf(a), sinf(a));
        }

        Vec3 InsideUnitSphere() noexcept
        {
            Vec3 p;
            do
            {
                p = Vec3(NextFloat() * 2.0f - 1.0f, NextFloat() * 2.0f - 1.0f, NextFloat() * 2.0f - 1.0f);
            } while (p.LengthSq() > 1.0f);
            return p;
        }

        // Uniform direction (Marsaglia 1972)
        Vec3 OnUnitSphere() noexcept
        {
            float x1, x2, w;
            do
            {
                x1 = NextFloat() * 2.0f - 1.0f;
                x2 = NextFloat() * 2.0f - 1.0f;
                w = x1 * x1 + x2 * x2;
            } while (w >= 1.0f);
            float s = 2.0f * sqrtf(1.0f - w);
            return Vec3(x1 * s, x2 * s, 1.0f - 2.0f * w);
        }

        // Uniform direction within halfAngle (radians) of direction
        Vec3 InCone(const Vec3& direction, float halfAngle) noexcept
        {
            Vec3 axis = direction.Normalized();
            float cosTheta = Lerp(cosf(halfAngle), 1.0f, NextFloat());
            float sinTheta = sqrtf(Max(0.0f, 1.0f - cosTheta * cosTheta));
            float phi = NextFloat() * TWO_PI;

            // Orthonormal basis around axis
            Vec3 helper = (Abs(axis.x) < 0.9f) ? Vec3::UnitX() : Vec3::UnitY();
            Vec3 tangent = helper.Cross(axis).Normalized();
            Vec3 bitangent = axis.Cross(tangent);

            return tangent * (cosf(phi) * sinTheta) + bitangent * (sinf(phi) * sinTheta) + axis * cosTheta;
        }

        // Uniform point inside triangle abc
        Vec3 InTriangle(const Vec3& a, const Vec3& b, const Vec3& c) noexcept
        {
            float r1 = NextFloat();
            float r2 = NextFloat();
            if (r1 + r2 > 1.0f)
            {
                r1 = 1.0f - r1;
                r2 = 1.0f - r2;
            }
            return a + (b - a) * r1 + (c - a) * r2;
        }

        // Uniform random rotation (Shoemake)
        Quat Rotation() noexcept
        {
            float u1 = NextFloat();
            float u2 = NextFloat() * TWO_PI;
            float u3 = NextFloat() * TWO_PI;
            float a = sqrtf(1.0f - u1);
            float b = sqrtf(u1);
            return Quat(a * sinf(u2), a * cosf(u2), b * sinf(u3), b * cosf(u3));
        }

        //--------------------------------------------------------
        // Collections
        //--------------------------------------------------------

        // Fisher-Yates shuffle
        template<typename T>
        void Shuffle(T* items, int count) noexcept
        {
            for (int i = count - 1; i > 0; --i)
            {
                int j = static_cast<int>(NextUInt(static_cast<uint32_t>(i + 1)));
                T tmp = items[i];
                items[i] = items[j];
                items[j] = tmp;
            }
        }

        // Picks an index with probability proportional to weights[i] (negative weights count as 0).
        // Returns -1 if all weights are zero.
        int WeightedIndex(const float* weights, int count) noexcept
        {
            float total = 0.0f;
            for (int i = 0; i < count; ++i)
                total += Max(0.0f, weights[i]);
            if (total <= 0.0f)
                return -1;

            float r = NextFloat() * total;
            int last = -1;
            for (int i = 0; i < count; ++i)
            {
                float w = Max(0.0f, weights[i]);
                if (w <= 0.0f)
                    continue;
                last = i;
                if (r < w)
                    return i;
                r -= w;
            }
            return last; // float rounding fallback
        }
    };

    static_assert(sizeof(Pcg32) == 16, "Pcg32 must be 16 bytes (C# mirror)");

    //============================================================
    // Stateless hashing (procedural content, per-cell randomness)
    //============================================================

    // PCG hash (Jarzynski & Olano 2020)
    constexpr uint32_t Hash32(uint32_t x) noexcept
    {
        uint32_t state = x * 747796405u + 2891336453u;
        uint32_t word = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;
        return (word >> 22u) ^ word;
    }

    constexpr uint32_t Hash(int x, uint32_t seed = 0u) noexcept
    {
        return Hash32(static_cast<uint32_t>(x) ^ Hash32(seed));
    }

    constexpr uint32_t Hash(int x, int y, uint32_t seed = 0u) noexcept
    {
        return Hash32(static_cast<uint32_t>(y) ^ Hash(x, seed));
    }

    constexpr uint32_t Hash(int x, int y, int z, uint32_t seed = 0u) noexcept
    {
        return Hash32(static_cast<uint32_t>(z) ^ Hash(x, y, seed));
    }

    // Maps a hash to [0, 1)
    constexpr float HashToFloat(uint32_t hash) noexcept
    {
        return static_cast<float>(hash >> 8u) * (1.0f / 16777216.0f);
    }
} // namespace xg
