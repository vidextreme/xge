// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.

#pragma once

// xgGeometry.h - Geometric queries between shapes
// Closest points, overlap tests, plane classification, ray casts,
// swept (moving) tests and 2D tests.
// Algorithms follow Ericson, "Real-Time Collision Detection" (section noted per function).

#include "xgMathCommon.h"
#include "xgVec2.h"
#include "xgVec3.h"
#include "xgMat3x3.h"
#include "xgBounds.h"
#include "xgRay.h"
#include "xgShapes.h"
#include "xgRect.h"

namespace xg
{
    //============================================================
    // Closest points
    //============================================================

    // Closest point on segment ab to p; outT receives the segment parameter [0, 1] (5.1.2)
    inline Vec3 ClosestPointOnSegment(const Vec3& p, const Vec3& a, const Vec3& b, float* outT = nullptr) noexcept
    {
        Vec3 ab = b - a;
        float lenSq = ab.LengthSq();
        float t = (lenSq > 1e-12f) ? Clamp((p - a).Dot(ab) / lenSq, 0.0f, 1.0f) : 0.0f;
        if (outT) *outT = t;
        return a + ab * t;
    }

    inline Vec3 ClosestPointOnSegment(const Vec3& p, const Segment& seg, float* outT = nullptr) noexcept
    {
        return ClosestPointOnSegment(p, seg.a, seg.b, outT);
    }

    // Closest points between segments p1q1 and p2q2 (5.1.9).
    // Returns the squared distance; s/t are the segment parameters, c1/c2 the points.
    inline float ClosestPointsSegmentSegment(const Vec3& p1, const Vec3& q1, const Vec3& p2, const Vec3& q2,
        float& s, float& t, Vec3& c1, Vec3& c2) noexcept
    {
        Vec3 d1 = q1 - p1;
        Vec3 d2 = q2 - p2;
        Vec3 r = p1 - p2;
        float a = d1.Dot(d1);
        float e = d2.Dot(d2);
        float f = d2.Dot(r);
        const float eps = 1e-12f;

        if (a <= eps && e <= eps)
        {
            s = t = 0.0f;
            c1 = p1;
            c2 = p2;
            return (c1 - c2).LengthSq();
        }

        if (a <= eps)
        {
            s = 0.0f;
            t = Clamp(f / e, 0.0f, 1.0f);
        }
        else
        {
            float c = d1.Dot(r);
            if (e <= eps)
            {
                t = 0.0f;
                s = Clamp(-c / a, 0.0f, 1.0f);
            }
            else
            {
                float b = d1.Dot(d2);
                float denom = a * e - b * b;
                s = (denom != 0.0f) ? Clamp((b * f - c * e) / denom, 0.0f, 1.0f) : 0.0f;
                t = (b * s + f) / e;
                if (t < 0.0f)
                {
                    t = 0.0f;
                    s = Clamp(-c / a, 0.0f, 1.0f);
                }
                else if (t > 1.0f)
                {
                    t = 1.0f;
                    s = Clamp((b - c) / a, 0.0f, 1.0f);
                }
            }
        }

        c1 = p1 + d1 * s;
        c2 = p2 + d2 * t;
        return (c1 - c2).LengthSq();
    }

