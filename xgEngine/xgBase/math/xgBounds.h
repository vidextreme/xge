// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.

#pragma once

// xgBounds.h - Bounding volumes (AABB, Sphere, Frustum)
// POD-friendly, trivially copyable, STL-free

#include "xgMathCommon.h"

namespace xg
{
    // Forward declarations
    struct AABB;
    struct Sphere;
    struct Frustum;
    struct Ray;

    namespace detail
    {
        // Bounds operations using XGVector/XGMatrix
        inline bool BoundsIntersectsAABB(XGVector minA, XGVector maxA, XGVector minB, XGVector maxB) noexcept;
        inline bool BoundsIntersectsSphere(XGVector center, float radius, XGVector min, XGVector max) noexcept;
        inline bool BoundsContainsPoint(XGVector point, XGVector min, XGVector max) noexcept;
        inline void BoundsMerge(XGVector minA, XGVector maxA, XGVector minB, XGVector maxB, XGVector& outMin, XGVector& outMax) noexcept;
        inline void BoundsExpand(XGVector min, XGVector max, XGVector point, XGVector& outMin, XGVector& outMax) noexcept;
        inline void BoundsTransform(XGVector min, XGVector max, XGMatrix m, XGVector& outMin, XGVector& outMax) noexcept;
    }

    //============================================================
    // AABB - Axis-Aligned Bounding Box
    //============================================================
    struct alignas(16) AABB
    {
        Vec3 min = Vec3::PositiveInfinity();
        Vec3 max = Vec3::NegativeInfinity();

        //--------------------------------------------------------
        // Constructors
        //--------------------------------------------------------
        constexpr AABB() noexcept = default;

        constexpr AABB(const Vec3& min_, const Vec3& max_) noexcept
            : min(min_), max(max_) {}

        constexpr AABB(float minX, float minY, float minZ, float maxX, float maxY, float maxZ) noexcept
            : min(minX, minY, minZ), max(maxX, maxY, maxZ) {}

        // Create from center and extents
        static AABB FromCenterExtents(const Vec3& center, const Vec3& extents) noexcept
        {
            return AABB(center - extents, center + extents);
        }

        // Create from points
        static AABB FromPoints(const Vec3* points, size_t count) noexcept
        {
            AABB box;
            if (count > 0)
            {
                box.min = box.max = points[0];
                for (size_t i = 1; i < count; ++i)
                {
                    box.Encapsulate(points[i]);
                }
            }
            return box;
        }

        //--------------------------------------------------------
        // Properties
        //--------------------------------------------------------
        constexpr Vec3 Center() const noexcept
        {
            return (min + max) * 0.5f;
        }

        constexpr Vec3 Extents() const noexcept
        {
            return (max - min) * 0.5f;
        }

        constexpr Vec3 Size() const noexcept
        {
            return max - min;
        }

        constexpr float Volume() const noexcept
        {
            Vec3 s = Size();
            return s.x * s.y * s.z;
        }

        constexpr float SurfaceArea() const noexcept
        {
            Vec3 s = Size();
            return 2.0f * (s.x * s.y + s.y * s.z + s.z * s.x);
        }

        constexpr bool IsValid() const noexcept
        {
            return min.x <= max.x && min.y <= max.y && min.z <= max.z;
        }

        constexpr bool IsEmpty() const noexcept
        {
            return !IsValid();
        }

        //--------------------------------------------------------
        // Point operations
        //--------------------------------------------------------
        constexpr bool Contains(const Vec3& point) const noexcept
        {
            return point.x >= min.x && point.x <= max.x &&
                   point.y >= min.y && point.y <= max.y &&
                   point.z >= min.z && point.z <= max.z;
        }

        Vec3 ClosestPoint(const Vec3& point) const noexcept
        {
            return Vec3::Clamp(point, min, max);
        }

        float DistanceSq(const Vec3& point) const noexcept
        {
            Vec3 closest = ClosestPoint(point);
            return (closest - point).LengthSq();
        }

        float Distance(const Vec3& point) const noexcept
        {
            return sqrtf(DistanceSq(point));
        }

        //--------------------------------------------------------
        // Modification
        //--------------------------------------------------------
        void Encapsulate(const Vec3& point) noexcept
        {
            min = Vec3::Min(min, point);
            max = Vec3::Max(max, point);
        }

