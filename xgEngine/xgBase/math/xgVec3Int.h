// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.

#pragma once

// xgVec3Int.h - 3D integer vector (voxels, 3D grids, chunk coordinates)
// POD-friendly, trivially copyable, STL-free. 12 bytes, unaligned.

#include <cstdint>
#include "xgMathCommon.h"
#include "xgVec3.h"
#include "xgVec2Int.h"

namespace xg
{
    //============================================================
    // Vec3Int
    //============================================================
    struct Vec3Int
    {
        int32_t x = 0;
        int32_t y = 0;
        int32_t z = 0;

        constexpr Vec3Int() noexcept = default;
        constexpr Vec3Int(int32_t x_, int32_t y_, int32_t z_) noexcept : x(x_), y(y_), z(z_) {}
        constexpr explicit Vec3Int(int32_t v) noexcept : x(v), y(v), z(v) {}
        constexpr Vec3Int(const Vec2Int& xy, int32_t z_) noexcept : x(xy.x), y(xy.y), z(z_) {}

        //--------------------------------------------------------
        // Access
        //--------------------------------------------------------
        constexpr int32_t& operator[](size_t i) noexcept { return (&x)[i]; }
        constexpr const int32_t& operator[](size_t i) const noexcept { return (&x)[i]; }

        //--------------------------------------------------------
        // Comparison
        //--------------------------------------------------------
        constexpr bool operator==(const Vec3Int& o) const noexcept { return x == o.x && y == o.y && z == o.z; }
        constexpr bool operator!=(const Vec3Int& o) const noexcept { return !(*this == o); }

        //--------------------------------------------------------
        // Arithmetic
        //--------------------------------------------------------
        constexpr Vec3Int operator+(const Vec3Int& o) const noexcept { return Vec3Int(x + o.x, y + o.y, z + o.z); }
        constexpr Vec3Int operator-(const Vec3Int& o) const noexcept { return Vec3Int(x - o.x, y - o.y, z - o.z); }
        constexpr Vec3Int operator*(const Vec3Int& o) const noexcept { return Vec3Int(x * o.x, y * o.y, z * o.z); }
        constexpr Vec3Int operator*(int32_t s) const noexcept { return Vec3Int(x * s, y * s, z * s); }
        constexpr Vec3Int operator/(int32_t s) const noexcept { return Vec3Int(x / s, y / s, z / s); }
        constexpr Vec3Int operator-() const noexcept { return Vec3Int(-x, -y, -z); }

        constexpr Vec3Int& operator+=(const Vec3Int& o) noexcept { x += o.x; y += o.y; z += o.z; return *this; }
        constexpr Vec3Int& operator-=(const Vec3Int& o) noexcept { x -= o.x; y -= o.y; z -= o.z; return *this; }
        constexpr Vec3Int& operator*=(int32_t s) noexcept { x *= s; y *= s; z *= s; return *this; }

        //--------------------------------------------------------
        // Operations
        //--------------------------------------------------------
        constexpr int32_t LengthSq() const noexcept { return x * x + y * y + z * z; }
        float Length() const noexcept { return sqrtf(static_cast<float>(LengthSq())); }

        constexpr Vec3 ToVec3() const noexcept { return Vec3(static_cast<float>(x), static_cast<float>(y), static_cast<float>(z)); }
        constexpr Vec2Int XY() const noexcept { return Vec2Int(x, y); }

        constexpr Vec3Int FloorDiv(int32_t d) const noexcept { return Vec3Int(xg::FloorDiv(x, d), xg::FloorDiv(y, d), xg::FloorDiv(z, d)); }
        constexpr Vec3Int FloorMod(int32_t d) const noexcept { return Vec3Int(xg::FloorMod(x, d), xg::FloorMod(y, d), xg::FloorMod(z, d)); }