    // Closest point on triangle abc to p (5.1.5)
    inline Vec3 ClosestPointOnTriangle(const Vec3& p, const Vec3& a, const Vec3& b, const Vec3& c) noexcept
    {
        Vec3 ab = b - a;
        Vec3 ac = c - a;
        Vec3 ap = p - a;
        float d1 = ab.Dot(ap);
        float d2 = ac.Dot(ap);
        if (d1 <= 0.0f && d2 <= 0.0f) return a;

        Vec3 bp = p - b;
        float d3 = ab.Dot(bp);
        float d4 = ac.Dot(bp);
        if (d3 >= 0.0f && d4 <= d3) return b;

        float vc = d1 * d4 - d3 * d2;
        if (vc <= 0.0f && d1 >= 0.0f && d3 <= 0.0f)
            return a + ab * (d1 / (d1 - d3));

        Vec3 cp = p - c;
        float d5 = ab.Dot(cp);
        float d6 = ac.Dot(cp);
        if (d6 >= 0.0f && d5 <= d6) return c;

        float vb = d5 * d2 - d1 * d6;
        if (vb <= 0.0f && d2 >= 0.0f && d6 <= 0.0f)
            return a + ac * (d2 / (d2 - d6));

        float va = d3 * d6 - d5 * d4;
        if (va <= 0.0f && (d4 - d3) >= 0.0f && (d5 - d6) >= 0.0f)
            return b + (c - b) * ((d4 - d3) / ((d4 - d3) + (d5 - d6)));

        float denom = 1.0f / (va + vb + vc);
        return a + ab * (vb * denom) + ac * (vc * denom);
    }

    inline Vec3 ClosestPointOnTriangle(const Vec3& p, const Triangle& tri) noexcept
    {
        return ClosestPointOnTriangle(p, tri.a, tri.b, tri.c);
    }

    // Closest point on (or in) the OBB to p (5.1.4)
    inline Vec3 ClosestPointOnOBB(const Vec3& p, const OBB& box) noexcept
    {
        Vec3 ax[3];
        box.Axes(ax);
        Vec3 d = p - box.center;
        Vec3 q = box.center;
        for (int i = 0; i < 3; ++i)
        {
            float dist = Clamp(d.Dot(ax[i]), -box.halfExtents[i], box.halfExtents[i]);
            q += ax[i] * dist;
        }
        return q;
    }

    inline Vec3 ClosestPointOnPlane(const Vec3& p, const Plane& plane) noexcept
    {
        return plane.Project(p);
    }

    // Closest point on the capsule surface; returns p itself if p is inside
    inline Vec3 ClosestPointOnCapsule(const Vec3& p, const Capsule& cap) noexcept
    {
        Vec3 q = ClosestPointOnSegment(p, cap.a, cap.b);
        Vec3 d = p - q;
        float len = d.Length();
        if (len <= cap.radius)
            return p;
        return q + d * (cap.radius / len);
    }

    //============================================================
    // Overlap tests
    //============================================================

    inline bool Intersects(const Sphere& s, const Capsule& cap) noexcept
    {
        Vec3 q = ClosestPointOnSegment(s.center, cap.a, cap.b);
        float r = s.radius + cap.radius;
        return (s.center - q).LengthSq() <= r * r;
    }

    inline bool Intersects(const Capsule& a, const Capsule& b) noexcept
    {
        float s, t;
        Vec3 c1, c2;
        float distSq = ClosestPointsSegmentSegment(a.a, a.b, b.a, b.b, s, t, c1, c2);
        float r = a.radius + b.radius;
        return distSq <= r * r;
    }

    inline bool Intersects(const Sphere& s, const OBB& box) noexcept
    {
        Vec3 q = ClosestPointOnOBB(s.center, box);
        return (q - s.center).LengthSq() <= s.radius * s.radius;
    }

    inline bool Intersects(const Sphere& s, const Triangle& tri) noexcept
    {
        Vec3 q = ClosestPointOnTriangle(s.center, tri);
        return (q - s.center).LengthSq() <= s.radius * s.radius;
    }

