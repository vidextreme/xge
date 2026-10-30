// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.

using System.Runtime.InteropServices;

// Managed mirror of xgBase/math/xgColor.h (pure C#, same layouts as xg::Color / xg::Color32).
// Color stores linear values; Color32 is usually sRGB. Conversions never apply gamma implicitly.

namespace xg.math
{
    [StructLayout(LayoutKind.Sequential, Size = 16)]
    public struct Color : IEquatable<Color>
    {
        public float r;
        public float g;
        public float b;
        public float a;

        public Color(float r, float g, float b, float a = 1.0f)
        {
            this.r = r;
            this.g = g;
            this.b = b;
            this.a = a;
        }

        public override string ToString() => $"Color({r:F4}, {g:F4}, {b:F4}, {a:F4})";

        // ------------------------------------------------------------
        // Operators (component-wise)
        // ------------------------------------------------------------
        public static Color operator +(Color x, Color y) => new(x.r + y.r, x.g + y.g, x.b + y.b, x.a + y.a);
        public static Color operator -(Color x, Color y) => new(x.r - y.r, x.g - y.g, x.b - y.b, x.a - y.a);
        public static Color operator *(Color x, Color y) => new(x.r * y.r, x.g * y.g, x.b * y.b, x.a * y.a);
        public static Color operator *(Color c, float s) => new(c.r * s, c.g * s, c.b * s, c.a * s);
        public static Color operator *(float s, Color c) => c * s;
        public static bool operator ==(Color x, Color y) => x.r == y.r && x.g == y.g && x.b == y.b && x.a == y.a;
        public static bool operator !=(Color x, Color y) => !(x == y);

        public bool Equals(Color other) => this == other;
        public override bool Equals(object? obj) => obj is Color other && this == other;
        public override int GetHashCode() => HashCode.Combine(r, g, b, a);

        public bool IsApproxEqual(Color o, float epsilon = 1e-6f)
            => MathF.Abs(r - o.r) < epsilon && MathF.Abs(g - o.g) < epsilon && MathF.Abs(b - o.b) < epsilon && MathF.Abs(a - o.a) < epsilon;

        // ------------------------------------------------------------
        // Conversions
        // ------------------------------------------------------------
        public Vector3 ToVector3() => new(r, g, b);
        public Color WithAlpha(float alpha) => new(r, g, b, alpha);

        public Color Clamped() => new(Math.Clamp(r, 0f, 1f), Math.Clamp(g, 0f, 1f), Math.Clamp(b, 0f, 1f), Math.Clamp(a, 0f, 1f));

        public static float LinearToGamma(float linear)
            => linear <= 0.0031308f ? 12.92f * linear : 1.055f * MathF.Pow(linear, 1.0f / 2.4f) - 0.055f;

        public static float GammaToLinear(float gamma)
            => gamma <= 0.04045f ? gamma / 12.92f : MathF.Pow((gamma + 0.055f) / 1.055f, 2.4f);

        public Color ToSRGB() => new(LinearToGamma(r), LinearToGamma(g), LinearToGamma(b), a);
        public Color ToLinear() => new(GammaToLinear(r), GammaToLinear(g), GammaToLinear(b), a);

        // Rec. 709 relative luminance of a linear color
        public float Luminance() => 0.2126f * r + 0.7152f * g + 0.0722f * b;

        public Color Premultiplied() => new(r * a, g * a, b * a, a);

        // HSV with all components in [0, 1] (hue wraps)
        public static Color FromHSV(float h, float s, float v, float alpha = 1.0f)
        {
            h -= MathF.Floor(h);
            float h6 = h * 6.0f;
            int sector = (int)h6 % 6;
            float f = h6 - MathF.Floor(h6);
            float p = v * (1.0f - s);
            float q = v * (1.0f - s * f);
            float t = v * (1.0f - s * (1.0f - f));
            return sector switch
            {
                0 => new Color(v, t, p, alpha),
                1 => new Color(q, v, p, alpha),
                2 => new Color(p, v, t, alpha),
                3 => new Color(p, q, v, alpha),
                4 => new Color(t, p, v, alpha),
                _ => new Color(v, p, q, alpha),
            };
        }

