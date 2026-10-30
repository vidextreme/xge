// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.

using System.Runtime.InteropServices;

// Managed mirror of xgBase/math/xgRandom.h (pure C#).
// Pcg32 produces exactly the same sequence as the C++ xg::Pcg32 for the same seed/stream,
// so gameplay randomness can be shared between native and managed modules.

namespace xg.math
{
    [StructLayout(LayoutKind.Sequential)]
    public struct Pcg32
    {
        public const ulong Multiplier = 6364136223846793005UL;
        public const ulong DefaultStream = 0xda3e39cb94b95bdbUL;

        public ulong State;
        public ulong Increment;

        public Pcg32()
        {
            State = 0x853c49e6748fea9bUL;
            Increment = DefaultStream;
        }

        // Same seeding as the reference pcg32_srandom_r(seed, stream)
        public Pcg32(ulong seed, ulong stream = 54UL)
        {
            State = 0;
            Increment = 0;
            Seed(seed, stream);
        }

        public void Seed(ulong seed, ulong stream = 54UL)
        {
            State = 0UL;
            Increment = (stream << 1) | 1UL;
            NextUInt();
            State += seed;
            NextUInt();
        }

        // ------------------------------------------------------------
        // Integers
        // ------------------------------------------------------------
        public uint NextUInt()
        {
            unchecked
            {
                ulong old = State;
                State = old * Multiplier + Increment;
                uint xorShifted = (uint)(((old >> 18) ^ old) >> 27);
                int rot = (int)(old >> 59);
                return (xorShifted >> rot) | (xorShifted << ((-rot) & 31));
            }
        }

        // Unbiased value in [0, bound)
        public uint NextUInt(uint bound)
        {
            unchecked
            {
                if (bound == 0u)
                    return 0u;
                ulong m = (ulong)NextUInt() * bound;
                uint low = (uint)m;
                if (low < bound)
                {
                    uint threshold = (0u - bound) % bound;
                    while (low < threshold)
                    {
                        m = (ulong)NextUInt() * bound;
                        low = (uint)m;
                    }
                }
                return (uint)(m >> 32);
            }
        }

        // Integer in [min, maxExclusive)
        public int Range(int min, int maxExclusive)
        {
            if (maxExclusive <= min)
                return min;
            return min + (int)NextUInt((uint)(maxExclusive - min));
        }

        // ------------------------------------------------------------
        // Floats
        // ------------------------------------------------------------
        public float NextFloat() => (NextUInt() >> 8) * (1.0f / 16777216.0f);

        public float Range(float min, float max) => min + NextFloat() * (max - min);

        public bool Chance(float probability) => NextFloat() < probability;

        public float Sign() => (NextUInt() & 1u) != 0 ? 1.0f : -1.0f;

        public float Gaussian(float mean = 0.0f, float stdDev = 1.0f)
        {
            float u, v, s;
            do
            {
                u = NextFloat() * 2.0f - 1.0f;
                v = NextFloat() * 2.0f - 1.0f;
                s = u * u + v * v;
            } while (s >= 1.0f || s == 0.0f);
            return mean + stdDev * u * MathF.Sqrt(-2.0f * MathF.Log(s) / s);
        }

        // ------------------------------------------------------------
        // Geometric samples
        // ------------------------------------------------------------
        public Vector2 InsideUnitCircle()
        {
            Vector2 p;
            do
            {
                p = new Vector2(NextFloat() * 2.0f - 1.0f, NextFloat() * 2.0f - 1.0f);
            } while (p.x * p.x + p.y * p.y > 1.0f);
            return p;
        }

        public Vector2 OnUnitCircle()
        {
            float a = NextFloat() * VecMath.TwoPi;
            return new Vector2(MathF.Cos(a), MathF.Sin(a));
        }

        public Vector3 InsideUnitSphere()
        {
            Vector3 p;
            do
            {
                p = new Vector3(NextFloat() * 2.0f - 1.0f, NextFloat() * 2.0f - 1.0f, NextFloat() * 2.0f - 1.0f);
            } while (VecMath.LengthSq(p) > 1.0f);
            return p;
        }