    // Separating axis test with the 15 candidate axes (4.4.1)
    inline bool Intersects(const OBB& a, const OBB& b) noexcept
    {
        const float eps = 1e-6f;
        Vec3 A[3], B[3];
        a.Axes(A);
        b.Axes(B);

        float R[3][3], AbsR[3][3];
        for (int i = 0; i < 3; ++i)
            for (int j = 0; j < 3; ++j)
            {
                R[i][j] = A[i].Dot(B[j]);
                AbsR[i][j] = Abs(R[i][j]) + eps; // epsilon guards near-parallel edges
            }

        Vec3 d = b.center - a.center;
        float t[3] = { d.Dot(A[0]), d.Dot(A[1]), d.Dot(A[2]) };
        const Vec3& ea = a.halfExtents;
        const Vec3& eb = b.halfExtents;
        float ra, rb;

        // Axes A0, A1, A2
        for (int i = 0; i < 3; ++i)
        {
            ra = ea[i];
            rb = eb[0] * AbsR[i][0] + eb[1] * AbsR[i][1] + eb[2] * AbsR[i][2];
            if (Abs(t[i]) > ra + rb) return false;
        }

        // Axes B0, B1, B2
        for (int j = 0; j < 3; ++j)
        {
            ra = ea[0] * AbsR[0][j] + ea[1] * AbsR[1][j] + ea[2] * AbsR[2][j];
            rb = eb[j];
            if (Abs(t[0] * R[0][j] + t[1] * R[1][j] + t[2] * R[2][j]) > ra + rb) return false;
        }

        // A0 x B0..B2
        ra = ea[1] * AbsR[2][0] + ea[2] * AbsR[1][0]; rb = eb[1] * AbsR[0][2] + eb[2] * AbsR[0][1];
        if (Abs(t[2] * R[1][0] - t[1] * R[2][0]) > ra + rb) return false;
        ra = ea[1] * AbsR[2][1] + ea[2] * AbsR[1][1]; rb = eb[0] * AbsR[0][2] + eb[2] * AbsR[0][0];
        if (Abs(t[2] * R[1][1] - t[1] * R[2][1]) > ra + rb) return false;
        ra = ea[1] * AbsR[2][2] + ea[2] * AbsR[1][2]; rb = eb[0] * AbsR[0][1] + eb[1] * AbsR[0][0];
        if (Abs(t[2] * R[1][2] - t[1] * R[2][2]) > ra + rb) return false;

        // A1 x B0..B2
        ra = ea[0] * AbsR[2][0] + ea[2] * AbsR[0][0]; rb = eb[1] * AbsR[1][2] + eb[2] * AbsR[1][1];
        if (Abs(t[0] * R[2][0] - t[2] * R[0][0]) > ra + rb) return false;
        ra = ea[0] * AbsR[2][1] + ea[2] * AbsR[0][1]; rb = eb[0] * AbsR[1][2] + eb[2] * AbsR[1][0];
        if (Abs(t[0] * R[2][1] - t[2] * R[0][1]) > ra + rb) return false;
        ra = ea[0] * AbsR[2][2] + ea[2] * AbsR[0][2]; rb = eb[0] * AbsR[1][1] + eb[1] * AbsR[1][0];
        if (Abs(t[0] * R[2][2] - t[2] * R[0][2]) > ra + rb) return false;

        // A2 x B0..B2
        ra = ea[0] * AbsR[1][0] + ea[1] * AbsR[0][0]; rb = eb[1] * AbsR[2][2] + eb[2] * AbsR[2][1];
        if (Abs(t[1] * R[0][0] - t[0] * R[1][0]) > ra + rb) return false;
        ra = ea[0] * AbsR[1][1] + ea[1] * AbsR[0][1]; rb = eb[0] * AbsR[2][2] + eb[2] * AbsR[2][0];
        if (Abs(t[1] * R[0][1] - t[0] * R[1][1]) > ra + rb) return false;
        ra = ea[0] * AbsR[1][2] + ea[1] * AbsR[0][2]; rb = eb[0] * AbsR[2][1] + eb[1] * AbsR[2][0];
        if (Abs(t[1] * R[0][2] - t[0] * R[1][2]) > ra + rb) return false;

        return true;
    }

    inline bool Intersects(const AABB& a, const OBB& b) noexcept
    {
        return Intersects(OBB::FromAABB(a), b);
    }

