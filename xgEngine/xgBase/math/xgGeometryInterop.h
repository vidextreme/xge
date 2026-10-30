// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.

#pragma once

// xgGeometryInterop.h - C ABI exports for geometry queries (implemented in xgScriptNative)
// Consumed by xgSharedKit.Managed/GeometryInterop.cs via LibraryImport.
// All arguments are passed by pointer; results are written through out pointers.
// bool is 1 byte (C# side marshals it as UnmanagedType.U1).

#include <cstdint>
#include "xgBase.h"
#include "xgMath.h"

namespace xg
{
    // Type ids for XG_MathSizeOf (keep in sync with MathTypeId in GeometryInterop.cs)
    enum class MathTypeId : int32_t
    {
        Vec2 = 0, Vec3, Vec4, Quat, Mat4x4, Mat3x3, Mat3x2, Transform2D,
        Vec2Int, Vec3Int, Color, Color32, Rect, RectInt,
        AABB, Sphere, Ray, Plane, Segment, Triangle, OBB, Capsule, Cylinder, Circle,
        SweepHit, Frustum, Pcg32,
        Count
    };

    // Layout validation: native sizeof for a MathTypeId, or -1 if unknown
    XG_MODULE_EXPORT int32_t XG_MathSizeOf(int32_t typeId);

    // Closest points
    XG_MODULE_EXPORT void  XG_ClosestPointOnSegment(const Vec3* p, const Vec3* a, const Vec3* b, Vec3* outPoint, float* outT);
    XG_MODULE_EXPORT void  XG_ClosestPointOnTriangle(const Vec3* p, const Triangle* tri, Vec3* outPoint);
    XG_MODULE_EXPORT void  XG_ClosestPointOnOBB(const Vec3* p, const OBB* box, Vec3* outPoint);
    XG_MODULE_EXPORT void  XG_ClosestPointOnCapsule(const Vec3* p, const Capsule* cap, Vec3* outPoint);
    XG_MODULE_EXPORT float XG_ClosestPointsSegmentSegment(const Segment* a, const Segment* b, float* outS, float* outT, Vec3* outPointA, Vec3* outPointB);

    // Overlap tests
    XG_MODULE_EXPORT bool XG_IntersectsSphereCapsule(const Sphere* s, const Capsule* cap);
    XG_MODULE_EXPORT bool XG_IntersectsCapsuleCapsule(const Capsule* a, const Capsule* b);
    XG_MODULE_EXPORT bool XG_IntersectsSphereOBB(const Sphere* s, const OBB* box);
    XG_MODULE_EXPORT bool XG_IntersectsOBBOBB(const OBB* a, const OBB* b);
    XG_MODULE_EXPORT bool XG_IntersectsAABBOBB(const AABB* a, const OBB* b);
    XG_MODULE_EXPORT bool XG_IntersectsSphereTriangle(const Sphere* s, const Triangle* tri);

    // Plane classification (returns PlaneSide: 0 = Back, 1 = Front, 2 = Intersecting)
    XG_MODULE_EXPORT int32_t XG_PlaneSideSphere(const Plane* plane, const Sphere* s);
    XG_MODULE_EXPORT int32_t XG_PlaneSideAABB(const Plane* plane, const AABB* box);
    XG_MODULE_EXPORT int32_t XG_PlaneSideOBB(const Plane* plane, const OBB* box);

    // Ray casts (t = distance along the unit ray direction)
    XG_MODULE_EXPORT bool XG_RaycastOBB(const Ray* ray, const OBB* box, float* outT);
    XG_MODULE_EXPORT bool XG_RaycastCapsule(const Ray* ray, const Capsule* cap, float* outT);

    // Swept tests (first shape moves by motion over t in [0, 1])
    XG_MODULE_EXPORT bool XG_SweepSphereSphere(const Sphere* moving, const Vec3* motion, const Sphere* target, SweepHit* outHit);
    XG_MODULE_EXPORT bool XG_SweepSpherePlane(const Sphere* moving, const Vec3* motion, const Plane* plane, SweepHit* outHit);
    XG_MODULE_EXPORT bool XG_SweepSphereAABB(const Sphere* moving, const Vec3* motion, const AABB* box, SweepHit* outHit);
    XG_MODULE_EXPORT bool XG_SweepAABBAABB(const AABB* moving, const Vec3* motion, const AABB* target, SweepHit* outHit);

    // 2D
    XG_MODULE_EXPORT bool XG_SegmentIntersect2D(const Vec2* a, const Vec2* b, const Vec2* c, const Vec2* d, Vec2* outPoint, float* outT);
    XG_MODULE_EXPORT bool XG_PointInPolygon(const Vec2* points, int32_t count, const Vec2* p);

    // OBB helpers
    XG_MODULE_EXPORT void XG_OBBFromAABB(const AABB* box, const Mat4x4* world, OBB* outBox);
    XG_MODULE_EXPORT void XG_OBBToAABB(const OBB* box, AABB* outBox);

    // Frustum (depthRange: 0 = ZeroToOne (D3D), 1 = MinusOneToOne (OpenGL))
    // Contains* return Frustum::ContainmentType: 0 = Disjoint, 1 = Contains, 2 = Intersects
    XG_MODULE_EXPORT void    XG_FrustumFromMatrix(const Mat4x4* viewProj, int32_t depthRange, Frustum* outFrustum);
    XG_MODULE_EXPORT int32_t XG_FrustumContainsAABB(const Frustum* frustum, const AABB* box);
    XG_MODULE_EXPORT int32_t XG_FrustumContainsSphere(const Frustum* frustum, const Sphere* s);
    XG_MODULE_EXPORT int32_t XG_FrustumContainsOBB(const Frustum* frustum, const OBB* box);
    // Batch cull: outVisible[i] = 1 if spheres[i] is at least partially inside; returns the visible count
    XG_MODULE_EXPORT int32_t XG_FrustumCullSpheres(const Frustum* frustum, const Sphere* spheres, int32_t count, uint8_t* outVisible);
}
