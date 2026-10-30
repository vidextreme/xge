// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.

#pragma once

// xgShapes.h - Collision / query shapes
// Plane, Segment, Triangle, OBB, Capsule, Cylinder, Circle, SweepHit.
// Queries between shapes live in xgGeometry.h.
//
// Layout note: Vec3 is 16 bytes (alignas(16)), so a float after a Vec3 sits at
// offset 16 and structs are padded to 16. The C# mirrors in ShapesInterop.cs
// declare the same explicit sizes; keep the static_asserts below in sync.

#include "xgMathCommon.h"
#include "xgVec2.h"
#include "xgVec3.h"
#include "xgVec4.h"
#include "xgQuat.h"
#include "xgMat4x4.h"
#include "xgMat3x3.h"
#include "xgBounds.h"

namespace xg
{
    //============================================================
    // Plane - dot(normal, p) + distance = 0, normal is unit length
    //============================================================
    struct alignas(16) Plane
    {
        Vec3 normal = Vec3::UnitY();
        float distance = 0.0f;

        constexpr Plane() noexcept = default;

        // Normalizes (normal, distance) together
        Plane(const Vec3& n, float d) noexcept
        {
            float len = n.Length();
            float inv = (len > EPSILON_POS) ? 1.0f / len : 0.0f;
            normal = n * inv;
            distance = d * inv;
        }

        Plane(float nx, float ny, float nz, float d) noexcept : Plane(Vec3(nx, ny, nz), d) {}

        // Assumes normal is already unit length
        static constexpr Plane FromRaw(const Vec3& unitNormal, float d) noexcept
        {
            Plane p;
            p.normal = unitNormal;
            p.distance = d;
            return p;
        }

        static Plane FromPointNormal(const Vec3& point, const Vec3& n) noexcept
        {
            Vec3 unit = n.Normalized();
            return FromRaw(unit, -unit.Dot(point));
        }

        // Normal follows the right-hand rule: Cross(b - a, c - a)
        static Plane FromPoints(const Vec3& a, const Vec3& b, const Vec3& c) noexcept
        {
            return FromPointNormal(a, (b - a).Cross(c - a));
        }

        // Signed distance (positive on the normal side)
        constexpr float Distance(const Vec3& p) const noexcept { return normal.Dot(p) + distance; }

        // Closest point on the plane
        constexpr Vec3 Project(const Vec3& p) const noexcept { return p - normal * Distance(p); }

        constexpr Plane Flipped() const noexcept { return FromRaw(-normal, -distance); }

        constexpr Vec4 ToVec4() const noexcept { return Vec4(normal.x, normal.y, normal.z, distance); }
    };

    //============================================================
    // Segment - line segment a -> b
    //============================================================
    struct alignas(16) Segment
    {
        Vec3 a;
        Vec3 b;

        constexpr Segment() noexcept = default;
        constexpr Segment(const Vec3& a_, const Vec3& b_) noexcept : a(a_), b(b_) {}

        constexpr Vec3 Direction() const noexcept { return b - a; }
        constexpr Vec3 Center() const noexcept { return (a + b) * 0.5f; }
        float Length() const noexcept { return (b - a).Length(); }
        constexpr Vec3 GetPoint(float t) const noexcept { return a + (b - a) * t; }
    };

    //============================================================
    // Triangle
    //============================================================
    struct alignas(16) Triangle
    {
        Vec3 a;
        Vec3 b;
        Vec3 c;

        constexpr Triangle() noexcept = default;
        constexpr Triangle(const Vec3& a_, const Vec3& b_, const Vec3& c_) noexcept : a(a_), b(b_), c(c_) {}

        // Unit normal, right-hand rule: Cross(b - a, c - a)
        Vec3 Normal() const noexcept { return (b - a).Cross(c - a).Normalized(); }
        float Area() const noexcept { return (b - a).Cross(c - a).Length() * 0.5f; }
        constexpr Vec3 Center() const noexcept { return (a + b + c) * (1.0f / 3.0f); }
        Plane ToPlane() const noexcept { return Plane::FromPoints(a, b, c); }

