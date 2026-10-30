// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.
#include "pch.h"
#include "xgGeometryInterop.h"
#include "xgInteropUtil.h"

// All pointer arguments come from managed callers and may be only 4-byte aligned:
// read them with interop::Load and write results with interop::Store (see xgInteropUtil.h).

namespace xg
{
    using interop::Load;
    using interop::Store;

    //============================================================
    // Layout
    //============================================================
    int32_t XG_MathSizeOf(int32_t typeId)
    {
        switch (static_cast<MathTypeId>(typeId))
        {
        case MathTypeId::Vec2:        return sizeof(Vec2);
        case MathTypeId::Vec3:        return sizeof(Vec3);
        case MathTypeId::Vec4:        return sizeof(Vec4);
        case MathTypeId::Quat:        return sizeof(Quat);
        case MathTypeId::Mat4x4:      return sizeof(Mat4x4);
        case MathTypeId::Mat3x3:      return sizeof(Mat3x3);
        case MathTypeId::Mat3x2:      return sizeof(Mat3x2);
        case MathTypeId::Transform2D: return sizeof(Transform2D);
        case MathTypeId::Vec2Int:     return sizeof(Vec2Int);
        case MathTypeId::Vec3Int:     return sizeof(Vec3Int);
        case MathTypeId::Color:       return sizeof(Color);
        case MathTypeId::Color32:     return sizeof(Color32);
        case MathTypeId::Rect:        return sizeof(Rect);
        case MathTypeId::RectInt:     return sizeof(RectInt);
        case MathTypeId::AABB:        return sizeof(AABB);
        case MathTypeId::Sphere:      return sizeof(Sphere);
        case MathTypeId::Ray:         return sizeof(Ray);
        case MathTypeId::Plane:       return sizeof(Plane);
        case MathTypeId::Segment:     return sizeof(Segment);
        case MathTypeId::Triangle:    return sizeof(Triangle);
        case MathTypeId::OBB:         return sizeof(OBB);
        case MathTypeId::Capsule:     return sizeof(Capsule);
        case MathTypeId::Cylinder:    return sizeof(Cylinder);
        case MathTypeId::Circle:      return sizeof(Circle);
        case MathTypeId::SweepHit:    return sizeof(SweepHit);
        case MathTypeId::Frustum:     return sizeof(Frustum);
        case MathTypeId::Pcg32:       return sizeof(Pcg32);
        default:                      return -1;
        }
    }

    //============================================================
    // Closest points
    //============================================================
    void XG_ClosestPointOnSegment(const Vec3* p, const Vec3* a, const Vec3* b, Vec3* outPoint, float* outT)
    {
        float t = 0.0f;
        Store(outPoint, ClosestPointOnSegment(Load(p), Load(a), Load(b), &t));
        interop::StoreIf(outT, t);
    }

    void XG_ClosestPointOnTriangle(const Vec3* p, const Triangle* tri, Vec3* outPoint)
    {
        Store(outPoint, ClosestPointOnTriangle(Load(p), Load(tri)));
    }

    void XG_ClosestPointOnOBB(const Vec3* p, const OBB* box, Vec3* outPoint)
    {
        Store(outPoint, ClosestPointOnOBB(Load(p), Load(box)));
    }

    void XG_ClosestPointOnCapsule(const Vec3* p, const Capsule* cap, Vec3* outPoint)
    {
        Store(outPoint, ClosestPointOnCapsule(Load(p), Load(cap)));
    }

    float XG_ClosestPointsSegmentSegment(const Segment* a, const Segment* b, float* outS, float* outT, Vec3* outPointA, Vec3* outPointB)
    {
        Segment sa = Load(a);
        Segment sb = Load(b);
        float s = 0.0f, t = 0.0f;
        Vec3 c1, c2;
        float distSq = ClosestPointsSegmentSegment(sa.a, sa.b, sb.a, sb.b, s, t, c1, c2);
        interop::StoreIf(outS, s);
        interop::StoreIf(outT, t);
        interop::StoreIf(outPointA, c1);
        interop::StoreIf(outPointB, c2);
        return distSq;
    }

    //============================================================
    // Overlap tests
    //============================================================
    bool XG_IntersectsSphereCapsule(const Sphere* s, const Capsule* cap) { return Intersects(Load(s), Load(cap)); }
    bool XG_IntersectsCapsuleCapsule(const Capsule* a, const Capsule* b) { return Intersects(Load(a), Load(b)); }
    bool XG_IntersectsSphereOBB(const Sphere* s, const OBB* box) { return Intersects(Load(s), Load(box)); }
    bool XG_IntersectsOBBOBB(const OBB* a, const OBB* b) { return Intersects(Load(a), Load(b)); }
    bool XG_IntersectsAABBOBB(const AABB* a, const OBB* b) { return Intersects(Load(a), Load(b)); }
    bool XG_IntersectsSphereTriangle(const Sphere* s, const Triangle* tri) { return Intersects(Load(s), Load(tri)); }

