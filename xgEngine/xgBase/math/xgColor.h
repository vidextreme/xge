// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.

#pragma once

// xgColor.h - Color types
// Color   : float RGBA, 16 bytes, aligned for SIMD. Store LINEAR values for lighting/blending.
// Color32 : 8-bit RGBA, 4 bytes. Usually sRGB-encoded (textures, UI, vertex colors).
// Conversions between the two never apply gamma implicitly; use ToSRGB()/ToLinear().

#include <cstdint>
#include "xgMathCommon.h"
#include "xgVec3.h"
#include "xgVec4.h"

namespace xg
{
    struct Color32;

    //============================================================
    // Color - float RGBA
    //============================================================
    struct alignas(16) Color
    {
        float r = 0.0f;
        float g = 0.0f;
        float b = 0.0f;
        float a = 1.0f;

        //--------------------------------------------------------
        // Constructors
        //--------------------------------------------------------
        constexpr Color() noexcept = default;
        constexpr Color(float r_, float g_, float b_, float a_ = 1.0f) noexcept : r(r_), g(g_), b(b_), a(a_) {}
        constexpr explicit Color(const Vec4& v) noexcept : r(v.x), g(v.y), b(v.z), a(v.w) {}
        constexpr Color(const Vec3& rgb, float a_) noexcept : r(rgb.x), g(rgb.y), b(rgb.z), a(a_) {}

        //--------------------------------------------------------
        // Comparison
        //--------------------------------------------------------
        constexpr bool operator==(const Color& o) const noexcept { return r == o.r && g == o.g && b == o.b && a == o.a; }
        constexpr bool operator!=(const Color& o) const noexcept { return !(*this == o); }

        constexpr bool IsApproxEqual(const Color& o, float epsilon = EPSILON_POS) const noexcept
        {
            return IsEqual(r, o.r, epsilon) && IsEqual(g, o.g, epsilon) && IsEqual(b, o.b, epsilon) && IsEqual(a, o.a, epsilon);
        }

        //--------------------------------------------------------
        // Arithmetic (component-wise)
        //--------------------------------------------------------
        constexpr Color operator+(const Color& o) const noexcept { return Color(r + o.r, g + o.g, b + o.b, a + o.a); }
        constexpr Color operator-(const Color& o) const noexcept { return Color(r - o.r, g - o.g, b - o.b, a - o.a); }
        constexpr Color operator*(const Color& o) const noexcept { return Color(r * o.r, g * o.g, b * o.b, a * o.a); }
        constexpr Color operator*(float s) const noexcept { return Color(r * s, g * s, b * s, a * s); }

        //--------------------------------------------------------
        // Conversions
        //--------------------------------------------------------
        constexpr Vec4 ToVec4() const noexcept { return Vec4(r, g, b, a); }
        constexpr Vec3 ToVec3() const noexcept { return Vec3(r, g, b); } // RGB only (not named RGB: Windows.h macro)

        constexpr Color WithAlpha(float alpha) const noexcept { return Color(r, g, b, alpha); }

        constexpr Color Clamped() const noexcept
        {
            return Color(Clamp(r, 0.0f, 1.0f), Clamp(g, 0.0f, 1.0f), Clamp(b, 0.0f, 1.0f), Clamp(a, 0.0f, 1.0f));
        }

        // Linear -> sRGB (alpha unchanged)
        Color ToSRGB() const noexcept
        {
            return Color(LinearToGamma(r), LinearToGamma(g), LinearToGamma(b), a);
        }

        // sRGB -> linear (alpha unchanged)
        Color ToLinear() const noexcept
        {
            return Color(GammaToLinear(r), GammaToLinear(g), GammaToLinear(b), a);
        }

        // Relative luminance of a linear color (Rec. 709)
        constexpr float Luminance() const noexcept
        {
            return 0.2126f * r + 0.7152f * g + 0.0722f * b;
        }

        // Multiplies RGB by alpha (for premultiplied blending)
        constexpr Color Premultiplied() const noexcept
        {
            return Color(r * a, g * a, b * a, a);
        }

        // HSV with all components in [0, 1] (hue wraps)
        static Color FromHSV(float h, float s, float v, float alpha = 1.0f) noexcept
        {
            h = h - floorf(h);
            float h6 = h * 6.0f;
            int sector = static_cast<int>(h6) % 6;
            float f = h6 - floorf(h6);
            float p = v * (1.0f - s);
            float q = v * (1.0f - s * f);
            float t = v * (1.0f - s * (1.0f - f));
            switch (sector)
            {
            case 0:  return Color(v, t, p, alpha);
            case 1:  return Color(q, v, p, alpha);
            case 2:  return Color(p, v, t, alpha);
            case 3:  return Color(p, q, v, alpha);
            case 4:  return Color(t, p, v, alpha);
            default: return Color(v, p, q, alpha);
            }
        }

        void ToHSV(float& h, float& s, float& v) const noexcept
        {
            float maxC = Max(r, Max(g, b));
            float minC = Min(r, Min(g, b));
            float delta = maxC - minC;

            v = maxC;
            s = (maxC > 0.0f) ? delta / maxC : 0.0f;

            if (delta <= 0.0f)
            {
                h = 0.0f;
                return;
            }

            if (maxC == r)
                h = (g - b) / delta + (g < b ? 6.0f : 0.0f);
            else if (maxC == g)
                h = (b - r) / delta + 2.0f;
            else
                h = (r - g) / delta + 4.0f;
            h /= 6.0f;
        }