        void Encapsulate(const AABB& other) noexcept
        {
            min = Vec3::Min(min, other.min);
            max = Vec3::Max(max, other.max);
        }

        void Expand(float amount) noexcept
        {
            Vec3 v(amount, amount, amount);
            min -= v;
            max += v;
        }

        void Expand(const Vec3& amount) noexcept
        {
            min -= amount;
            max += amount;
        }

        void SetInvalid() noexcept
        {
            min = Vec3::PositiveInfinity();
            max = Vec3::NegativeInfinity();
        }

        //--------------------------------------------------------
        // Intersection tests
        //--------------------------------------------------------
        bool Intersects(const AABB& other) const noexcept
        {
            return detail::BoundsIntersectsAABB(min.ToXM(), max.ToXM(), other.min.ToXM(), other.max.ToXM());
        }

        bool Intersects(const Sphere& sphere) const noexcept;

        bool Intersects(const Ray& ray, float& tNear, float& tFar) const noexcept;

        // Containment
        bool Contains(const AABB& other) const noexcept
        {
            return other.min.x >= min.x && other.max.x <= max.x &&
                   other.min.y >= min.y && other.max.y <= max.y &&
                   other.min.z >= min.z && other.max.z <= max.z;
        }

        // Transform
        AABB Transform(const Mat4x4& matrix) const noexcept
        {
            AABB result;
            detail::BoundsTransform(min.ToXM(), max.ToXM(), matrix.ToXM(), result.min.ToXM(), result.max.ToXM());
            return result;
        }

        void Transform(const Mat4x4& matrix, AABB& out) const noexcept
        {
            detail::BoundsTransform(min.ToXM(), max.ToXM(), matrix.ToXM(), out.min.ToXM(), out.max.ToXM());
        }

        //--------------------------------------------------------
        // Static constants
        //--------------------------------------------------------
        static constexpr AABB Invalid() noexcept
        {
            return AABB(Vec3::PositiveInfinity(), Vec3::NegativeInfinity());
        }

        static constexpr AABB Zero() noexcept
        {
            return AABB(Vec3::Zero(), Vec3::Zero());
        }

        static constexpr AABB Unit() noexcept
        {
            return AABB(Vec3::Zero(), Vec3::One());
        }

        static constexpr AABB UnitCentered() noexcept
        {
            return AABB(Vec3(-0.5f), Vec3(0.5f));
        }
    };

    //============================================================
    // Sphere
    //============================================================
    struct alignas(16) Sphere
    {
        Vec3 center = Vec3::Zero();
        float radius = 0.0f;

        //--------------------------------------------------------
        // Constructors
        //--------------------------------------------------------
        constexpr Sphere() noexcept = default;

        constexpr Sphere(const Vec3& c, float r) noexcept
            : center(c), radius(r) {}

        constexpr Sphere(float x, float y, float z, float r) noexcept
            : center(x, y, z), radius(r) {}

        static Sphere FromPoints(const Vec3* points, size_t count) noexcept
        {
            if (count == 0) return Sphere();
            if (count == 1) return Sphere(points[0], 0.0f);

            // Ritter's algorithm for bounding sphere
            Vec3 minPt = points[0];
            Vec3 maxPt = points[0];

            for (size_t i = 1; i < count; ++i)
            {
                minPt = Vec3::Min(minPt, points[i]);
                maxPt = Vec3::Max(maxPt, points[i]);
            }

            Vec3 center = (minPt + maxPt) * 0.5f;
            float radius = 0.0f;

            for (size_t i = 0; i < count; ++i)
            {
                float dist = (points[i] - center).Length();
                if (dist > radius)
                    radius = dist;
            }

            return Sphere(center, radius);
        }

        //--------------------------------------------------------
        // Properties
        //--------------------------------------------------------
        constexpr float Diameter() const noexcept { return radius * 2.0f; }
        constexpr float Volume() const noexcept { return (4.0f / 3.0f) * PI * radius * radius * radius; }
        constexpr float SurfaceArea() const noexcept { return 4.0f * PI * radius * radius; }
        constexpr bool IsValid() const noexcept { return radius >= 0.0f; }

        //--------------------------------------------------------
        // Point operations
        //--------------------------------------------------------
        constexpr bool Contains(const Vec3& point) const noexcept
        {
            return (point - center).LengthSq() <= radius * radius;
        }