    //============================================================
    // Plane classification
    //============================================================
    int32_t XG_PlaneSideSphere(const Plane* plane, const Sphere* s) { return static_cast<int32_t>(Classify(Load(plane), Load(s))); }
    int32_t XG_PlaneSideAABB(const Plane* plane, const AABB* box) { return static_cast<int32_t>(Classify(Load(plane), Load(box))); }
    int32_t XG_PlaneSideOBB(const Plane* plane, const OBB* box) { return static_cast<int32_t>(Classify(Load(plane), Load(box))); }

    //============================================================
    // Ray casts
    //============================================================
    bool XG_RaycastOBB(const Ray* ray, const OBB* box, float* outT)
    {
        float t = 0.0f;
        bool hit = Raycast(Load(ray), Load(box), t);
        interop::StoreIf(outT, t);
        return hit;
    }

    bool XG_RaycastCapsule(const Ray* ray, const Capsule* cap, float* outT)
    {
        float t = 0.0f;
        bool hit = Raycast(Load(ray), Load(cap), t);
        interop::StoreIf(outT, t);
        return hit;
    }

    //============================================================
    // Swept tests
    //============================================================
    bool XG_SweepSphereSphere(const Sphere* moving, const Vec3* motion, const Sphere* target, SweepHit* outHit)
    {
        SweepHit hit;
        bool result = Sweep(Load(moving), Load(motion), Load(target), hit);
        Store(outHit, hit);
        return result;
    }

    bool XG_SweepSpherePlane(const Sphere* moving, const Vec3* motion, const Plane* plane, SweepHit* outHit)
    {
        SweepHit hit;
        bool result = Sweep(Load(moving), Load(motion), Load(plane), hit);
        Store(outHit, hit);
        return result;
    }

    bool XG_SweepSphereAABB(const Sphere* moving, const Vec3* motion, const AABB* box, SweepHit* outHit)
    {
        SweepHit hit;
        bool result = Sweep(Load(moving), Load(motion), Load(box), hit);
        Store(outHit, hit);
        return result;
    }

    bool XG_SweepAABBAABB(const AABB* moving, const Vec3* motion, const AABB* target, SweepHit* outHit)
    {
        SweepHit hit;
        bool result = Sweep(Load(moving), Load(motion), Load(target), hit);
        Store(outHit, hit);
        return result;
    }

    //============================================================
    // 2D (Vec2 is 4-byte aligned, but go through Load/Store for consistency)
    //============================================================
    bool XG_SegmentIntersect2D(const Vec2* a, const Vec2* b, const Vec2* c, const Vec2* d, Vec2* outPoint, float* outT)
    {
        Vec2 point;
        float t = 0.0f;
        bool hit = SegmentIntersect2D(Load(a), Load(b), Load(c), Load(d), &point, &t);
        interop::StoreIf(outPoint, point);
        interop::StoreIf(outT, t);
        return hit;
    }

    bool XG_PointInPolygon(const Vec2* points, int32_t count, const Vec2* p)
    {
        if (!points || count < 3)
            return false;
        return PointInPolygon(points, count, Load(p));
    }

    //============================================================
    // OBB helpers
    //============================================================
    void XG_OBBFromAABB(const AABB* box, const Mat4x4* world, OBB* outBox)
    {
        Store(outBox, OBB::FromAABB(Load(box), Load(world)));
    }

    void XG_OBBToAABB(const OBB* box, AABB* outBox)
    {
        Store(outBox, Load(box).ToAABB());
    }

    //============================================================
    // Frustum
    //============================================================
    void XG_FrustumFromMatrix(const Mat4x4* viewProj, int32_t depthRange, Frustum* outFrustum)
    {
        Store(outFrustum, Frustum::FromMatrix(Load(viewProj),
            depthRange == 1 ? Frustum::DepthRange::MinusOneToOne : Frustum::DepthRange::ZeroToOne));
    }

    int32_t XG_FrustumContainsAABB(const Frustum* frustum, const AABB* box)
    {
        return static_cast<int32_t>(Load(frustum).Contains(Load(box)));
    }

    int32_t XG_FrustumContainsSphere(const Frustum* frustum, const Sphere* s)
    {
        return static_cast<int32_t>(Load(frustum).Contains(Load(s)));
    }

    int32_t XG_FrustumContainsOBB(const Frustum* frustum, const OBB* box)
    {
        return static_cast<int32_t>(Load(frustum).Contains(Load(box)));
    }

    int32_t XG_FrustumCullSpheres(const Frustum* frustum, const Sphere* spheres, int32_t count, uint8_t* outVisible)
    {
        Frustum f = Load(frustum);
        int32_t visible = 0;
        for (int32_t i = 0; i < count; ++i)
        {
            bool inside = f.Intersects(Load(spheres + i));
            outVisible[i] = inside ? 1 : 0;
            visible += inside ? 1 : 0;
        }
        return visible;
    }
}