        // Defined after Color32
        constexpr Color32 ToColor32() const noexcept;
        static constexpr Color FromColor32(const Color32& c) noexcept;

        //--------------------------------------------------------
        // Interpolation
        //--------------------------------------------------------
        static constexpr Color Lerp(const Color& a, const Color& b, float t) noexcept
        {
            return Color(
                xg::Lerp(a.r, b.r, t),
                xg::Lerp(a.g, b.g, t),
                xg::Lerp(a.b, b.b, t),
                xg::Lerp(a.a, b.a, t));
        }

        //--------------------------------------------------------
        // Named colors
        //--------------------------------------------------------
        static constexpr Color White() noexcept { return Color(1.0f, 1.0f, 1.0f, 1.0f); }
        static constexpr Color Black() noexcept { return Color(0.0f, 0.0f, 0.0f, 1.0f); }
        static constexpr Color Clear() noexcept { return Color(0.0f, 0.0f, 0.0f, 0.0f); }
        static constexpr Color Gray() noexcept { return Color(0.5f, 0.5f, 0.5f, 1.0f); }
        static constexpr Color Red() noexcept { return Color(1.0f, 0.0f, 0.0f, 1.0f); }
        static constexpr Color Green() noexcept { return Color(0.0f, 1.0f, 0.0f, 1.0f); }
        static constexpr Color Blue() noexcept { return Color(0.0f, 0.0f, 1.0f, 1.0f); }
        static constexpr Color Yellow() noexcept { return Color(1.0f, 1.0f, 0.0f, 1.0f); }
        static constexpr Color Cyan() noexcept { return Color(0.0f, 1.0f, 1.0f, 1.0f); }
        static constexpr Color Magenta() noexcept { return Color(1.0f, 0.0f, 1.0f, 1.0f); }
    };

    static_assert(std::is_standard_layout_v<Color>, "Color must be standard layout");
    static_assert(std::is_trivially_copyable_v<Color>, "Color must be trivially copyable");
    static_assert(sizeof(Color) == 16, "Color must be 4 floats");

    //============================================================
    // Color32 - 8-bit RGBA (memory order r, g, b, a)
    //============================================================
    struct Color32
    {
        uint8_t r = 0;
        uint8_t g = 0;
        uint8_t b = 0;
        uint8_t a = 255;

        constexpr Color32() noexcept = default;
        constexpr Color32(uint8_t r_, uint8_t g_, uint8_t b_, uint8_t a_ = 255) noexcept : r(r_), g(g_), b(b_), a(a_) {}

        constexpr bool operator==(const Color32& o) const noexcept { return r == o.r && g == o.g && b == o.b && a == o.a; }
        constexpr bool operator!=(const Color32& o) const noexcept { return !(*this == o); }

        // 0xRRGGBBAA
        constexpr uint32_t PackRGBA() const noexcept
        {
            return (static_cast<uint32_t>(r) << 24) | (static_cast<uint32_t>(g) << 16) | (static_cast<uint32_t>(b) << 8) | a;
        }

        // 0xAABBGGRR (matches DXGI_FORMAT_R8G8B8A8 read as a little-endian uint32)
        constexpr uint32_t PackABGR() const noexcept
        {
            return (static_cast<uint32_t>(a) << 24) | (static_cast<uint32_t>(b) << 16) | (static_cast<uint32_t>(g) << 8) | r;
        }

        static constexpr Color32 UnpackRGBA(uint32_t v) noexcept
        {
            return Color32(static_cast<uint8_t>(v >> 24), static_cast<uint8_t>(v >> 16), static_cast<uint8_t>(v >> 8), static_cast<uint8_t>(v));
        }

        static constexpr Color32 UnpackABGR(uint32_t v) noexcept
        {
            return Color32(static_cast<uint8_t>(v), static_cast<uint8_t>(v >> 8), static_cast<uint8_t>(v >> 16), static_cast<uint8_t>(v >> 24));
        }

        static constexpr Color32 Lerp(const Color32& a, const Color32& b, float t) noexcept
        {
            return Color::Lerp(Color::FromColor32(a), Color::FromColor32(b), t).ToColor32();
        }
    };

    static_assert(std::is_standard_layout_v<Color32>, "Color32 must be standard layout");
    static_assert(std::is_trivially_copyable_v<Color32>, "Color32 must be trivially copyable");
    static_assert(sizeof(Color32) == 4, "Color32 must be 4 bytes");

    //============================================================
    // Color <-> Color32 (no gamma conversion)
    //============================================================
    constexpr Color32 Color::ToColor32() const noexcept
    {
        Color c = Clamped();
        return Color32(
            static_cast<uint8_t>(c.r * 255.0f + 0.5f),
            static_cast<uint8_t>(c.g * 255.0f + 0.5f),
            static_cast<uint8_t>(c.b * 255.0f + 0.5f),
            static_cast<uint8_t>(c.a * 255.0f + 0.5f));
    }

    constexpr Color Color::FromColor32(const Color32& c) noexcept
    {
        constexpr float inv = 1.0f / 255.0f;
        return Color(c.r * inv, c.g * inv, c.b * inv, c.a * inv);
    }

    constexpr Color operator*(float s, const Color& c) noexcept { return c * s; }

    constexpr Color Lerp(const Color& a, const Color& b, float t) noexcept { return Color::Lerp(a, b, t); }
} // namespace xg
