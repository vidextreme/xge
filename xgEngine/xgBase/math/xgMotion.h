// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.

#pragma once

// xgMotion.h - Gameplay motion helpers
// Remapping, frame-rate independent damping, SmoothDamp, springs,
// angle wrapping and rotate-towards. All angles are in radians.

#include "xgMathCommon.h"
#include "xgVec2.h"
#include "xgVec3.h"
#include "xgQuat.h"

namespace xg
{
    //============================================================
    // Remapping
    //============================================================

    // Inverse of Lerp: returns t such that Lerp(a, b, t) == value
    constexpr float InverseLerp(float a, float b, float value) noexcept
    {
        return (a != b) ? (value - a) / (b - a) : 0.0f;
    }

    // Maps value from [inMin, inMax] to [outMin, outMax] (unclamped)
    constexpr float Remap(float value, float inMin, float inMax, float outMin, float outMax) noexcept
    {
        return Lerp(outMin, outMax, InverseLerp(inMin, inMax, value));
    }

    constexpr float Saturate(float x) noexcept
    {
        return Clamp(x, 0.0f, 1.0f);
    }

    constexpr float RemapClamped(float value, float inMin, float inMax, float outMin, float outMax) noexcept
    {
        return Lerp(outMin, outMax, Saturate(InverseLerp(inMin, inMax, value)));
    }

    // Moves current towards target by at most maxDelta
    constexpr float MoveTowards(float current, float target, float maxDelta) noexcept
    {
        float delta = target - current;
        if (Abs(delta) <= maxDelta)
            return target;
        return current + Sign(delta) * maxDelta;
    }

    //============================================================
    // Angles (radians)
    //============================================================

    // Wraps an angle to the range [-PI, PI)
    inline float WrapAngle(float radians) noexcept
    {
        return radians - TWO_PI * floorf((radians + PI) * INV_TWO_PI);
    }

    // Shortest signed difference from current to target, in [-PI, PI)
    inline float DeltaAngle(float current, float target) noexcept
    {
        return WrapAngle(target - current);
    }

    // Interpolates along the shortest arc
    inline float LerpAngle(float a, float b, float t) noexcept
    {
        return a + DeltaAngle(a, b) * t;
    }

    // Moves an angle towards target along the shortest arc by at most maxDelta
    inline float MoveTowardsAngle(float current, float target, float maxDelta) noexcept
    {
        float delta = DeltaAngle(current, target);
        if (Abs(delta) <= maxDelta)
            return current + delta;
        return current + Sign(delta) * maxDelta;
    }

    //============================================================
    // Frame-rate independent damping
    //
    // Damp(a, b, lambda, dt) = Lerp(a, b, 1 - exp(-lambda * dt))
    // Gives the same curve at any frame rate, unlike Lerp(a, b, k) per frame.
    // lambda is the decay rate (higher = faster).
    //============================================================

    inline float DampFactor(float lambda, float dt) noexcept
    {
        return 1.0f - expf(-lambda * dt);
    }

    inline float Damp(float a, float b, float lambda, float dt) noexcept
    {
        return Lerp(a, b, DampFactor(lambda, dt));
    }

    inline Vec2 Damp(const Vec2& a, const Vec2& b, float lambda, float dt) noexcept
    {
        return Vec2::Lerp(a, b, DampFactor(lambda, dt));
    }

    inline Vec3 Damp(const Vec3& a, const Vec3& b, float lambda, float dt) noexcept
    {
        return Vec3::Lerp(a, b, DampFactor(lambda, dt));
    }

    inline Quat Damp(const Quat& a, const Quat& b, float lambda, float dt) noexcept
    {
        return Quat::Slerp(a, b, DampFactor(lambda, dt));
    }

    inline float DampAngle(float a, float b, float lambda, float dt) noexcept
    {
        return LerpAngle(a, b, DampFactor(lambda, dt));
    }

    //============================================================
    // SmoothDamp
    //
    // Critically damped smoothing towards a moving target that never
    // overshoots. velocity is in/out state owned by the caller.
    // smoothTime is roughly the time to reach the target.
    //============================================================