        // Barycentric weights (u, v, w) with p = u*a + v*b + w*c (p assumed on the plane)
        Vec3 Barycentric(const Vec3& p) const noexcept
        {
            Vec3 v0 = b - a, v1 = c - a, v2 = p - a;
            float d00 = v0.Dot(v0);
            float d01 = v0.Dot(v1);
            float d11 = v1.Dot(v1);
            float d20 = v2.Dot(v0);
            float d21 = v2.Dot(v1);
            float denom = d00 * d11 - d01 * d01;
            if (Abs(denom) < 1e-12f)
                return Vec3(1.0f, 0.0f, 0.0f);
            float v = (d11 * d20 - d01 * d21) / denom;
            float w = (d00 * d21 - d01 * d20) / denom;
            return Vec3(1.0f - v - w, v, w);
        }
    };

    //============================================================
    // OBB - oriented bounding box
    //============================================================
    struct alignas(16) OBB
    {
        Vec3 center;
        Vec3 halfExtents = Vec3(0.5f, 0.5f, 0.5f);
        Quat orientation;

        constexpr OBB() noexcept = default;
        constexpr OBB(const Vec3& center_, const Vec3& halfExtents_, const Quat& orientation_) noexcept
            : center(center_), halfExtents(halfExtents_), orientation(orientation_) {}

        // Local box transformed by a TRS matrix (shear is not representable and is dropped)
        static OBB FromAABB(const AABB& box, const Mat4x4& world) noexcept
        {
            Vec3 scale, translation;
            Quat rotation;
            world.Decompose(scale, rotation, translation);
            return OBB(world.TransformPoint(box.Center()), Vec3::Abs(box.Extents() * scale), rotation.Normalized());
        }

        static constexpr OBB FromAABB(const AABB& box) noexcept
        {
            return OBB(box.Center(), box.Extents(), Quat());
        }

        // Rotation matrix; row i is local axis i in world space
        constexpr Mat3x3 RotationMatrix() const noexcept { return Mat3x3::FromQuat(orientation); }

        void Axes(Vec3 out[3]) const noexcept
        {
            Mat3x3 r = RotationMatrix();
            out[0] = r.Row(0);
            out[1] = r.Row(1);
            out[2] = r.Row(2);
        }

        void Corners(Vec3 out[8]) const noexcept
        {
            Vec3 ax[3];
            Axes(ax);
            Vec3 ex = ax[0] * halfExtents.x;
            Vec3 ey = ax[1] * halfExtents.y;
            Vec3 ez = ax[2] * halfExtents.z;
            for (int i = 0; i < 8; ++i)
            {
                out[i] = center
                    + ((i & 1) ? ex : -ex)
                    + ((i & 2) ? ey : -ey)
                    + ((i & 4) ? ez : -ez);
            }
        }

        AABB ToAABB() const noexcept
        {
            Mat3x3 r = RotationMatrix();
            Vec3 e(
                Abs(r.m[0][0]) * halfExtents.x + Abs(r.m[1][0]) * halfExtents.y + Abs(r.m[2][0]) * halfExtents.z,
                Abs(r.m[0][1]) * halfExtents.x + Abs(r.m[1][1]) * halfExtents.y + Abs(r.m[2][1]) * halfExtents.z,
                Abs(r.m[0][2]) * halfExtents.x + Abs(r.m[1][2]) * halfExtents.y + Abs(r.m[2][2]) * halfExtents.z);
            return AABB(center - e, center + e);
        }

        // World point -> box-local point
        Vec3 ToLocal(const Vec3& p) const noexcept
        {
            Vec3 ax[3];
            Axes(ax);
            Vec3 d = p - center;
            return Vec3(d.Dot(ax[0]), d.Dot(ax[1]), d.Dot(ax[2]));
        }

        bool Contains(const Vec3& p) const noexcept
        {
            Vec3 l = ToLocal(p);
            return Abs(l.x) <= halfExtents.x && Abs(l.y) <= halfExtents.y && Abs(l.z) <= halfExtents.z;
        }
    };

    //============================================================
    // Capsule - segment a -> b swept by radius (character controllers)
    //============================================================
    struct alignas(16) Capsule
    {
        Vec3 a;
        Vec3 b = Vec3(0.0f, 1.0f, 0.0f);
        float radius = 0.5f;

        constexpr Capsule() noexcept = default;
        constexpr Capsule(const Vec3& a_, const Vec3& b_, float radius_) noexcept : a(a_), b(b_), radius(radius_) {}

        constexpr Segment Axis() const noexcept { return Segment(a, b); }
        constexpr Vec3 Center() const noexcept { return (a + b) * 0.5f; }
        float Height() const noexcept { return (b - a).Length() + 2.0f * radius; }

