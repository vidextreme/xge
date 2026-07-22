// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.

#pragma once

// xgRay.h - Ray type for ray casting
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
        inline bool RayIntersectsAABB(XGVector origin, XGVector direction, XGVector min, XGVector max, float& tNear, float& tFar) noexcept;
        inline bool RayIntersectsSphere(XGVector origin, XGVector direction, XGVector center, float radius, float& tNear, float& tFar) noexcept;
        inline bool RayIntersectsPlane(XGVector origin, XGVector direction, XGVector plane, float& t) noexcept;
        inline bool RayIntersectsTriangle(XGVector origin, XGVector direction, XGVector v0, XGVector v1, XGVector v2, float& t, float& u, float& v) noexcept;
    }

    //============================================================
    // Ray - Origin + Direction
    //============================================================
    struct alignas(16) Ray
    {
        Vec3 origin = Vec3::Zero();
        Vec3 direction = Vec3::UnitZ(); // Default to forward

        //--------------------------------------------------------
        // Constructors
        //--------------------------------------------------------
        constexpr Ray() noexcept = default;

        constexpr Ray(const Vec3& origin_, const Vec3& direction_) noexcept
            : origin(origin_), direction(direction_.Normalized()) {}

        constexpr Ray(float ox, float oy, float oz, float dx, float dy, float dz) noexcept
            : origin(ox, oy, oz), direction(Vec3(dx, dy, dz).Normalized()) {}

        constexpr Ray(const Ray&) noexcept = default;
        constexpr Ray(Ray&&) noexcept = default;
        constexpr Ray& operator=(const Ray&) noexcept = default;
        constexpr Ray& operator=(Ray&&) noexcept = default;

        //--------------------------------------------------------
        // Properties
        //--------------------------------------------------------
        constexpr Vec3 GetPoint(float t) const noexcept
        {
            return origin + direction * t;
        }

        constexpr Vec3 GetPoint(double t) const noexcept
        {
            return origin + direction * static_cast<float>(t);
        }

        //--------------------------------------------------------
        // Intersection tests
        //--------------------------------------------------------

        // AABB
        bool Intersects(const AABB& box, float& tNear, float& tFar) const noexcept
        {
            return detail::RayIntersectsAABB(
                origin.ToXM(), direction.ToXM(),
                box.min.ToXM(), box.max.ToXM(),
                tNear, tFar);
        }

        bool Intersects(const AABB& box) const noexcept
        {
            float tNear, tFar;
            return Intersects(box, tNear, tFar);
        }

        // Sphere
        bool Intersects(const Sphere& sphere, float& tNear, float& tFar) const noexcept
        {
            return detail::RayIntersectsSphere(
                origin.ToXM(), direction.ToXM(),
                sphere.center.ToXM(), sphere.radius,
                tNear, tFar);
        }

        bool Intersects(const Sphere& sphere) const noexcept
        {
            float tNear, tFar;
            return Intersects(sphere, tNear, tFar);
        }

        // Plane
        struct Plane
        {
            Vec3 normal = Vec3::UnitY();
            float distance = 0.0f; // d in ax + by + cz + d = 0

            constexpr Plane() noexcept = default;
            constexpr Plane(const Vec3& n, float d) noexcept : normal(n.Normalized()), distance(d) {}
            constexpr Plane(float nx, float ny, float nz, float d) noexcept : normal(Vec3(nx, ny, nz).Normalized()), distance(d) {}

            static Plane FromPointNormal(const Vec3& point, const Vec3& normal) noexcept
            {
                return Plane(normal.Normalized(), -normal.Normalized().Dot(point));
            }

            float Distance(const Vec3& point) const noexcept
            {
                return normal.Dot(point) + distance;
            }
        };

        bool Intersects(const Plane& plane, float& t) const noexcept
        {
            XGVector planeVec = detail::Vec4Set(plane.normal.x, plane.normal.y, plane.normal.z, plane.distance);
            return detail::RayIntersectsPlane(origin.ToXM(), direction.ToXM(), planeVec, t);
        }

        // Triangle
        bool IntersectsTriangle(const Vec3& v0, const Vec3& v1, const Vec3& v2, float& t, float& u, float& v) const noexcept
        {
            return detail::RayIntersectsTriangle(
                origin.ToXM(), direction.ToXM(),
                v0.ToXM(), v1.ToXM(), v2.ToXM(),
                t, u, v);
        }

        // Frustum
        bool Intersects(const Frustum& frustum, float& tNear, float& tFar) const noexcept
        {
            tNear = -INFINITY_F;
            tFar = INFINITY_F;

            for (int i = 0; i < Frustum::Count; ++i)
            {
                const Vec4& plane = frustum.planes[i];
                float denom = direction.Dot(Vec3(plane.x, plane.y, plane.z));

                if (IsZero(denom))
                {
                    // Ray is parallel to plane
                    float numer = origin.Dot(Vec3(plane.x, plane.y, plane.z)) + plane.w;
                    if (numer < 0.0f)
                        return false; // Outside
                    continue; // Inside, check other planes
                }

                float numer = origin.Dot(Vec3(plane.x, plane.y, plane.z)) + plane.w;
                float t = -numer / denom;

                if (denom > 0.0f) // Entering
                {
                    if (t > tNear) tNear = t;
                    if (tNear > tFar) return false;
                }
                else // Exiting
                {
                    if (t < tFar) tFar = t;
                    if (tNear > tFar) return false;
                }
            }
            return tFar >= 0.0f;
        }

        //--------------------------------------------------------
        // Transform
        //--------------------------------------------------------
        Ray Transform(const Mat4x4& matrix) const noexcept
        {
            Ray result;
            result.origin = matrix.TransformPoint(origin);
            result.direction = matrix.TransformVector(direction).Normalized();
            return result;
        }

        //--------------------------------------------------------
        // Static constants
        //--------------------------------------------------------
        static constexpr Ray Zero() noexcept { return Ray(Vec3::Zero(), Vec3::UnitZ()); }
    };

    //============================================================
    // RaycastHit - Result of ray casting
    //============================================================
    struct RaycastHit
    {
        float distance = INFINITY_F;
        Vec3 point = Vec3::Zero();
        Vec3 normal = Vec3::Zero();
        float u = 0.0f;
        float v = 0.0f;
        int triangleIndex = -1;
        void* userData = nullptr;

        constexpr bool HasHit() const noexcept
        {
            return distance < INFINITY_F;
        }
    };

    // Static assertions
    static_assert(std::is_trivial_v<Ray>, "Ray must be trivial");
    static_assert(std::is_standard_layout_v<Ray>, "Ray must be standard layout");
    static_assert(std::is_trivially_copyable_v<Ray>, "Ray must be trivially copyable");
    static_assert(sizeof(Ray) == 2 * sizeof(Vec3), "Ray size mismatch");

    static_assert(std::is_trivial_v<Ray::Plane>, "Ray::Plane must be trivial");
    static_assert(std::is_standard_layout_v<Ray::Plane>, "Ray::Plane must be standard layout");
    static_assert(std::is_trivially_copyable_v<Ray::Plane>, "Ray::Plane must be trivially copyable");

    static_assert(std::is_trivial_v<RaycastHit>, "RaycastHit must be trivial");
    static_assert(std::is_standard_layout_v<RaycastHit>, "RaycastHit must be standard layout");
    static_assert(std::is_trivially_copyable_v<RaycastHit>, "RaycastHit must be trivially copyable");
} // namespace xg