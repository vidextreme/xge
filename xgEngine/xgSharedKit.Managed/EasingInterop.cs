// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.

// Managed mirror of xgBase/math/xgEasing.h (pure C#).
// xg.EaseType is generated from the C++ enum by xgReflectGen (Generated/xgEasing.generated.cs).

namespace xg.math
{
    public static class Ease
    {
        private const float BackC1 = 1.70158f;
        private const float BackC2 = BackC1 * 1.525f;
        private const float BackC3 = BackC1 + 1.0f;
        private const float ElasticC4 = VecMath.TwoPi / 3.0f;
        private const float ElasticC5 = VecMath.TwoPi / 4.5f;

        public static float Linear(float t) => t;

        // Quad
        public static float InQuad(float t) => t * t;
        public static float OutQuad(float t) => 1.0f - (1.0f - t) * (1.0f - t);
        public static float InOutQuad(float t) => t < 0.5f ? 2.0f * t * t : 1.0f - (-2.0f * t + 2.0f) * (-2.0f * t + 2.0f) * 0.5f;

        // Cubic
        public static float InCubic(float t) => t * t * t;
        public static float OutCubic(float t) { float u = 1.0f - t; return 1.0f - u * u * u; }
        public static float InOutCubic(float t)
        {
            if (t < 0.5f) return 4.0f * t * t * t;
            float u = -2.0f * t + 2.0f;
            return 1.0f - u * u * u * 0.5f;
        }

        // Quart
        public static float InQuart(float t) => t * t * t * t;
        public static float OutQuart(float t) { float u = 1.0f - t; return 1.0f - u * u * u * u; }
        public static float InOutQuart(float t)
        {
            if (t < 0.5f) return 8.0f * t * t * t * t;
            float u = -2.0f * t + 2.0f;
            return 1.0f - u * u * u * u * 0.5f;
        }

        // Quint
        public static float InQuint(float t) => t * t * t * t * t;
        public static float OutQuint(float t) { float u = 1.0f - t; return 1.0f - u * u * u * u * u; }
        public static float InOutQuint(float t)
        {
            if (t < 0.5f) return 16.0f * t * t * t * t * t;
            float u = -2.0f * t + 2.0f;
            return 1.0f - u * u * u * u * u * 0.5f;
        }

        // Sine
        public static float InSine(float t) => 1.0f - MathF.Cos(t * VecMath.HalfPi);
        public static float OutSine(float t) => MathF.Sin(t * VecMath.HalfPi);
        public static float InOutSine(float t) => -(MathF.Cos(VecMath.Pi * t) - 1.0f) * 0.5f;

        // Expo
        public static float InExpo(float t) => t <= 0.0f ? 0.0f : MathF.Pow(2.0f, 10.0f * t - 10.0f);
        public static float OutExpo(float t) => t >= 1.0f ? 1.0f : 1.0f - MathF.Pow(2.0f, -10.0f * t);
        public static float InOutExpo(float t)
        {
            if (t <= 0.0f) return 0.0f;
            if (t >= 1.0f) return 1.0f;
            return t < 0.5f
                ? MathF.Pow(2.0f, 20.0f * t - 10.0f) * 0.5f
                : (2.0f - MathF.Pow(2.0f, -20.0f * t + 10.0f)) * 0.5f;
        }

        // Circ
        public static float InCirc(float t) => 1.0f - MathF.Sqrt(MathF.Max(0.0f, 1.0f - t * t));
        public static float OutCirc(float t) { float u = t - 1.0f; return MathF.Sqrt(MathF.Max(0.0f, 1.0f - u * u)); }
        public static float InOutCirc(float t)
        {
            if (t < 0.5f)
            {
                float u = 2.0f * t;
                return (1.0f - MathF.Sqrt(MathF.Max(0.0f, 1.0f - u * u))) * 0.5f;
            }
            float v = -2.0f * t + 2.0f;
            return (MathF.Sqrt(MathF.Max(0.0f, 1.0f - v * v)) + 1.0f) * 0.5f;
        }

        // Back (overshoots)
        public static float InBack(float t) => BackC3 * t * t * t - BackC1 * t * t;
        public static float OutBack(float t) { float u = t - 1.0f; return 1.0f + BackC3 * u * u * u + BackC1 * u * u; }
        public static float InOutBack(float t)
        {
            if (t < 0.5f)
            {
                float u = 2.0f * t;
                return (u * u * ((BackC2 + 1.0f) * u - BackC2)) * 0.5f;
            }
            float v = 2.0f * t - 2.0f;
            return (v * v * ((BackC2 + 1.0f) * v + BackC2) + 2.0f) * 0.5f;
        }

