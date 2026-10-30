// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.

#pragma once

// xgEasing.h - Easing curves for tweens, UI and animation
// All functions map t in [0, 1] to [0, 1] (Back/Elastic overshoot by design).

#include <cstdint>
#include "xgMathCommon.h"

// XG_ENUM lives in xgBase.h; keep the math headers free of that dependency
#ifndef XG_ENUM
#define XG_ENUM(...)
#endif

namespace xg
{
    // Reflected to C# by xgReflectGen (xgSharedKit.Managed/Generated/xgEasing.generated.cs)
    XG_ENUM(inherit = byte)
    enum class EaseType : uint8_t
    {
        Linear,
        InQuad, OutQuad, InOutQuad,
        InCubic, OutCubic, InOutCubic,
        InQuart, OutQuart, InOutQuart,
        InQuint, OutQuint, InOutQuint,
        InSine, OutSine, InOutSine,
        InExpo, OutExpo, InOutExpo,
        InCirc, OutCirc, InOutCirc,
        InBack, OutBack, InOutBack,
        InElastic, OutElastic, InOutElastic,
        InBounce, OutBounce, InOutBounce,
        Count
    };

    namespace ease
    {
        constexpr float BackC1 = 1.70158f;
        constexpr float BackC2 = BackC1 * 1.525f;
        constexpr float BackC3 = BackC1 + 1.0f;
        constexpr float ElasticC4 = TWO_PI / 3.0f;
        constexpr float ElasticC5 = TWO_PI / 4.5f;

        constexpr float Linear(float t) noexcept { return t; }

        // Quad
        constexpr float InQuad(float t) noexcept { return t * t; }
        constexpr float OutQuad(float t) noexcept { return 1.0f - (1.0f - t) * (1.0f - t); }
        constexpr float InOutQuad(float t) noexcept
        {
            return t < 0.5f ? 2.0f * t * t : 1.0f - (-2.0f * t + 2.0f) * (-2.0f * t + 2.0f) * 0.5f;
        }

        // Cubic
        constexpr float InCubic(float t) noexcept { return t * t * t; }
        constexpr float OutCubic(float t) noexcept { float u = 1.0f - t; return 1.0f - u * u * u; }
        constexpr float InOutCubic(float t) noexcept
        {
            if (t < 0.5f) return 4.0f * t * t * t;
            float u = -2.0f * t + 2.0f;
            return 1.0f - u * u * u * 0.5f;
        }

        // Quart
        constexpr float InQuart(float t) noexcept { return t * t * t * t; }
        constexpr float OutQuart(float t) noexcept { float u = 1.0f - t; return 1.0f - u * u * u * u; }
        constexpr float InOutQuart(float t) noexcept
        {
            if (t < 0.5f) return 8.0f * t * t * t * t;
            float u = -2.0f * t + 2.0f;
            return 1.0f - u * u * u * u * 0.5f;
        }

        // Quint
        constexpr float InQuint(float t) noexcept { return t * t * t * t * t; }
        constexpr float OutQuint(float t) noexcept { float u = 1.0f - t; return 1.0f - u * u * u * u * u; }
        constexpr float InOutQuint(float t) noexcept
        {
            if (t < 0.5f) return 16.0f * t * t * t * t * t;
            float u = -2.0f * t + 2.0f;
            return 1.0f - u * u * u * u * u * 0.5f;
        }

        // Sine
        inline float InSine(float t) noexcept { return 1.0f - cosf(t * HALF_PI); }
        inline float OutSine(float t) noexcept { return sinf(t * HALF_PI); }
        inline float InOutSine(float t) noexcept { return -(cosf(PI * t) - 1.0f) * 0.5f; }

        // Expo
        inline float InExpo(float t) noexcept { return t <= 0.0f ? 0.0f : powf(2.0f, 10.0f * t - 10.0f); }
        inline float OutExpo(float t) noexcept { return t >= 1.0f ? 1.0f : 1.0f - powf(2.0f, -10.0f * t); }
        inline float InOutExpo(float t) noexcept
        {
            if (t <= 0.0f) return 0.0f;
            if (t >= 1.0f) return 1.0f;
            return t < 0.5f
                ? powf(2.0f, 20.0f * t - 10.0f) * 0.5f
                : (2.0f - powf(2.0f, -20.0f * t + 10.0f)) * 0.5f;
        }

        // Circ
        inline float InCirc(float t) noexcept { return 1.0f - sqrtf(Max(0.0f, 1.0f - t * t)); }
        inline float OutCirc(float t) noexcept { float u = t - 1.0f; return sqrtf(Max(0.0f, 1.0f - u * u)); }
        inline float InOutCirc(float t) noexcept
        {
            if (t < 0.5f)
            {
                float u = 2.0f * t;
                return (1.0f - sqrtf(Max(0.0f, 1.0f - u * u))) * 0.5f;
            }
            float u = -2.0f * t + 2.0f;
            return (sqrtf(Max(0.0f, 1.0f - u * u)) + 1.0f) * 0.5f;
        }

