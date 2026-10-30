// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.

// Managed mirror of xgBase/math/xgMotion.h.
// Pure C# (no native calls): these run every frame and are too small to pay a P/Invoke.
// Formulas match the C++ versions; xgMathVerify.Managed checks them against the native golden values.

namespace xg.math
{
    // ------------------------------------------------------------
    // Managed component math used by the managed mirrors
    // (the Vector2/Vector3 operators in Vector*Interop.cs call into native code)
    // ------------------------------------------------------------
    internal static class VecMath
    {
        public const float Pi = 3.14159265358979323846f;
        public const float TwoPi = 2.0f * Pi;
        public const float HalfPi = Pi * 0.5f;
        public const float InvTwoPi = 1.0f / TwoPi;
        public const float EpsilonPos = 1e-6f;

        public static Vector2 Add(Vector2 a, Vector2 b) => new(a.x + b.x, a.y + b.y);
        public static Vector2 Sub(Vector2 a, Vector2 b) => new(a.x - b.x, a.y - b.y);
        public static Vector2 Scale(Vector2 v, float s) => new(v.x * s, v.y * s);
        public static float Dot(Vector2 a, Vector2 b) => a.x * b.x + a.y * b.y;
        public static float Cross(Vector2 a, Vector2 b) => a.x * b.y - a.y * b.x;
        public static Vector2 Lerp(Vector2 a, Vector2 b, float t) => new(a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t);

        public static Vector3 Add(Vector3 a, Vector3 b) => new(a.x + b.x, a.y + b.y, a.z + b.z);
        public static Vector3 Sub(Vector3 a, Vector3 b) => new(a.x - b.x, a.y - b.y, a.z - b.z);
        public static Vector3 Scale(Vector3 v, float s) => new(v.x * s, v.y * s, v.z * s);
        public static float Dot(Vector3 a, Vector3 b) => a.x * b.x + a.y * b.y + a.z * b.z;
        public static Vector3 Cross(Vector3 a, Vector3 b) => new(a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x);
        public static float LengthSq(Vector3 v) => v.x * v.x + v.y * v.y + v.z * v.z;
        public static float Length(Vector3 v) => MathF.Sqrt(LengthSq(v));
        public static Vector3 Lerp(Vector3 a, Vector3 b, float t) => new(a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t);

        public static Vector3 Normalize(Vector3 v)
        {
            float len = Length(v);
            return len > EpsilonPos ? Scale(v, 1.0f / len) : new Vector3(0f, 0f, 0f);
        }

        public static float QuatDot(Quaternion a, Quaternion b) => a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;

        public static Quaternion QuatNormalize(Quaternion q)
        {
            float len = MathF.Sqrt(QuatDot(q, q));
            if (len > EpsilonPos)
            {
                float inv = 1.0f / len;
                return new Quaternion(q.x * inv, q.y * inv, q.z * inv, q.w * inv);
            }
            return new Quaternion(0f, 0f, 0f, 1f);
        }

        // Same algorithm as Quat::Slerp in xgQuat.h
        public static Quaternion Slerp(Quaternion a, Quaternion b, float t)
        {
            float cosTheta = QuatDot(a, b);
            Quaternion end = b;
            if (cosTheta < 0.0f)
            {
                end = new Quaternion(-b.x, -b.y, -b.z, -b.w);
                cosTheta = -cosTheta;
            }

            Quaternion result;
            if (cosTheta > 1.0f - 1e-6f)
            {
                result = new Quaternion(
                    a.x + (end.x - a.x) * t,
                    a.y + (end.y - a.y) * t,
                    a.z + (end.z - a.z) * t,
                    a.w + (end.w - a.w) * t);
            }
            else
            {
                float theta = MathF.Acos(cosTheta);
                float invSinTheta = 1.0f / MathF.Sin(theta);
                float w1 = MathF.Sin((1.0f - t) * theta) * invSinTheta;
                float w2 = MathF.Sin(t * theta) * invSinTheta;
                result = new Quaternion(
                    a.x * w1 + end.x * w2,
                    a.y * w1 + end.y * w2,
                    a.z * w1 + end.z * w2,
                    a.w * w1 + end.w * w2);
            }
            return QuatNormalize(result);
        }