        Vec3 ClosestPoint(const Vec3& point) const noexcept
        {
            Vec3 dir = point - center;
            float dist = dir.Length();
            if (dist > radius && dist > EPSILON_POS)
                return center + dir * (radius / dist);
            return point;
        }

        float Distance(const Vec3& point) const noexcept
        {
            return (point - center).Length() - radius;
        }

        float DistanceSq(const Vec3& point) const noexcept
        {
            float d = Distance(point);
            return d * d;
        }

        //--------------------------------------------------------
        // Modification
        //--------------------------------------------------------
        void Encapsulate(const Vec3& point) noexcept
        {
            Vec3 dir = point - center;
            float dist = dir.Length();
            if (dist > radius)
            {
                if (radius > 0.0f)
                {
                    float newRadius = (radius + dist) * 0.5f;
                    center += dir * ((newRadius - radius) / dist);
                    radius = newRadius;
                }
                else
                {
                    center = point;
                    radius = 0.0f;
                }
            }
        }

        void Encapsulate(const Sphere& other) noexcept
        {
            Vec3 dir = other.center - center;
            float dist = dir.Length();
            float r1 = radius;
            float r2 = other.radius;

            if (dist + r2 <= r1) return; // other inside this
            if (dist + r1 <= r2) { *this = other; return; } // this inside other

            float newRadius = (r1 + r2 + dist) * 0.5f;
            center += dir * ((newRadius - r1) / dist);
            radius = newRadius;
        }

        void Expand(float amount) noexcept
        {
            radius += amount;
        }

        //--------------------------------------------------------
        // Intersection tests
        //--------------------------------------------------------
        bool Intersects(const Sphere& other) const noexcept
        {
            float distSq = (center - other.center).LengthSq();
            float radiusSum = radius + other.radius;
            return distSq <= radiusSum * radiusSum;
        }

        bool Intersects(const AABB& box) const noexcept;

        bool Intersects(const Ray& ray, float& tNear, float& tFar) const noexcept;

        // Containment
        bool Contains(const Sphere& other) const noexcept
        {
            float dist = (center - other.center).Length();
            return dist + other.radius <= radius;
        }

        // Transform
        Sphere Transform(const Mat4x4& matrix) const noexcept
        {
            Sphere result;
            result.center = matrix.TransformPoint(center);
            // For uniform scale only
            Vec3 scale = matrix.GetScale();
            result.radius = radius * fmaxf(fmaxf(scale.x, scale.y), scale.z);
            return result;
        }

        //--------------------------------------------------------
        // Static constants
        //--------------------------------------------------------
        static constexpr Sphere Unit() noexcept { return Sphere(Vec3::Zero(), 1.0f); }
        static constexpr Sphere UnitCentered() noexcept { return Sphere(Vec3::Zero(), 0.5f); }
        static constexpr Sphere Invalid() noexcept { return Sphere(Vec3::Zero(), -1.0f); }
    };

    //============================================================
    // Frustum (for culling)
    //============================================================
    struct alignas(16) Frustum
    {
        enum PlaneIndex : uint8_t
        {
            Near = 0,
            Far = 1,
            Left = 2,
            Right = 3,
            Top = 4,
            Bottom = 5,
            Count = 6
        };

        Vec4 planes[Count]; // x, y, z = normal, w = distance

        //--------------------------------------------------------
        // Constructors
        //--------------------------------------------------------
        constexpr Frustum() noexcept = default;

        // Create from view-projection matrix
        static Frustum FromMatrix(const Mat4x4& viewProj) noexcept
        {
            Frustum f;
            // Extract planes from matrix
            const float* m = &viewProj.m[0][0];

            // Near: m3 + m2
            f.planes[Near] = Vec4(
                m[3] + m[2], m[7] + m[6], m[11] + m[10], m[15] + m[14]
            );

            // Far: m3 - m2
            f.planes[Far] = Vec4(
                m[3] - m[2], m[7] - m[6], m[11] - m[10], m[15] - m[14]
            );

            // Left: m3 + m0
            f.planes[Left] = Vec4(
                m[3] + m[0], m[7] + m[4], m[11] + m[8], m[15] + m[12]
            );

            // Right: m3 - m0
            f.planes[Right] = Vec4(
                m[3] - m[0], m[7] - m[4], m[11] - m[8], m[15] - m[12]
            );

            // Top: m3 - m1
            f.planes[Top] = Vec4(
                m[3] - m[1], m[7] - m[5], m[11] - m[9], m[15] - m[13]
            );

            // Bottom: m3 + m1
            f.planes[Bottom] = Vec4(
                m[3] + m[1], m[7] + m[5], m[11] + m[9], m[15] + m[13]
            );

            // Normalize planes
            for (int i = 0; i < Count; ++i)
            {
                Vec3 normal(f.planes[i].x, f.planes[i].y, f.planes[i].z);
                float len = normal.Length();
                if (len > EPSILON_POS)
                {
                    float invLen = 1.0f / len;
                    f.planes[i].x *= invLen;
                    f.planes[i].y *= invLen;
                    f.planes[i].z *= invLen;
                    f.planes[i].w *= invLen;
                }
            }

            return f;
        }