        public void ToHSV(out float h, out float s, out float v)
        {
            float maxC = MathF.Max(r, MathF.Max(g, b));
            float minC = MathF.Min(r, MathF.Min(g, b));
            float delta = maxC - minC;

            v = maxC;
            s = maxC > 0.0f ? delta / maxC : 0.0f;

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

        public Color32 ToColor32()
        {
            Color c = Clamped();
            return new Color32(
                (byte)(c.r * 255.0f + 0.5f),
                (byte)(c.g * 255.0f + 0.5f),
                (byte)(c.b * 255.0f + 0.5f),
                (byte)(c.a * 255.0f + 0.5f));
        }

        public static Color FromColor32(Color32 c)
        {
            const float inv = 1.0f / 255.0f;
            return new Color(c.r * inv, c.g * inv, c.b * inv, c.a * inv);
        }

        public static Color Lerp(Color x, Color y, float t)
            => new(x.r + (y.r - x.r) * t, x.g + (y.g - x.g) * t, x.b + (y.b - x.b) * t, x.a + (y.a - x.a) * t);

        // ------------------------------------------------------------
        // Named colors
        // ------------------------------------------------------------
        public static Color White() => new(1f, 1f, 1f, 1f);
        public static Color Black() => new(0f, 0f, 0f, 1f);
        public static Color Clear() => new(0f, 0f, 0f, 0f);
        public static Color Gray() => new(0.5f, 0.5f, 0.5f, 1f);
        public static Color Red() => new(1f, 0f, 0f, 1f);
        public static Color Green() => new(0f, 1f, 0f, 1f);
        public static Color Blue() => new(0f, 0f, 1f, 1f);
        public static Color Yellow() => new(1f, 1f, 0f, 1f);
        public static Color Cyan() => new(0f, 1f, 1f, 1f);
        public static Color Magenta() => new(1f, 0f, 1f, 1f);
    }

    // 8-bit RGBA, memory order r, g, b, a
    [StructLayout(LayoutKind.Sequential)]
    public struct Color32 : IEquatable<Color32>
    {
        public byte r;
        public byte g;
        public byte b;
        public byte a;

        public Color32(byte r, byte g, byte b, byte a = 255)
        {
            this.r = r;
            this.g = g;
            this.b = b;
            this.a = a;
        }

        public override string ToString() => $"Color32({r}, {g}, {b}, {a})";

        public static bool operator ==(Color32 x, Color32 y) => x.r == y.r && x.g == y.g && x.b == y.b && x.a == y.a;
        public static bool operator !=(Color32 x, Color32 y) => !(x == y);
        public bool Equals(Color32 other) => this == other;
        public override bool Equals(object? obj) => obj is Color32 other && this == other;
        public override int GetHashCode() => (int)PackRGBA();

        // 0xRRGGBBAA
        public uint PackRGBA() => ((uint)r << 24) | ((uint)g << 16) | ((uint)b << 8) | a;

        // 0xAABBGGRR (DXGI_FORMAT_R8G8B8A8 as a little-endian uint)
        public uint PackABGR() => ((uint)a << 24) | ((uint)b << 16) | ((uint)g << 8) | r;

        public static Color32 UnpackRGBA(uint v) => new((byte)(v >> 24), (byte)(v >> 16), (byte)(v >> 8), (byte)v);
        public static Color32 UnpackABGR(uint v) => new((byte)v, (byte)(v >> 8), (byte)(v >> 16), (byte)(v >> 24));

        public static Color32 Lerp(Color32 x, Color32 y, float t)
            => Color.Lerp(Color.FromColor32(x), Color.FromColor32(y), t).ToColor32();

        public static implicit operator Color(Color32 c) => Color.FromColor32(c);
    }
}