        // Same as Quat::Angle: 2 * acos(|dot|)
        public static float QuatAngle(Quaternion a, Quaternion b)
        {
            float dot = Math.Clamp(QuatDot(a, b), -1.0f, 1.0f);
            return 2.0f * MathF.Acos(MathF.Abs(dot));
        }

        // Same as Quat::RotateVector (q * v * q^-1), for unit quaternions
        public static Vector3 Rotate(Quaternion q, Vector3 v)
        {
            Vector3 u = new(q.x, q.y, q.z);
            Vector3 t = Scale(Cross(u, v), 2.0f);
            return Add(Add(v, Scale(t, q.w)), Cross(u, t));
        }

        public static Quaternion FromAxisAngle(Vector3 axis, float angle)
        {
            float half = angle * 0.5f;
            float s = MathF.Sin(half);
            Vector3 n = Normalize(axis);
            return new Quaternion(n.x * s, n.y * s, n.z * s, MathF.Cos(half));
        }
    }

    // ------------------------------------------------------------
    // Motion helpers (mirror of xgMotion.h). All angles in radians.
    // ------------------------------------------------------------
    public static class Motion
    {
        // Remapping
        public static float InverseLerp(float a, float b, float value) => a != b ? (value - a) / (b - a) : 0.0f;

        public static float Remap(float value, float inMin, float inMax, float outMin, float outMax)
            => outMin + (outMax - outMin) * InverseLerp(inMin, inMax, value);

        public static float Saturate(float x) => Math.Clamp(x, 0.0f, 1.0f);

        public static float RemapClamped(float value, float inMin, float inMax, float outMin, float outMax)
            => outMin + (outMax - outMin) * Saturate(InverseLerp(inMin, inMax, value));

        public static float MoveTowards(float current, float target, float maxDelta)
        {
            float delta = target - current;
            if (MathF.Abs(delta) <= maxDelta)
                return target;
            return current + MathF.Sign(delta) * maxDelta;
        }

        // Angles
        public static float WrapAngle(float radians)
            => radians - VecMath.TwoPi * MathF.Floor((radians + VecMath.Pi) * VecMath.InvTwoPi);

        public static float DeltaAngle(float current, float target) => WrapAngle(target - current);

        public static float LerpAngle(float a, float b, float t) => a + DeltaAngle(a, b) * t;

        public static float MoveTowardsAngle(float current, float target, float maxDelta)
        {
            float delta = DeltaAngle(current, target);
            if (MathF.Abs(delta) <= maxDelta)
                return current + delta;
            return current + MathF.Sign(delta) * maxDelta;
        }

        // Frame-rate independent damping: Lerp(a, b, 1 - exp(-lambda * dt))
        public static float DampFactor(float lambda, float dt) => 1.0f - MathF.Exp(-lambda * dt);

        public static float Damp(float a, float b, float lambda, float dt) => a + (b - a) * DampFactor(lambda, dt);
        public static Vector2 Damp(Vector2 a, Vector2 b, float lambda, float dt) => VecMath.Lerp(a, b, DampFactor(lambda, dt));
        public static Vector3 Damp(Vector3 a, Vector3 b, float lambda, float dt) => VecMath.Lerp(a, b, DampFactor(lambda, dt));
        public static Quaternion Damp(Quaternion a, Quaternion b, float lambda, float dt) => VecMath.Slerp(a, b, DampFactor(lambda, dt));
        public static float DampAngle(float a, float b, float lambda, float dt) => LerpAngle(a, b, DampFactor(lambda, dt));

        // SmoothDamp: critically damped, never overshoots. velocity is caller-owned state.
        private static float SmoothDampDecay(float omega, float dt)
        {
            float x = omega * dt;
            return 1.0f / (1.0f + x + 0.48f * x * x + 0.235f * x * x * x);
        }