        constexpr AABB ToAABB() const noexcept
        {
            Vec3 r(radius, radius, radius);
            return AABB(Vec3::Min(a, b) - r, Vec3::Max(a, b) + r);
        }
    };

    //============================================================
    // Cylinder - flat-capped cylinder along a -> b
    //============================================================
    struct alignas(16) Cylinder
    {
        Vec3 a;
        Vec3 b = Vec3(0.0f, 1.0f, 0.0f);
        float radius = 0.5f;

        constexpr Cylinder() noexcept = default;
        constexpr Cylinder(const Vec3& a_, const Vec3& b_, float radius_) noexcept : a(a_), b(b_), radius(radius_) {}

        constexpr Vec3 Center() const noexcept { return (a + b) * 0.5f; }
        float Height() const noexcept { return (b - a).Length(); }

        // Exact bounds of the end discs
        AABB ToAABB() const noexcept
        {
            Vec3 d = (b - a).Normalized();
            Vec3 e(
                radius * sqrtf(Max(0.0f, 1.0f - d.x * d.x)),
                radius * sqrtf(Max(0.0f, 1.0f - d.y * d.y)),
                radius * sqrtf(Max(0.0f, 1.0f - d.z * d.z)));
            return AABB(Vec3::Min(a, b) - e, Vec3::Max(a, b) + e);
        }

        bool Contains(const Vec3& p) const noexcept
        {
            Vec3 axis = b - a;
            float lenSq = axis.LengthSq();
            if (lenSq < 1e-12f)
                return false;
            float t = (p - a).Dot(axis) / lenSq;
            if (t < 0.0f || t > 1.0f)
                return false;
            return (p - (a + axis * t)).LengthSq() <= radius * radius;
        }
    };

    //============================================================
    // Circle (2D)
    //============================================================
    struct Circle
    {
        Vec2 center;
        float radius = 0.5f;

        constexpr Circle() noexcept = default;
        constexpr Circle(const Vec2& center_, float radius_) noexcept : center(center_), radius(radius_) {}

        constexpr bool Contains(const Vec2& p) const noexcept { return (p - center).LengthSq() <= radius * radius; }
    };

    //============================================================
    // SweepHit - result of a swept (moving) shape test
    //============================================================
    struct alignas(16) SweepHit
    {
        Vec3 point;          // contact point at time of impact
        Vec3 normal;         // contact normal, pointing away from the static shape
        float t = 0.0f;      // time of impact in [0, 1] along the motion
    };

    //============================================================
    // Frustum vs OBB (declared in xgBounds.h)
    //============================================================
    inline Frustum::ContainmentType Frustum::Contains(const OBB& box) const noexcept
    {
        Vec3 ax[3];
        box.Axes(ax);
        bool intersects = false;
        for (int i = 0; i < Count; ++i)
        {
            Vec3 n(planes[i].x, planes[i].y, planes[i].z);
            float r = box.halfExtents.x * Abs(n.Dot(ax[0]))
                    + box.halfExtents.y * Abs(n.Dot(ax[1]))
                    + box.halfExtents.z * Abs(n.Dot(ax[2]));
            float s = n.Dot(box.center) + planes[i].w;
            if (s < -r)
                return ContainmentType::Disjoint;
            if (s < r)
                intersects = true;
        }
        return intersects ? ContainmentType::Intersects : ContainmentType::Contains;
    }

    //============================================================
    // Layout (mirrored in C#, checked at runtime by XG_MathSizeOf)
    //============================================================
    static_assert(std::is_trivially_copyable_v<Plane> && sizeof(Plane) == 32, "Plane layout");
    static_assert(std::is_trivially_copyable_v<Segment> && sizeof(Segment) == 32, "Segment layout");
    static_assert(std::is_trivially_copyable_v<Triangle> && sizeof(Triangle) == 48, "Triangle layout");
    static_assert(std::is_trivially_copyable_v<OBB> && sizeof(OBB) == 48, "OBB layout");
    static_assert(std::is_trivially_copyable_v<Capsule> && sizeof(Capsule) == 48, "Capsule layout");
    static_assert(std::is_trivially_copyable_v<Cylinder> && sizeof(Cylinder) == 48, "Cylinder layout");
    static_assert(std::is_trivially_copyable_v<Circle> && sizeof(Circle) == 12, "Circle layout");
    static_assert(std::is_trivially_copyable_v<SweepHit> && sizeof(SweepHit) == 48, "SweepHit layout");
} // namespace xg
