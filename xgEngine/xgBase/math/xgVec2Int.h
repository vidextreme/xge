// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.

#pragma once

// xgVec2Int.h - 2D integer vector (grids, tiles, pixels, chunk coordinates)
// POD-friendly, trivially copyable, STL-free. 8 bytes, unaligned.

#include <cstdint>
#include "xgMathCommon.h"
#include "xgVec2.h"

namespace xg
{
    //============================================================
    // Integer helpers
    //============================================================

    // Division rounding towards negative infinity (-1 / 16 == -1)
    constexpr int FloorDiv(int a, int b) noexcept
    {
        int q = a / b;
        return (a % b != 0 && ((a < 0) != (b < 0))) ? q - 1 : q;
    }

    // Modulo with the sign of the divisor (-1 mod 16 == 15)
    constexpr int FloorMod(int a, int b) noexcept
    {
        return a - FloorDiv(a, b) * b;
    }

    //============================================================
    // Vec2Int
    //============================================================
    struct Vec2Int
    {
        int32_t x = 0;
        int32_t y = 0;

        constexpr Vec2Int() noexcept = default;
        constexpr Vec2Int(int32_t x_, int32_t y_) noexcept : x(x_), y(y_) {}
        constexpr explicit Vec2Int(int32_t v) noexcept : x(v), y(v) {}

        //--------------------------------------------------------
        // Access
        //--------------------------------------------------------
        constexpr int32_t& operator[](size_t i) noexcept { return (&x)[i]; }
        constexpr const int32_t& operator[](size_t i) const noexcept { return (&x)[i]; }

        //--------------------------------------------------------
        // Comparison
        //--------------------------------------------------------
        constexpr bool operator==(const Vec2Int& o) const noexcept { return x == o.x && y == o.y; }
        constexpr bool operator!=(const Vec2Int& o) const noexcept { return !(*this == o); }

        //--------------------------------------------------------
        // Arithmetic
        //--------------------------------------------------------
        constexpr Vec2Int operator+(const Vec2Int& o) const noexcept { return Vec2Int(x + o.x, y + o.y); }
        constexpr Vec2Int operator-(const Vec2Int& o) const noexcept { return Vec2Int(x - o.x, y - o.y); }
        constexpr Vec2Int operator*(const Vec2Int& o) const noexcept { return Vec2Int(x * o.x, y * o.y); }
        constexpr Vec2Int operator*(int32_t s) const noexcept { return Vec2Int(x * s, y * s); }
        constexpr Vec2Int operator/(int32_t s) const noexcept { return Vec2Int(x / s, y / s); }
        constexpr Vec2Int operator-() const noexcept { return Vec2Int(-x, -y); }

        constexpr Vec2Int& operator+=(const Vec2Int& o) noexcept { x += o.x; y += o.y; return *this; }
        constexpr Vec2Int& operator-=(const Vec2Int& o) noexcept { x -= o.x; y -= o.y; return *this; }
        constexpr Vec2Int& operator*=(int32_t s) noexcept { x *= s; y *= s; return *this; }

        //--------------------------------------------------------
        // Operations
        //--------------------------------------------------------
        constexpr int32_t LengthSq() const noexcept { return x * x + y * y; }
        float Length() const noexcept { return sqrtf(static_cast<float>(LengthSq())); }

        constexpr Vec2 ToVec2() const noexcept { return Vec2(static_cast<float>(x), static_cast<float>(y)); }

        // Floor division / modulo per component (world cell -> chunk, local cell)
        constexpr Vec2Int FloorDiv(int32_t d) const noexcept { return Vec2Int(xg::FloorDiv(x, d), xg::FloorDiv(y, d)); }
        constexpr Vec2Int FloorMod(int32_t d) const noexcept { return Vec2Int(xg::FloorMod(x, d), xg::FloorMod(y, d)); }

        constexpr uint32_t Hash() const noexcept
        {
            return static_cast<uint32_t>(x) * 73856093u ^ static_cast<uint32_t>(y) * 19349663u;
        }

        //--------------------------------------------------------
        // Static helpers
        //--------------------------------------------------------
        static constexpr Vec2Int Min(const Vec2Int& a, const Vec2Int& b) noexcept { return Vec2Int(xg::Min(a.x, b.x), xg::Min(a.y, b.y)); }
        static constexpr Vec2Int Max(const Vec2Int& a, const Vec2Int& b) noexcept { return Vec2Int(xg::Max(a.x, b.x), xg::Max(a.y, b.y)); }
        static constexpr Vec2Int Abs(const Vec2Int& v) noexcept { return Vec2Int(xg::Abs(v.x), xg::Abs(v.y)); }
        static constexpr Vec2Int Clamp(const Vec2Int& v, const Vec2Int& lo, const Vec2Int& hi) noexcept
        {
            return Vec2Int(xg::Clamp(v.x, lo.x, hi.x), xg::Clamp(v.y, lo.y, hi.y));
        }

        // |dx| + |dy| (4-way grid moves)
        static constexpr int32_t ManhattanDistance(const Vec2Int& a, const Vec2Int& b) noexcept
        {
            return xg::Abs(a.x - b.x) + xg::Abs(a.y - b.y);
        }

        // max(|dx|, |dy|) (8-way grid moves)
        static constexpr int32_t ChebyshevDistance(const Vec2Int& a, const Vec2Int& b) noexcept
        {
            return xg::Max(xg::Abs(a.x - b.x), xg::Abs(a.y - b.y));
        }

        static Vec2Int FloorToInt(const Vec2& v) noexcept { return Vec2Int(static_cast<int32_t>(floorf(v.x)), static_cast<int32_t>(floorf(v.y))); }
        static Vec2Int CeilToInt(const Vec2& v) noexcept { return Vec2Int(static_cast<int32_t>(ceilf(v.x)), static_cast<int32_t>(ceilf(v.y))); }
        static Vec2Int RoundToInt(const Vec2& v) noexcept { return Vec2Int(static_cast<int32_t>(roundf(v.x)), static_cast<int32_t>(roundf(v.y))); }

        static constexpr Vec2Int Zero() noexcept { return Vec2Int(0, 0); }
        static constexpr Vec2Int One() noexcept { return Vec2Int(1, 1); }
        static constexpr Vec2Int Up() noexcept { return Vec2Int(0, 1); }
        static constexpr Vec2Int Down() noexcept { return Vec2Int(0, -1); }
        static constexpr Vec2Int Left() noexcept { return Vec2Int(-1, 0); }
        static constexpr Vec2Int Right() noexcept { return Vec2Int(1, 0); }
    };

    static_assert(std::is_standard_layout_v<Vec2Int>, "Vec2Int must be standard layout");
    static_assert(std::is_trivially_copyable_v<Vec2Int>, "Vec2Int must be trivially copyable");
    static_assert(sizeof(Vec2Int) == 8, "Vec2Int must be 2 ints");

    constexpr Vec2Int operator*(int32_t s, const Vec2Int& v) noexcept { return v * s; }

    inline Vec2Int FloorToVec2Int(const Vec2& v) noexcept { return Vec2Int::FloorToInt(v); }
} // namespace xg