        public Vector3 OnUnitSphere()
        {
            float x1, x2, w;
            do
            {
                x1 = NextFloat() * 2.0f - 1.0f;
                x2 = NextFloat() * 2.0f - 1.0f;
                w = x1 * x1 + x2 * x2;
            } while (w >= 1.0f);
            float s = 2.0f * MathF.Sqrt(1.0f - w);
            return new Vector3(x1 * s, x2 * s, 1.0f - 2.0f * w);
        }

        public Vector3 InCone(Vector3 direction, float halfAngle)
        {
            Vector3 axis = VecMath.Normalize(direction);
            float cosHalf = MathF.Cos(halfAngle);
            float cosTheta = cosHalf + (1.0f - cosHalf) * NextFloat();
            float sinTheta = MathF.Sqrt(MathF.Max(0.0f, 1.0f - cosTheta * cosTheta));
            float phi = NextFloat() * VecMath.TwoPi;

            Vector3 helper = MathF.Abs(axis.x) < 0.9f ? new Vector3(1f, 0f, 0f) : new Vector3(0f, 1f, 0f);
            Vector3 tangent = VecMath.Normalize(VecMath.Cross(helper, axis));
            Vector3 bitangent = VecMath.Cross(axis, tangent);

            return VecMath.Add(VecMath.Add(
                VecMath.Scale(tangent, MathF.Cos(phi) * sinTheta),
                VecMath.Scale(bitangent, MathF.Sin(phi) * sinTheta)),
                VecMath.Scale(axis, cosTheta));
        }

        public Vector3 InTriangle(Vector3 a, Vector3 b, Vector3 c)
        {
            float r1 = NextFloat();
            float r2 = NextFloat();
            if (r1 + r2 > 1.0f)
            {
                r1 = 1.0f - r1;
                r2 = 1.0f - r2;
            }
            return VecMath.Add(VecMath.Add(a, VecMath.Scale(VecMath.Sub(b, a), r1)), VecMath.Scale(VecMath.Sub(c, a), r2));
        }

        // Uniform random rotation (Shoemake)
        public Quaternion Rotation()
        {
            float u1 = NextFloat();
            float u2 = NextFloat() * VecMath.TwoPi;
            float u3 = NextFloat() * VecMath.TwoPi;
            float a = MathF.Sqrt(1.0f - u1);
            float b = MathF.Sqrt(u1);
            return new Quaternion(a * MathF.Sin(u2), a * MathF.Cos(u2), b * MathF.Sin(u3), b * MathF.Cos(u3));
        }

        // ------------------------------------------------------------
        // Collections
        // ------------------------------------------------------------
        public void Shuffle<T>(Span<T> items)
        {
            for (int i = items.Length - 1; i > 0; --i)
            {
                int j = (int)NextUInt((uint)(i + 1));
                (items[i], items[j]) = (items[j], items[i]);
            }
        }

        // Index chosen proportionally to weights (negative = 0); -1 if all weights are zero
        public int WeightedIndex(ReadOnlySpan<float> weights)
        {
            float total = 0.0f;
            foreach (float w in weights)
                total += MathF.Max(0.0f, w);
            if (total <= 0.0f)
                return -1;

            float r = NextFloat() * total;
            int last = -1;
            for (int i = 0; i < weights.Length; ++i)
            {
                float w = MathF.Max(0.0f, weights[i]);
                if (w <= 0.0f)
                    continue;
                last = i;
                if (r < w)
                    return i;
                r -= w;
            }
            return last;
        }
    }

    // ------------------------------------------------------------
    // Stateless hashing (mirror of xg::Hash32 / xg::Hash / xg::HashToFloat)
    // ------------------------------------------------------------
    public static class Hash
    {
        public static uint Hash32(uint x)
        {
            unchecked
            {
                uint state = x * 747796405u + 2891336453u;
                uint word = ((state >> (int)((state >> 28) + 4u)) ^ state) * 277803737u;
                return (word >> 22) ^ word;
            }
        }

        public static uint Of(int x, uint seed = 0u) => Hash32(unchecked((uint)x) ^ Hash32(seed));
        public static uint Of(int x, int y, uint seed = 0u) => Hash32(unchecked((uint)y) ^ Of(x, seed));
        public static uint Of(int x, int y, int z, uint seed = 0u) => Hash32(unchecked((uint)z) ^ Of(x, y, seed));

        public static float ToFloat(uint hash) => (hash >> 8) * (1.0f / 16777216.0f);
    }
}