        constexpr uint32_t Hash() const noexcept
        {
            return static_cast<uint32_t>(x) * 73856093u ^ static_cast<uint32_t>(y) * 19349663u ^ static_cast<uint32_t>(z) * 83492791u;
        }

        //--------------------------------------------------------
        // Static helpers
        //--------------------------------------------------------
        static constexpr Vec3Int Min(const Vec3Int& a, const Vec3Int& b) noexcept { return Vec3Int(xg::Min(a.x, b.x), xg::Min(a.y, b.y), xg::Min(a.z, b.z)); }
        static constexpr Vec3Int Max(const Vec3Int& a, const Vec3Int& b) noexcept { return Vec3Int(xg::Max(a.x, b.x), xg::Max(a.y, b.y), xg::Max(a.z, b.z)); }
        static constexpr Vec3Int Abs(const Vec3Int& v) noexcept { return Vec3Int(xg::Abs(v.x), xg::Abs(v.y), xg::Abs(v.z)); }
        static constexpr Vec3Int Clamp(const Vec3Int& v, const Vec3Int& lo, const Vec3Int& hi) noexcept
        {
            return Vec3Int(xg::Clamp(v.x, lo.x, hi.x), xg::Clamp(v.y, lo.y, hi.y), xg::Clamp(v.z, lo.z, hi.z));
        }

        static constexpr int32_t ManhattanDistance(const Vec3Int& a, const Vec3Int& b) noexcept
        {
            return xg::Abs(a.x - b.x) + xg::Abs(a.y - b.y) + xg::Abs(a.z - b.z);
        }

        static constexpr int32_t ChebyshevDistance(const Vec3Int& a, const Vec3Int& b) noexcept
        {
            return xg::Max(xg::Max(xg::Abs(a.x - b.x), xg::Abs(a.y - b.y)), xg::Abs(a.z - b.z));
        }

        static Vec3Int FloorToInt(const Vec3& v) noexcept
        {
            return Vec3Int(static_cast<int32_t>(floorf(v.x)), static_cast<int32_t>(floorf(v.y)), static_cast<int32_t>(floorf(v.z)));
        }
        static Vec3Int CeilToInt(const Vec3& v) noexcept
        {
            return Vec3Int(static_cast<int32_t>(ceilf(v.x)), static_cast<int32_t>(ceilf(v.y)), static_cast<int32_t>(ceilf(v.z)));
        }
        static Vec3Int RoundToInt(const Vec3& v) noexcept
        {
            return Vec3Int(static_cast<int32_t>(roundf(v.x)), static_cast<int32_t>(roundf(v.y)), static_cast<int32_t>(roundf(v.z)));
        }

        static constexpr Vec3Int Zero() noexcept { return Vec3Int(0, 0, 0); }
        static constexpr Vec3Int One() noexcept { return Vec3Int(1, 1, 1); }
        static constexpr Vec3Int Up() noexcept { return Vec3Int(0, 1, 0); }
        static constexpr Vec3Int Down() noexcept { return Vec3Int(0, -1, 0); }
        static constexpr Vec3Int Left() noexcept { return Vec3Int(-1, 0, 0); }
        static constexpr Vec3Int Right() noexcept { return Vec3Int(1, 0, 0); }
        static constexpr Vec3Int Forward() noexcept { return Vec3Int(0, 0, 1); }
        static constexpr Vec3Int Back() noexcept { return Vec3Int(0, 0, -1); }
    };

    static_assert(std::is_standard_layout_v<Vec3Int>, "Vec3Int must be standard layout");
    static_assert(std::is_trivially_copyable_v<Vec3Int>, "Vec3Int must be trivially copyable");
    static_assert(sizeof(Vec3Int) == 12, "Vec3Int must be 3 ints");

    constexpr Vec3Int operator*(int32_t s, const Vec3Int& v) noexcept { return v * s; }

    inline Vec3Int FloorToVec3Int(const Vec3& v) noexcept { return Vec3Int::FloorToInt(v); }
} // namespace xg