    //============================================================
    // Plane classification
    //============================================================
    enum class PlaneSide : uint8_t
    {
        Back = 0,         // entirely on the negative side
        Front = 1,        // entirely on the normal side
        Intersecting = 2  // straddles the plane
    };

    namespace detail
    {
        inline PlaneSide ClassifyAgainstPlane(float signedDist, float radius) noexcept
        {
            if (signedDist > radius) return PlaneSide::Front;
            if (signedDist < -radius) return PlaneSide::Back;
            return PlaneSide::Intersecting;
        }
    }

    inline PlaneSide Classify(const Plane& plane, const Sphere& s) noexcept
    {
        return detail::ClassifyAgainstPlane(plane.Distance(s.center), s.radius);
    }

    inline PlaneSide Classify(const Plane& plane, const AABB& box) noexcept
    {
        Vec3 e = box.Extents();
        float r = e.x * Abs(plane.normal.x) + e.y * Abs(plane.normal.y) + e.z * Abs(plane.normal.z);
        return detail::ClassifyAgainstPlane(plane.Distance(box.Center()), r);
    }

    inline PlaneSide Classify(const Plane& plane, const OBB& box) noexcept
    {
        Vec3 ax[3];
        box.Axes(ax);
        float r = box.halfExtents.x * Abs(plane.normal.Dot(ax[0]))
                + box.halfExtents.y * Abs(plane.normal.Dot(ax[1]))
                + box.halfExtents.z * Abs(plane.normal.Dot(ax[2]));
        return detail::ClassifyAgainstPlane(plane.Distance(box.center), r);
    }

    //============================================================
    // Ray casts (ray.direction is unit length; t is distance along the ray)
    // A ray starting inside the shape hits at t = 0.
    //============================================================

    namespace detail
    {
        // Slab test against [-e, e] in local space; returns entry distance
        inline bool RaySlabs(const Vec3& o, const Vec3& d, const Vec3& lo, const Vec3& hi, float tMax, float& tHit) noexcept
        {
            float tMin = 0.0f;
            for (int i = 0; i < 3; ++i)
            {
                if (Abs(d[i]) < 1e-12f)
                {
                    if (o[i] < lo[i] || o[i] > hi[i])
                        return false;
                }
                else
                {
                    float inv = 1.0f / d[i];
                    float t1 = (lo[i] - o[i]) * inv;
                    float t2 = (hi[i] - o[i]) * inv;
                    if (t1 > t2) Swap(t1, t2);
                    tMin = Max(tMin, t1);
                    tMax = Min(tMax, t2);
                    if (tMin > tMax)
                        return false;
                }
            }
            tHit = tMin;
            return true;
        }

        // Ray (unit dir) vs sphere; t = 0 if origin is inside
        inline bool RaySphere(const Vec3& o, const Vec3& d, const Vec3& c, float r, float& t) noexcept
        {
            Vec3 m = o - c;
            float b = m.Dot(d);
            float cc = m.Dot(m) - r * r;
            if (cc > 0.0f && b > 0.0f)
                return false;
            float disc = b * b - cc;
            if (disc < 0.0f)
                return false;
            t = Max(0.0f, -b - sqrtf(disc));
            return true;
        }
    }

    inline bool Raycast(const Ray& ray, const OBB& box, float& t) noexcept
    {
        Vec3 ax[3];
        box.Axes(ax);
        Vec3 rel = ray.origin - box.center;
        Vec3 o(rel.Dot(ax[0]), rel.Dot(ax[1]), rel.Dot(ax[2]));
        Vec3 d(ray.direction.Dot(ax[0]), ray.direction.Dot(ax[1]), ray.direction.Dot(ax[2]));
        return detail::RaySlabs(o, d, -box.halfExtents, box.halfExtents, INFINITY_F, t);
    }