        public static float SmoothDamp(float current, float target, ref float velocity,
            float smoothTime, float dt, float maxSpeed = float.MaxValue)
        {
            smoothTime = MathF.Max(0.0001f, smoothTime);
            float omega = 2.0f / smoothTime;
            float decay = SmoothDampDecay(omega, dt);

            float originalTarget = target;
            float maxChange = maxSpeed * smoothTime;
            float change = Math.Clamp(current - target, -maxChange, maxChange);
            target = current - change;

            float temp = (velocity + omega * change) * dt;
            velocity = (velocity - omega * temp) * decay;
            float output = target + (change + temp) * decay;

            if ((originalTarget - current > 0.0f) == (output > originalTarget))
            {
                output = originalTarget;
                velocity = dt > 0.0f ? (output - originalTarget) / dt : 0.0f;
            }
            return output;
        }

        public static float SmoothDampAngle(float current, float target, ref float velocity,
            float smoothTime, float dt, float maxSpeed = float.MaxValue)
        {
            target = current + DeltaAngle(current, target);
            return SmoothDamp(current, target, ref velocity, smoothTime, dt, maxSpeed);
        }

        public static Vector2 SmoothDamp(Vector2 current, Vector2 target, ref Vector2 velocity,
            float smoothTime, float dt, float maxSpeed = float.MaxValue)
        {
            smoothTime = MathF.Max(0.0001f, smoothTime);
            float omega = 2.0f / smoothTime;
            float decay = SmoothDampDecay(omega, dt);

            Vector2 change = VecMath.Sub(current, target);
            float maxChange = maxSpeed * smoothTime;
            float changeLenSq = VecMath.Dot(change, change);
            if (changeLenSq > maxChange * maxChange)
                change = VecMath.Scale(change, maxChange / MathF.Sqrt(changeLenSq));
            Vector2 clampedTarget = VecMath.Sub(current, change);

            Vector2 temp = VecMath.Scale(VecMath.Add(velocity, VecMath.Scale(change, omega)), dt);
            velocity = VecMath.Scale(VecMath.Sub(velocity, VecMath.Scale(temp, omega)), decay);
            Vector2 output = VecMath.Add(clampedTarget, VecMath.Scale(VecMath.Add(change, temp), decay));

            if (VecMath.Dot(VecMath.Sub(target, current), VecMath.Sub(output, target)) > 0.0f)
            {
                output = target;
                velocity = new Vector2(0f, 0f);
            }
            return output;
        }

        public static Vector3 SmoothDamp(Vector3 current, Vector3 target, ref Vector3 velocity,
            float smoothTime, float dt, float maxSpeed = float.MaxValue)
        {
            smoothTime = MathF.Max(0.0001f, smoothTime);
            float omega = 2.0f / smoothTime;
            float decay = SmoothDampDecay(omega, dt);

            Vector3 change = VecMath.Sub(current, target);
            float maxChange = maxSpeed * smoothTime;
            float changeLenSq = VecMath.LengthSq(change);
            if (changeLenSq > maxChange * maxChange)
                change = VecMath.Scale(change, maxChange / MathF.Sqrt(changeLenSq));
            Vector3 clampedTarget = VecMath.Sub(current, change);

            Vector3 temp = VecMath.Scale(VecMath.Add(velocity, VecMath.Scale(change, omega)), dt);
            velocity = VecMath.Scale(VecMath.Sub(velocity, VecMath.Scale(temp, omega)), decay);
            Vector3 output = VecMath.Add(clampedTarget, VecMath.Scale(VecMath.Add(change, temp), decay));

            if (VecMath.Dot(VecMath.Sub(target, current), VecMath.Sub(output, target)) > 0.0f)
            {
                output = target;
                velocity = new Vector3(0f, 0f, 0f);
            }
            return output;
        }

        // RotateTowards
        public static Quaternion RotateTowards(Quaternion from, Quaternion to, float maxRadians)
        {
            float angle = VecMath.QuatAngle(from, to);
            if (angle <= maxRadians || angle < VecMath.EpsilonPos)
                return to;
            return VecMath.Slerp(from, to, maxRadians / angle);
        }