    namespace detail
    {
        // Pade approximation of exp(-omega * dt) used by SmoothDamp
        inline float SmoothDampDecay(float omega, float dt) noexcept
        {
            float x = omega * dt;
            return 1.0f / (1.0f + x + 0.48f * x * x + 0.235f * x * x * x);
        }
    }

    inline float SmoothDamp(float current, float target, float& velocity,
        float smoothTime, float dt, float maxSpeed = INFINITY_F) noexcept
    {
        smoothTime = Max(0.0001f, smoothTime);
        float omega = 2.0f / smoothTime;
        float decay = detail::SmoothDampDecay(omega, dt);

        float originalTarget = target;
        float maxChange = maxSpeed * smoothTime;
        float change = Clamp(current - target, -maxChange, maxChange);
        target = current - change;

        float temp = (velocity + omega * change) * dt;
        velocity = (velocity - omega * temp) * decay;
        float output = target + (change + temp) * decay;

        // Prevent overshoot
        if ((originalTarget - current > 0.0f) == (output > originalTarget))
        {
            output = originalTarget;
            velocity = (dt > 0.0f) ? (output - originalTarget) / dt : 0.0f;
        }
        return output;
    }

    inline float SmoothDampAngle(float current, float target, float& velocity,
        float smoothTime, float dt, float maxSpeed = INFINITY_F) noexcept
    {
        target = current + DeltaAngle(current, target);
        return SmoothDamp(current, target, velocity, smoothTime, dt, maxSpeed);
    }

    inline Vec2 SmoothDamp(const Vec2& current, const Vec2& targetIn, Vec2& velocity,
        float smoothTime, float dt, float maxSpeed = INFINITY_F) noexcept
    {
        smoothTime = Max(0.0001f, smoothTime);
        float omega = 2.0f / smoothTime;
        float decay = detail::SmoothDampDecay(omega, dt);

        Vec2 change = current - targetIn;
        float maxChange = maxSpeed * smoothTime;
        float changeLenSq = change.LengthSq();
        if (changeLenSq > maxChange * maxChange)
            change = change * (maxChange / sqrtf(changeLenSq));
        Vec2 target = current - change;

        Vec2 temp = (velocity + change * omega) * dt;
        velocity = (velocity - temp * omega) * decay;
        Vec2 output = target + (change + temp) * decay;

        // Prevent overshoot
        if ((targetIn - current).Dot(output - targetIn) > 0.0f)
        {
            output = targetIn;
            velocity = Vec2::Zero();
        }
        return output;
    }

    inline Vec3 SmoothDamp(const Vec3& current, const Vec3& targetIn, Vec3& velocity,
        float smoothTime, float dt, float maxSpeed = INFINITY_F) noexcept
    {
        smoothTime = Max(0.0001f, smoothTime);
        float omega = 2.0f / smoothTime;
        float decay = detail::SmoothDampDecay(omega, dt);

        Vec3 change = current - targetIn;
        float maxChange = maxSpeed * smoothTime;
        float changeLenSq = change.LengthSq();
        if (changeLenSq > maxChange * maxChange)
            change = change * (maxChange / sqrtf(changeLenSq));
        Vec3 target = current - change;

        Vec3 temp = (velocity + change * omega) * dt;
        velocity = (velocity - temp * omega) * decay;
        Vec3 output = target + (change + temp) * decay;

        // Prevent overshoot
        if ((targetIn - current).Dot(output - targetIn) > 0.0f)
        {
            output = targetIn;
            velocity = Vec3::Zero();
        }
        return output;
    }

    //============================================================
    // Springs
    //
    // UpdateCritical: critically damped spring parameterized by half-life
    //   (time to cover half the remaining distance). Exact, never overshoots.
    // Update: damped spring with frequency (Hz) and damping ratio
    //   (1 = critical, < 1 = bouncy, > 1 = sluggish). Implicit Euler step,
    //   stable for any dt.
    //============================================================

    namespace detail
    {
        constexpr float LN2 = 0.69314718056f;

        inline float HalfLifeToDamping(float halfLife) noexcept
        {
            return (4.0f * LN2) / (halfLife + 1e-5f);
        }
    }