    inline bool Raycast(const Ray& ray, const Capsule& cap, float& t) noexcept
    {
        const Vec3& ro = ray.origin;
        const Vec3& rd = ray.direction;
        float best = INFINITY_F;
        float hit;

        // Cylindrical body
        Vec3 ba = cap.b - cap.a;
        Vec3 oa = ro - cap.a;
        float baba = ba.Dot(ba);
        float bard = ba.Dot(rd);
        float baoa = ba.Dot(oa);
        float rdoa = rd.Dot(oa);
        float oaoa = oa.Dot(oa);
        float A = baba - bard * bard;
        if (A > 1e-12f)
        {
            float B = baba * rdoa - baoa * bard;
            float C = baba * oaoa - baoa * baoa - cap.radius * cap.radius * baba;
            float h = B * B - A * C;
            if (h >= 0.0f)
            {
                float tb = (-B - sqrtf(h)) / A;
                float y = baoa + tb * bard;
                if (y > 0.0f && y < baba)
                {
                    if (tb >= 0.0f)
                        best = tb;
                    else if (C <= 0.0f)
                        best = 0.0f; // origin inside the body
                }
            }
        }

        // End caps
        if (detail::RaySphere(ro, rd, cap.a, cap.radius, hit)) best = Min(best, hit);
        if (detail::RaySphere(ro, rd, cap.b, cap.radius, hit)) best = Min(best, hit);

        if (best == INFINITY_F)
            return false;
        t = best;
        return true;
    }

    //============================================================
    // Swept tests
    // The first shape moves by `motion` over t in [0, 1]; the second is static.
    // Shapes already touching at t = 0 report t = 0.
    //============================================================

    // Moving sphere vs static sphere
    inline bool Sweep(const Sphere& moving, const Vec3& motion, const Sphere& target, SweepHit& hit) noexcept
    {
        Vec3 s = moving.center - target.center;
        float r = moving.radius + target.radius;
        float c = s.Dot(s) - r * r;

        float t;
        if (c <= 0.0f)
        {
            t = 0.0f;
        }
        else
        {
            float a = motion.Dot(motion);
            float b = motion.Dot(s);
            if (a < 1e-12f || b >= 0.0f)
                return false; // not moving, or moving apart
            float disc = b * b - a * c;
            if (disc < 0.0f)
                return false;
            t = (-b - sqrtf(disc)) / a;
            if (t > 1.0f)
                return false;
        }

        Vec3 centerAtT = moving.center + motion * t;
        Vec3 n = (centerAtT - target.center).Normalized();
        hit.t = t;
        hit.normal = n;
        hit.point = target.center + n * target.radius;
        return true;
    }

    // Moving sphere vs plane (5.5.3)
    inline bool Sweep(const Sphere& moving, const Vec3& motion, const Plane& plane, SweepHit& hit) noexcept
    {
        float dist = plane.Distance(moving.center);
        if (Abs(dist) <= moving.radius)
        {
            hit.t = 0.0f;
            hit.normal = dist >= 0.0f ? plane.normal : -plane.normal;
            hit.point = moving.center - plane.normal * dist;
            return true;
        }

        float denom = plane.normal.Dot(motion);
        if (denom * dist >= 0.0f)
            return false; // parallel or moving away

        float r = dist > 0.0f ? moving.radius : -moving.radius;
        float t = (r - dist) / denom;
        if (t > 1.0f)
            return false;

        hit.t = t;
        hit.normal = dist > 0.0f ? plane.normal : -plane.normal;
        hit.point = moving.center + motion * t - plane.normal * r;
        return true;
    }

    namespace detail
    {
        inline Vec3 AABBCorner(const AABB& b, int n) noexcept
        {
            return Vec3((n & 1) ? b.max.x : b.min.x, (n & 2) ? b.max.y : b.min.y, (n & 4) ? b.max.z : b.min.z);
        }