        public static Vector3 RotateTowards(Vector3 current, Vector3 target, float maxRadians, float maxMagnitudeDelta)
        {
            float currentLen = VecMath.Length(current);
            float targetLen = VecMath.Length(target);

            if (currentLen < VecMath.EpsilonPos || targetLen < VecMath.EpsilonPos)
            {
                Vector3 delta = VecMath.Sub(target, current);
                float dist = VecMath.Length(delta);
                if (dist <= maxMagnitudeDelta || dist < VecMath.EpsilonPos)
                    return target;
                return VecMath.Add(current, VecMath.Scale(delta, maxMagnitudeDelta / dist));
            }

            Vector3 from = VecMath.Scale(current, 1.0f / currentLen);
            Vector3 to = VecMath.Scale(target, 1.0f / targetLen);
            float angle = MathF.Acos(Math.Clamp(VecMath.Dot(from, to), -1.0f, 1.0f));

            Vector3 dir;
            if (angle <= maxRadians)
            {
                dir = to;
            }
            else
            {
                Vector3 axis = VecMath.Cross(from, to);
                if (VecMath.LengthSq(axis) < VecMath.EpsilonPos)
                {
                    axis = MathF.Abs(from.x) < 0.9f
                        ? VecMath.Cross(from, new Vector3(1f, 0f, 0f))
                        : VecMath.Cross(from, new Vector3(0f, 1f, 0f));
                }
                dir = VecMath.Rotate(VecMath.FromAxisAngle(axis, maxRadians), from);
            }

            return VecMath.Scale(dir, MoveTowards(currentLen, targetLen, maxMagnitudeDelta));
        }
    }

    // ------------------------------------------------------------
    // Springs (mirror of xg::Spring / xg::SpringVec3)
    // ------------------------------------------------------------
    public struct Spring
    {
        public float Value;
        public float Velocity;

        private const float Ln2 = 0.69314718056f;
        internal static float HalfLifeToDamping(float halfLife) => (4.0f * Ln2) / (halfLife + 1e-5f);

        // Critically damped; halfLife = time to cover half the remaining distance
        public void UpdateCritical(float target, float halfLife, float dt)
        {
            float y = HalfLifeToDamping(halfLife) * 0.5f;
            float j0 = Value - target;
            float j1 = Velocity + j0 * y;
            float eydt = MathF.Exp(-y * dt);
            Value = eydt * (j0 + j1 * dt) + target;
            Velocity = eydt * (Velocity - j1 * y * dt);
        }

        // frequency in Hz; dampingRatio 1 = critical, < 1 bouncy, > 1 sluggish
        public void Update(float target, float frequency, float dampingRatio, float dt)
        {
            float omega = VecMath.TwoPi * frequency;
            float f = 1.0f + 2.0f * dt * dampingRatio * omega;
            float hoo = dt * omega * omega;
            float hhoo = dt * hoo;
            float detInv = 1.0f / (f + hhoo);
            float detX = f * Value + dt * Velocity + hhoo * target;
            float detV = Velocity + hoo * (target - Value);
            Value = detX * detInv;
            Velocity = detV * detInv;
        }
    }

    public struct SpringVector3
    {
        public Vector3 Value;
        public Vector3 Velocity;

        public void UpdateCritical(Vector3 target, float halfLife, float dt)
        {
            float y = Spring.HalfLifeToDamping(halfLife) * 0.5f;
            Vector3 j0 = VecMath.Sub(Value, target);
            Vector3 j1 = VecMath.Add(Velocity, VecMath.Scale(j0, y));
            float eydt = MathF.Exp(-y * dt);
            Value = VecMath.Add(VecMath.Scale(VecMath.Add(j0, VecMath.Scale(j1, dt)), eydt), target);
            Velocity = VecMath.Scale(VecMath.Sub(Velocity, VecMath.Scale(j1, y * dt)), eydt);
        }

        public void Update(Vector3 target, float frequency, float dampingRatio, float dt)
        {
            float omega = VecMath.TwoPi * frequency;
            float f = 1.0f + 2.0f * dt * dampingRatio * omega;
            float hoo = dt * omega * omega;
            float hhoo = dt * hoo;
            float detInv = 1.0f / (f + hhoo);
            Vector3 detX = VecMath.Add(VecMath.Add(VecMath.Scale(Value, f), VecMath.Scale(Velocity, dt)), VecMath.Scale(target, hhoo));
            Vector3 detV = VecMath.Add(Velocity, VecMath.Scale(VecMath.Sub(target, Value), hoo));
            Value = VecMath.Scale(detX, detInv);
            Velocity = VecMath.Scale(detV, detInv);
        }
    }
}