        //--------------------------------------------------------
        // Culling tests
        //--------------------------------------------------------
        enum class ContainmentType
        {
            Disjoint,
            Contains,
            Intersects
        };

        ContainmentType Contains(const Vec3& point) const noexcept
        {
            for (int i = 0; i < Count; ++i)
            {
                float dist = planes[i].x * point.x + planes[i].y * point.y + planes[i].z * point.z + planes[i].w;
                if (dist < 0.0f)
                    return ContainmentType::Disjoint;
            }
            return ContainmentType::Contains;
        }

        ContainmentType Contains(const AABB& box) const noexcept
        {
            bool intersects = false;
            for (int i = 0; i < Count; ++i)
            {
                Vec3 normal(planes[i].x, planes[i].y, planes[i].z);
                float dist = planes[i].w;

                // Find the positive vertex
                Vec3 posVertex(
                    normal.x >= 0 ? box.max.x : box.min.x,
                    normal.y >= 0 ? box.max.y : box.min.y,
                    normal.z >= 0 ? box.max.z : box.min.z
                );

                float d = normal.Dot(posVertex) + dist;
                if (d < 0.0f)
                    return ContainmentType::Disjoint;

                // Find the negative vertex
                Vec3 negVertex(
                    normal.x >= 0 ? box.min.x : box.max.x,
                    normal.y >= 0 ? box.min.y : box.max.y,
                    normal.z >= 0 ? box.min.z : box.max.z
                );

                d = normal.Dot(negVertex) + dist;
                if (d < 0.0f)
                    intersects = true;
            }
            return intersects ? ContainmentType::Intersects : ContainmentType::Contains;
        }

        ContainmentType Contains(const Sphere& sphere) const noexcept
        {
            for (int i = 0; i < Count; ++i)
            {
                float dist = planes[i].x * sphere.center.x +
                             planes[i].y * sphere.center.y +
                             planes[i].z * sphere.center.z +
                             planes[i].w;

                if (dist < -sphere.radius)
                    return ContainmentType::Disjoint;

                if (dist < sphere.radius)
                    return ContainmentType::Intersects;
            }
            return ContainmentType::Contains;
        }

        // Intersection with ray
        bool Intersects(const Ray& ray, float& tNear, float& tFar) const noexcept;
    };

    // Static assertions
    static_assert(std::is_trivial_v<AABB>, "AABB must be trivial");
    static_assert(std::is_standard_layout_v<AABB>, "AABB must be standard layout");
    static_assert(std::is_trivially_copyable_v<AABB>, "AABB must be trivially copyable");
    static_assert(sizeof(AABB) == 2 * sizeof(Vec3), "AABB size mismatch");

    static_assert(std::is_trivial_v<Sphere>, "Sphere must be trivial");
    static_assert(std::is_standard_layout_v<Sphere>, "Sphere must be standard layout");
    static_assert(std::is_trivially_copyable_v<Sphere>, "Sphere must be trivially copyable");
    static_assert(sizeof(Sphere) == sizeof(Vec3) + sizeof(float), "Sphere size mismatch");

    static_assert(std::is_trivial_v<Frustum>, "Frustum must be trivial");
    static_assert(std::is_standard_layout_v<Frustum>, "Frustum must be standard layout");
    static_assert(std::is_trivially_copyable_v<Frustum>, "Frustum must be trivially copyable");
    static_assert(sizeof(Frustum) == 6 * sizeof(Vec4), "Frustum size mismatch");
} // namespace xg