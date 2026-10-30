// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.

#pragma once

// xgRect.h - 2D rectangles
// Rect    : float min/max (UI, sprites, 2D bounds, viewports)
// RectInt : integer min (inclusive) / max (exclusive) (pixels, tiles, scissor rects)

#include "xgMathCommon.h"
#include "xgVec2.h"
#include "xgVec2Int.h"

namespace xg
{
    //============================================================
    // Rect
    //============================================================
    struct Rect
    {
        Vec2 min;
        Vec2 max;

        //--------------------------------------------------------
        // Constructors
        //--------------------------------------------------------
        constexpr Rect() noexcept = default;
        constexpr Rect(const Vec2& min_, const Vec2& max_) noexcept : min{ min_ }, max{ max_ } {}

        static constexpr Rect FromXYWH(float x, float y, float width, float height) noexcept
        {
            return Rect(Vec2(x, y), Vec2(x + width, y + height));
        }

        static constexpr Rect FromCenterSize(const Vec2& center, const Vec2& size) noexcept
        {
            return Rect(center - size * 0.5f, center + size * 0.5f);
        }

        // Normalizes so that min <= max
        static constexpr Rect FromPoints(const Vec2& a, const Vec2& b) noexcept
        {
            return Rect(Vec2::Min(a, b), Vec2::Max(a, b));
        }

        constexpr bool operator==(const Rect& o) const noexcept { return min == o.min && max == o.max; }
        constexpr bool operator!=(const Rect& o) const noexcept { return !(*this == o); }

        //--------------------------------------------------------
        // Properties
        //--------------------------------------------------------
        constexpr float X() const noexcept { return min.x; }
        constexpr float Y() const noexcept { return min.y; }
        constexpr float Width() const noexcept { return max.x - min.x; }
        constexpr float Height() const noexcept { return max.y - min.y; }
        constexpr Vec2 Size() const noexcept { return max - min; }
        constexpr Vec2 Center() const noexcept { return (min + max) * 0.5f; }
        constexpr float Area() const noexcept { return Width() * Height(); }
        constexpr bool IsValid() const noexcept { return min.x <= max.x && min.y <= max.y; }

        //--------------------------------------------------------
        // Queries
        //--------------------------------------------------------
        constexpr bool Contains(const Vec2& p) const noexcept
        {
            return p.x >= min.x && p.x <= max.x && p.y >= min.y && p.y <= max.y;
        }

        constexpr bool Contains(const Rect& r) const noexcept
        {
            return r.min.x >= min.x && r.max.x <= max.x && r.min.y >= min.y && r.max.y <= max.y;
        }

        constexpr bool Overlaps(const Rect& r) const noexcept
        {
            return min.x <= r.max.x && max.x >= r.min.x && min.y <= r.max.y && max.y >= r.min.y;
        }

        // Overlapping region; result.IsValid() is false when they don't overlap
        constexpr Rect Intersection(const Rect& r) const noexcept
        {
            return Rect(Vec2::Max(min, r.min), Vec2::Min(max, r.max));
        }

        constexpr Rect Union(const Rect& r) const noexcept
        {
            return Rect(Vec2::Min(min, r.min), Vec2::Max(max, r.max));
        }

        constexpr Vec2 ClosestPoint(const Vec2& p) const noexcept
        {
            return Vec2::Clamp(p, min, max);
        }

        //--------------------------------------------------------
        // Modification
        //--------------------------------------------------------
        constexpr void Encapsulate(const Vec2& p) noexcept
        {
            min = Vec2::Min(min, p);
            max = Vec2::Max(max, p);
        }

        constexpr void Encapsulate(const Rect& r) noexcept
        {
            min = Vec2::Min(min, r.min);
            max = Vec2::Max(max, r.max);
        }

        constexpr Rect Expanded(float amount) const noexcept
        {
            return Rect(min - Vec2(amount, amount), max + Vec2(amount, amount));
        }

        constexpr Rect Expanded(const Vec2& amount) const noexcept
        {
            return Rect(min - amount, max + amount);
        }

        //--------------------------------------------------------
        // Normalized coordinates (UI anchoring): (0,0) = min, (1,1) = max
        //--------------------------------------------------------
        constexpr Vec2 NormalizedToPoint(const Vec2& n) const noexcept
        {
            return Vec2(Lerp(min.x, max.x, n.x), Lerp(min.y, max.y, n.y));
        }

        constexpr Vec2 PointToNormalized(const Vec2& p) const noexcept
        {
            float w = Width();
            float h = Height();
            return Vec2(w != 0.0f ? (p.x - min.x) / w : 0.0f, h != 0.0f ? (p.y - min.y) / h : 0.0f);
        }

        static constexpr Rect Zero() noexcept { return Rect(Vec2(0.0f, 0.0f), Vec2(0.0f, 0.0f)); }
        static constexpr Rect Unit() noexcept { return Rect(Vec2(0.0f, 0.0f), Vec2(1.0f, 1.0f)); }
    };

    static_assert(std::is_trivially_copyable_v<Rect>, "Rect must be trivially copyable");
    static_assert(sizeof(Rect) == 2 * sizeof(Vec2), "Rect must be 2 Vec2");

    //============================================================
    // RectInt - min inclusive, max exclusive
    //============================================================
    struct RectInt
    {
        Vec2Int min;
        Vec2Int max;

        constexpr RectInt() noexcept = default;
        constexpr RectInt(const Vec2Int& min_, const Vec2Int& max_) noexcept : min{ min_ }, max{ max_ } {}

        static constexpr RectInt FromXYWH(int32_t x, int32_t y, int32_t width, int32_t height) noexcept
        {
            return RectInt(Vec2Int(x, y), Vec2Int(x + width, y + height));
        }

        constexpr bool operator==(const RectInt& o) const noexcept { return min == o.min && max == o.max; }
        constexpr bool operator!=(const RectInt& o) const noexcept { return !(*this == o); }

        constexpr int32_t X() const noexcept { return min.x; }
        constexpr int32_t Y() const noexcept { return min.y; }
        constexpr int32_t Width() const noexcept { return max.x - min.x; }
        constexpr int32_t Height() const noexcept { return max.y - min.y; }
        constexpr Vec2Int Size() const noexcept { return max - min; }
        constexpr int32_t Area() const noexcept { return Width() * Height(); }
        constexpr bool IsEmpty() const noexcept { return max.x <= min.x || max.y <= min.y; }

        constexpr bool Contains(const Vec2Int& p) const noexcept
        {
            return p.x >= min.x && p.x < max.x && p.y >= min.y && p.y < max.y;
        }

        constexpr bool Overlaps(const RectInt& r) const noexcept
        {
            return min.x < r.max.x && max.x > r.min.x && min.y < r.max.y && max.y > r.min.y;
        }

        constexpr RectInt Intersection(const RectInt& r) const noexcept
        {
            return RectInt(Vec2Int::Max(min, r.min), Vec2Int::Min(max, r.max));
        }

        constexpr RectInt Union(const RectInt& r) const noexcept
        {
            return RectInt(Vec2Int::Min(min, r.min), Vec2Int::Max(max, r.max));
        }

        constexpr Vec2Int ClampPoint(const Vec2Int& p) const noexcept
        {
            return Vec2Int::Clamp(p, min, max - Vec2Int(1, 1));
        }

        constexpr Rect ToRect() const noexcept { return Rect(min.ToVec2(), max.ToVec2()); }
    };

    static_assert(std::is_trivially_copyable_v<RectInt>, "RectInt must be trivially copyable");
    static_assert(sizeof(RectInt) == 16, "RectInt must be 4 ints");
} // namespace xg