        // Segment p0 -> p0 + motion (t in [0, 1]) vs capsule
        inline bool SegmentCapsule(const Vec3& p0, const Vec3& motion, const Vec3& a, const Vec3& b, float r, float& t) noexcept
        {
            float len = motion.Length();
            if (len < 1e-12f)
                return false;
            Ray ray;
            ray.origin = p0;
            ray.direction = motion / len;
            float dist;
            if (!Raycast(ray, Capsule(a, b, r), dist) || dist > len)
                return false;
            t = dist / len;
            return true;
        }
    }

    // Moving sphere vs static AABB (5.5.7)
    inline bool Sweep(const Sphere& moving, const Vec3& motion, const AABB& box, SweepHit& hit) noexcept
    {
        float t;
        float r = moving.radius;

        if (box.DistanceSq(moving.center) <= r * r)
        {
            t = 0.0f;
        }
        else
        {
            // Ray vs box expanded by r
            Vec3 rv(r, r, r);
            float tSlab;
            if (!detail::RaySlabs(moving.center, motion, box.min - rv, box.max + rv, 1.0f, tSlab))
                return false;

            Vec3 p = moving.center + motion * tSlab;
            int u = 0, v = 0;
            if (p.x < box.min.x) u |= 1;
            if (p.x > box.max.x) v |= 1;
            if (p.y < box.min.y) u |= 2;
            if (p.y > box.max.y) v |= 2;
            if (p.z < box.min.z) u |= 4;
            if (p.z > box.max.z) v |= 4;
            int m = u + v;

            if (m == 7)
            {
                // Vertex region: test the three edge capsules meeting at the vertex
                float best = INFINITY_F, te;
                Vec3 corner = detail::AABBCorner(box, v);
                if (detail::SegmentCapsule(moving.center, motion, corner, detail::AABBCorner(box, v ^ 1), r, te)) best = Min(best, te);
                if (detail::SegmentCapsule(moving.center, motion, corner, detail::AABBCorner(box, v ^ 2), r, te)) best = Min(best, te);
                if (detail::SegmentCapsule(moving.center, motion, corner, detail::AABBCorner(box, v ^ 4), r, te)) best = Min(best, te);
                if (best == INFINITY_F)
                    return false;
                t = best;
            }
            else if ((m & (m - 1)) == 0)
            {
                t = tSlab; // face region
            }
            else
            {
                // Edge region: test the capsule along that edge
                if (!detail::SegmentCapsule(moving.center, motion, detail::AABBCorner(box, u ^ 7), detail::AABBCorner(box, v), r, t))
                    return false;
            }
        }

        Vec3 centerAtT = moving.center + motion * t;
        Vec3 q = box.ClosestPoint(centerAtT);
        Vec3 n = centerAtT - q;
        hit.t = t;
        hit.point = q;
        hit.normal = (n.LengthSq() > 1e-12f) ? n.Normalized() : (-motion).Normalized();
        return true;
    }

    // Moving AABB vs static AABB (5.5.8)
    inline bool Sweep(const AABB& moving, const Vec3& motion, const AABB& target, SweepHit& hit) noexcept
    {
        if (moving.Intersects(target))
        {
            hit.t = 0.0f;
            hit.point = target.ClosestPoint(moving.Center());
            hit.normal = (moving.Center() - target.Center()).Normalized();
            return true;
        }

        float tFirst = 0.0f;
        float tLast = 1.0f;
        int hitAxis = -1;

        for (int i = 0; i < 3; ++i)
        {
            float v = motion[i];
            if (v < 0.0f)
            {
                if (moving.max[i] < target.min[i]) return false; // moving away
                if (moving.min[i] > target.max[i])
                {
                    float tEnter = (target.max[i] - moving.min[i]) / v;
                    if (tEnter > tFirst) { tFirst = tEnter; hitAxis = i; }
                }
                if (moving.max[i] > target.min[i])
                    tLast = Min(tLast, (target.min[i] - moving.max[i]) / v);
            }
            else if (v > 0.0f)
            {
                if (moving.min[i] > target.max[i]) return false;
                if (moving.max[i] < target.min[i])
                {
                    float tEnter = (target.min[i] - moving.max[i]) / v;
                    if (tEnter > tFirst) { tFirst = tEnter; hitAxis = i; }
                }
                if (moving.min[i] < target.max[i])
                    tLast = Min(tLast, (target.max[i] - moving.min[i]) / v);
            }
            else if (moving.max[i] < target.min[i] || moving.min[i] > target.max[i])
            {
                return false; // separated on a static axis
            }

            if (tFirst > tLast)
                return false;
        }

        if (hitAxis < 0)
            return false;

        Vec3 n = Vec3::Zero();
        n[hitAxis] = motion[hitAxis] > 0.0f ? -1.0f : 1.0f;
        hit.t = tFirst;
        hit.normal = n;
        hit.point = target.ClosestPoint(moving.Center() + motion * tFirst);
        return true;
    }