    struct Spring
    {
        float Value = 0.0f;
        float Velocity = 0.0f;

        void UpdateCritical(float target, float halfLife, float dt) noexcept
        {
            float y = detail::HalfLifeToDamping(halfLife) * 0.5f;
            float j0 = Value - target;
            float j1 = Velocity + j0 * y;
            float eydt = expf(-y * dt);
            Value = eydt * (j0 + j1 * dt) + target;
            Velocity = eydt * (Velocity - j1 * y * dt);
        }

        void Update(float target, float frequency, float dampingRatio, float dt) noexcept
        {
            float omega = TWO_PI * frequency;
            float f = 1.0f + 2.0f * dt * dampingRatio * omega;
            float hoo = dt * omega * omega;
            float hhoo = dt * hoo;
            float detInv = 1.0f / (f + hhoo);
            float detX = f * Value + dt * Velocity + hhoo * target;
            float detV = Velocity + hoo * (target - Value);
            Value = detX * detInv;
            Velocity = detV * detInv;
        }
    };

    struct SpringVec3
    {
        Vec3 Value;
        Vec3 Velocity;

        void UpdateCritical(const Vec3& target, float halfLife, float dt) noexcept
        {
            float y = detail::HalfLifeToDamping(halfLife) * 0.5f;
            Vec3 j0 = Value - target;
            Vec3 j1 = Velocity + j0 * y;
            float eydt = expf(-y * dt);
            Value = (j0 + j1 * dt) * eydt + target;
            Velocity = (Velocity - j1 * (y * dt)) * eydt;
        }

        void Update(const Vec3& target, float frequency, float dampingRatio, float dt) noexcept
        {
            float omega = TWO_PI * frequency;
            float f = 1.0f + 2.0f * dt * dampingRatio * omega;
            float hoo = dt * omega * omega;
            float hhoo = dt * hoo;
            float detInv = 1.0f / (f + hhoo);
            Vec3 detX = Value * f + Velocity * dt + target * hhoo;
            Vec3 detV = Velocity + (target - Value) * hoo;
            Value = detX * detInv;
            Velocity = detV * detInv;
        }
    };

    //============================================================
    // RotateTowards
    //============================================================

    // Rotates from towards to by at most maxRadians
    inline Quat RotateTowards(const Quat& from, const Quat& to, float maxRadians) noexcept
    {
        float angle = from.Angle(to);
        if (angle <= maxRadians || angle < EPSILON_POS)
            return to;
        return Quat::Slerp(from, to, maxRadians / angle);
    }

    // Rotates the direction of current towards target by at most maxRadians,
    // and moves its length towards target's length by at most maxMagnitudeDelta
    inline Vec3 RotateTowards(const Vec3& current, const Vec3& target,
        float maxRadians, float maxMagnitudeDelta) noexcept
    {
        float currentLen = current.Length();
        float targetLen = target.Length();

        if (currentLen < EPSILON_POS || targetLen < EPSILON_POS)
        {
            // No meaningful direction: just move linearly
            Vec3 delta = target - current;
            float dist = delta.Length();
            if (dist <= maxMagnitudeDelta || dist < EPSILON_POS)
                return target;
            return current + delta * (maxMagnitudeDelta / dist);
        }

        Vec3 from = current / currentLen;
        Vec3 to = target / targetLen;
        float cosAngle = Clamp(from.Dot(to), -1.0f, 1.0f);
        float angle = acosf(cosAngle);

        Vec3 dir;
        if (angle <= maxRadians)
        {
            dir = to;
        }
        else
        {
            Vec3 axis = from.Cross(to);
            if (axis.LengthSq() < EPSILON_POS)
            {
                // Opposite directions: rotate around any perpendicular axis
                axis = (Abs(from.x) < 0.9f) ? from.Cross(Vec3::UnitX()) : from.Cross(Vec3::UnitY());
            }
            dir = Quat::FromAxisAngle(axis.Normalized(), maxRadians).RotateVector(from);
        }

        float len = MoveTowards(currentLen, targetLen, maxMagnitudeDelta);
        return dir * len;
    }
} // namespace xg