        // Elastic (overshoots)
        public static float InElastic(float t)
        {
            if (t <= 0.0f) return 0.0f;
            if (t >= 1.0f) return 1.0f;
            return -MathF.Pow(2.0f, 10.0f * t - 10.0f) * MathF.Sin((t * 10.0f - 10.75f) * ElasticC4);
        }
        public static float OutElastic(float t)
        {
            if (t <= 0.0f) return 0.0f;
            if (t >= 1.0f) return 1.0f;
            return MathF.Pow(2.0f, -10.0f * t) * MathF.Sin((t * 10.0f - 0.75f) * ElasticC4) + 1.0f;
        }
        public static float InOutElastic(float t)
        {
            if (t <= 0.0f) return 0.0f;
            if (t >= 1.0f) return 1.0f;
            return t < 0.5f
                ? -(MathF.Pow(2.0f, 20.0f * t - 10.0f) * MathF.Sin((20.0f * t - 11.125f) * ElasticC5)) * 0.5f
                : (MathF.Pow(2.0f, -20.0f * t + 10.0f) * MathF.Sin((20.0f * t - 11.125f) * ElasticC5)) * 0.5f + 1.0f;
        }

        // Bounce
        public static float OutBounce(float t)
        {
            const float n1 = 7.5625f;
            const float d1 = 2.75f;
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
        public static float InBounce(float t) => 1.0f - OutBounce(1.0f - t);
        public static float InOutBounce(float t)
            => t < 0.5f ? (1.0f - OutBounce(1.0f - 2.0f * t)) * 0.5f : (1.0f + OutBounce(2.0f * t - 1.0f)) * 0.5f;

        // Data-driven evaluation; t is clamped to [0, 1]
        public static float Evaluate(EaseType type, float t)
        {
            t = Math.Clamp(t, 0.0f, 1.0f);
            return type switch
            {
                EaseType.Linear => Linear(t),
                EaseType.InQuad => InQuad(t),
                EaseType.OutQuad => OutQuad(t),
                EaseType.InOutQuad => InOutQuad(t),
                EaseType.InCubic => InCubic(t),
                EaseType.OutCubic => OutCubic(t),
                EaseType.InOutCubic => InOutCubic(t),
                EaseType.InQuart => InQuart(t),
                EaseType.OutQuart => OutQuart(t),
                EaseType.InOutQuart => InOutQuart(t),
                EaseType.InQuint => InQuint(t),
                EaseType.OutQuint => OutQuint(t),
                EaseType.InOutQuint => InOutQuint(t),
                EaseType.InSine => InSine(t),
                EaseType.OutSine => OutSine(t),
                EaseType.InOutSine => InOutSine(t),
                EaseType.InExpo => InExpo(t),
                EaseType.OutExpo => OutExpo(t),
                EaseType.InOutExpo => InOutExpo(t),
                EaseType.InCirc => InCirc(t),
                EaseType.OutCirc => OutCirc(t),
                EaseType.InOutCirc => InOutCirc(t),
                EaseType.InBack => InBack(t),
                EaseType.OutBack => OutBack(t),
                EaseType.InOutBack => InOutBack(t),
                EaseType.InElastic => InElastic(t),
                EaseType.OutElastic => OutElastic(t),
                EaseType.InOutElastic => InOutElastic(t),
                EaseType.InBounce => InBounce(t),
                EaseType.OutBounce => OutBounce(t),
                EaseType.InOutBounce => InOutBounce(t),
                _ => t,
            };
        }

        // Eased interpolation
        public static float Tween(float a, float b, float t, EaseType type) => a + (b - a) * Evaluate(type, t);
        public static Vector2 Tween(Vector2 a, Vector2 b, float t, EaseType type) => VecMath.Lerp(a, b, Evaluate(type, t));
        public static Vector3 Tween(Vector3 a, Vector3 b, float t, EaseType type) => VecMath.Lerp(a, b, Evaluate(type, t));
        public static Color Tween(Color a, Color b, float t, EaseType type) => Color.Lerp(a, b, Evaluate(type, t));
    }
}