        // Back (overshoots)
        constexpr float InBack(float t) noexcept { return BackC3 * t * t * t - BackC1 * t * t; }
        constexpr float OutBack(float t) noexcept
        {
            float u = t - 1.0f;
            return 1.0f + BackC3 * u * u * u + BackC1 * u * u;
        }
        constexpr float InOutBack(float t) noexcept
        {
            if (t < 0.5f)
            {
                float u = 2.0f * t;
                return (u * u * ((BackC2 + 1.0f) * u - BackC2)) * 0.5f;
            }
            float u = 2.0f * t - 2.0f;
            return (u * u * ((BackC2 + 1.0f) * u + BackC2) + 2.0f) * 0.5f;
        }

        // Elastic (overshoots)
        inline float InElastic(float t) noexcept
        {
            if (t <= 0.0f) return 0.0f;
            if (t >= 1.0f) return 1.0f;
            return -powf(2.0f, 10.0f * t - 10.0f) * sinf((t * 10.0f - 10.75f) * ElasticC4);
        }
        inline float OutElastic(float t) noexcept
        {
            if (t <= 0.0f) return 0.0f;
            if (t >= 1.0f) return 1.0f;
            return powf(2.0f, -10.0f * t) * sinf((t * 10.0f - 0.75f) * ElasticC4) + 1.0f;
        }
        inline float InOutElastic(float t) noexcept
        {
            if (t <= 0.0f) return 0.0f;
            if (t >= 1.0f) return 1.0f;
            return t < 0.5f
                ? -(powf(2.0f, 20.0f * t - 10.0f) * sinf((20.0f * t - 11.125f) * ElasticC5)) * 0.5f
                : (powf(2.0f, -20.0f * t + 10.0f) * sinf((20.0f * t - 11.125f) * ElasticC5)) * 0.5f + 1.0f;
        }

        // Bounce
        constexpr float OutBounce(float t) noexcept
        {
            constexpr float n1 = 7.5625f;
            constexpr float d1 = 2.75f;
            if (t < 1.0f / d1)
                return n1 * t * t;
            if (t < 2.0f / d1)
            {
                t -= 1.5f / d1;
                return n1 * t * t + 0.75f;
            }
            if (t < 2.5f / d1)
            {
                t -= 2.25f / d1;
                return n1 * t * t + 0.9375f;
            }
            t -= 2.625f / d1;
            return n1 * t * t + 0.984375f;
        }
        constexpr float InBounce(float t) noexcept { return 1.0f - OutBounce(1.0f - t); }
        constexpr float InOutBounce(float t) noexcept
        {
            return t < 0.5f
                ? (1.0f - OutBounce(1.0f - 2.0f * t)) * 0.5f
                : (1.0f + OutBounce(2.0f * t - 1.0f)) * 0.5f;
        }
    } // namespace ease

    //============================================================
    // Data-driven evaluation
    //============================================================

    // Evaluates the curve; t is clamped to [0, 1]
    inline float Ease(EaseType type, float t) noexcept
    {
        t = Clamp(t, 0.0f, 1.0f);
        switch (type)
        {
        case EaseType::Linear:       return ease::Linear(t);
        case EaseType::InQuad:       return ease::InQuad(t);
        case EaseType::OutQuad:      return ease::OutQuad(t);
        case EaseType::InOutQuad:    return ease::InOutQuad(t);
        case EaseType::InCubic:      return ease::InCubic(t);
        case EaseType::OutCubic:     return ease::OutCubic(t);
        case EaseType::InOutCubic:   return ease::InOutCubic(t);
        case EaseType::InQuart:      return ease::InQuart(t);
        case EaseType::OutQuart:     return ease::OutQuart(t);
        case EaseType::InOutQuart:   return ease::InOutQuart(t);
        case EaseType::InQuint:      return ease::InQuint(t);
        case EaseType::OutQuint:     return ease::OutQuint(t);
        case EaseType::InOutQuint:   return ease::InOutQuint(t);
        case EaseType::InSine:       return ease::InSine(t);
        case EaseType::OutSine:      return ease::OutSine(t);
        case EaseType::InOutSine:    return ease::InOutSine(t);
        case EaseType::InExpo:       return ease::InExpo(t);
        case EaseType::OutExpo:      return ease::OutExpo(t);
        case EaseType::InOutExpo:    return ease::InOutExpo(t);
        case EaseType::InCirc:       return ease::InCirc(t);
        case EaseType::OutCirc:      return ease::OutCirc(t);
        case EaseType::InOutCirc:    return ease::InOutCirc(t);
        case EaseType::InBack:       return ease::InBack(t);
        case EaseType::OutBack:      return ease::OutBack(t);
        case EaseType::InOutBack:    return ease::InOutBack(t);
        case EaseType::InElastic:    return ease::InElastic(t);
        case EaseType::OutElastic:   return ease::OutElastic(t);
        case EaseType::InOutElastic: return ease::InOutElastic(t);
        case EaseType::InBounce:     return ease::InBounce(t);
        case EaseType::OutBounce:    return ease::OutBounce(t);
        case EaseType::InOutBounce:  return ease::InOutBounce(t);
        default:                     return t;
        }
    }

    // Eased interpolation for any type with a Lerp(a, b, t) overload
    template<typename T>
    inline T Tween(const T& a, const T& b, float t, EaseType type) noexcept
    {
        return Lerp(a, b, Ease(type, t));
    }
} // namespace xg