    //============================================================
    // 2D
    //============================================================

    inline Vec2 ClosestPointOnSegment2D(const Vec2& p, const Vec2& a, const Vec2& b, float* outT = nullptr) noexcept
    {
        Vec2 ab = b - a;
        float lenSq = ab.LengthSq();
        float t = (lenSq > 1e-12f) ? Clamp((p - a).Dot(ab) / lenSq, 0.0f, 1.0f) : 0.0f;
        if (outT) *outT = t;
        return a + ab * t;
    }

    constexpr bool Intersects(const Circle& a, const Circle& b) noexcept
    {
        float r = a.radius + b.radius;
        return (a.center - b.center).LengthSq() <= r * r;
    }

    constexpr bool Intersects(const Circle& c, const Rect& r) noexcept
    {
        return (r.ClosestPoint(c.center) - c.center).LengthSq() <= c.radius * c.radius;
    }

    constexpr bool Intersects(const Rect& a, const Rect& b) noexcept
    {
        return a.Overlaps(b);
    }

    // Proper intersection of segments ab and cd. Collinear overlaps are reported as no hit.
    inline bool SegmentIntersect2D(const Vec2& a, const Vec2& b, const Vec2& c, const Vec2& d,
        Vec2* outPoint = nullptr, float* outT = nullptr) noexcept
    {
        Vec2 r = b - a;
        Vec2 s = d - c;
        float denom = r.Cross(s);
        if (Abs(denom) < 1e-12f)
            return false;
        Vec2 ac = c - a;
        float t = ac.Cross(s) / denom;
        float u = ac.Cross(r) / denom;
        if (t < 0.0f || t > 1.0f || u < 0.0f || u > 1.0f)
            return false;
        if (outPoint) *outPoint = a + r * t;
        if (outT) *outT = t;
        return true;
    }

    // Works for either winding; points on an edge count as inside
    constexpr bool PointInTriangle2D(const Vec2& p, const Vec2& a, const Vec2& b, const Vec2& c) noexcept
    {
        float d1 = (b - a).Cross(p - a);
        float d2 = (c - b).Cross(p - b);
        float d3 = (a - c).Cross(p - c);
        bool hasNeg = d1 < 0.0f || d2 < 0.0f || d3 < 0.0f;
        bool hasPos = d1 > 0.0f || d2 > 0.0f || d3 > 0.0f;
        return !(hasNeg && hasPos);
    }

    // Winding-number test; handles concave and self-intersecting polygons
    inline bool PointInPolygon(const Vec2* points, int count, const Vec2& p) noexcept
    {
        int winding = 0;
        for (int i = 0; i < count; ++i)
        {
            const Vec2& a = points[i];
            const Vec2& b = points[(i + 1) % count];
            float side = (b - a).Cross(p - a);
            if (a.y <= p.y)
            {
                if (b.y > p.y && side > 0.0f)
                    ++winding;
            }
            else if (b.y <= p.y && side < 0.0f)
            {
                --winding;
            }
        }
        return winding != 0;
    }
} // namespace xg
