// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.

using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;

// Geometry queries implemented natively in xgScriptNative (xgBase/math/xgGeometry.h),
// exported as XG_<Op> (xgBase/math/xgGeometryInterop.h).

namespace xg.math
{
    internal static unsafe partial class NativeGeometry
    {
        private const string DLL = "xgScriptNative";

        // Layout
        [LibraryImport(DLL, EntryPoint = "XG_MathSizeOf")]
        [SuppressGCTransition]
        public static partial int MathSizeOf(int typeId);

        // Closest points
        [LibraryImport(DLL, EntryPoint = "XG_ClosestPointOnSegment")]
        [SuppressGCTransition]
        public static partial void ClosestPointOnSegment(in Vector3 p, in Vector3 a, in Vector3 b, out Vector3 point, out float t);

        [LibraryImport(DLL, EntryPoint = "XG_ClosestPointOnTriangle")]
        [SuppressGCTransition]
        public static partial void ClosestPointOnTriangle(in Vector3 p, in Triangle tri, out Vector3 point);

        [LibraryImport(DLL, EntryPoint = "XG_ClosestPointOnOBB")]
        [SuppressGCTransition]
        public static partial void ClosestPointOnOBB(in Vector3 p, in OBB box, out Vector3 point);

        [LibraryImport(DLL, EntryPoint = "XG_ClosestPointOnCapsule")]
        [SuppressGCTransition]
        public static partial void ClosestPointOnCapsule(in Vector3 p, in Capsule cap, out Vector3 point);

        [LibraryImport(DLL, EntryPoint = "XG_ClosestPointsSegmentSegment")]
        [SuppressGCTransition]
        public static partial float ClosestPointsSegmentSegment(in Segment a, in Segment b, out float s, out float t, out Vector3 pointA, out Vector3 pointB);

        // Overlap tests
        [LibraryImport(DLL, EntryPoint = "XG_IntersectsSphereCapsule")]
        [SuppressGCTransition]
        [return: MarshalAs(UnmanagedType.U1)]
        public static partial bool IntersectsSphereCapsule(in Sphere s, in Capsule cap);

        [LibraryImport(DLL, EntryPoint = "XG_IntersectsCapsuleCapsule")]
        [SuppressGCTransition]
        [return: MarshalAs(UnmanagedType.U1)]
        public static partial bool IntersectsCapsuleCapsule(in Capsule a, in Capsule b);

        [LibraryImport(DLL, EntryPoint = "XG_IntersectsSphereOBB")]
        [SuppressGCTransition]
        [return: MarshalAs(UnmanagedType.U1)]
        public static partial bool IntersectsSphereOBB(in Sphere s, in OBB box);

        [LibraryImport(DLL, EntryPoint = "XG_IntersectsOBBOBB")]
        [SuppressGCTransition]
        [return: MarshalAs(UnmanagedType.U1)]
        public static partial bool IntersectsOBBOBB(in OBB a, in OBB b);

        [LibraryImport(DLL, EntryPoint = "XG_IntersectsAABBOBB")]
        [SuppressGCTransition]
        [return: MarshalAs(UnmanagedType.U1)]
        public static partial bool IntersectsAABBOBB(in AABB a, in OBB b);

        [LibraryImport(DLL, EntryPoint = "XG_IntersectsSphereTriangle")]
        [SuppressGCTransition]
        [return: MarshalAs(UnmanagedType.U1)]
        public static partial bool IntersectsSphereTriangle(in Sphere s, in Triangle tri);

        // Plane classification
        [LibraryImport(DLL, EntryPoint = "XG_PlaneSideSphere")]
        [SuppressGCTransition]
        public static partial int PlaneSideSphere(in Plane plane, in Sphere s);

        [LibraryImport(DLL, EntryPoint = "XG_PlaneSideAABB")]
        [SuppressGCTransition]
        public static partial int PlaneSideAABB(in Plane plane, in AABB box);

        [LibraryImport(DLL, EntryPoint = "XG_PlaneSideOBB")]
        [SuppressGCTransition]
        public static partial int PlaneSideOBB(in Plane plane, in OBB box);

        // Ray casts
        [LibraryImport(DLL, EntryPoint = "XG_RaycastOBB")]
        [SuppressGCTransition]
        [return: MarshalAs(UnmanagedType.U1)]
        public static partial bool RaycastOBB(in Ray ray, in OBB box, out float t);

        [LibraryImport(DLL, EntryPoint = "XG_RaycastCapsule")]
        [SuppressGCTransition]
        [return: MarshalAs(UnmanagedType.U1)]
        public static partial bool RaycastCapsule(in Ray ray, in Capsule cap, out float t);

        // Swept tests
        [LibraryImport(DLL, EntryPoint = "XG_SweepSphereSphere")]
        [SuppressGCTransition]
        [return: MarshalAs(UnmanagedType.U1)]
        public static partial bool SweepSphereSphere(in Sphere moving, in Vector3 motion, in Sphere target, out SweepHit hit);

        [LibraryImport(DLL, EntryPoint = "XG_SweepSpherePlane")]
        [SuppressGCTransition]
        [return: MarshalAs(UnmanagedType.U1)]
        public static partial bool SweepSpherePlane(in Sphere moving, in Vector3 motion, in Plane plane, out SweepHit hit);

        [LibraryImport(DLL, EntryPoint = "XG_SweepSphereAABB")]
        [SuppressGCTransition]
        [return: MarshalAs(UnmanagedType.U1)]
        public static partial bool SweepSphereAABB(in Sphere moving, in Vector3 motion, in AABB box, out SweepHit hit);

        [LibraryImport(DLL, EntryPoint = "XG_SweepAABBAABB")]
        [SuppressGCTransition]
        [return: MarshalAs(UnmanagedType.U1)]
        public static partial bool SweepAABBAABB(in AABB moving, in Vector3 motion, in AABB target, out SweepHit hit);

        // 2D
        [LibraryImport(DLL, EntryPoint = "XG_SegmentIntersect2D")]
        [SuppressGCTransition]
        [return: MarshalAs(UnmanagedType.U1)]
        public static partial bool SegmentIntersect2D(in Vector2 a, in Vector2 b, in Vector2 c, in Vector2 d, out Vector2 point, out float t);

        [LibraryImport(DLL, EntryPoint = "XG_PointInPolygon")]
        [SuppressGCTransition]
        [return: MarshalAs(UnmanagedType.U1)]
        public static partial bool PointInPolygon(Vector2* points, int count, in Vector2 p);

        // OBB helpers
        [LibraryImport(DLL, EntryPoint = "XG_OBBFromAABB")]
        [SuppressGCTransition]
        public static partial void OBBFromAABB(in AABB box, in Matrix4x4 world, out OBB result);

        [LibraryImport(DLL, EntryPoint = "XG_OBBToAABB")]
        [SuppressGCTransition]
        public static partial void OBBToAABB(in OBB box, out AABB result);

        // Frustum
        [LibraryImport(DLL, EntryPoint = "XG_FrustumFromMatrix")]
        [SuppressGCTransition]
        public static partial void FrustumFromMatrix(in Matrix4x4 viewProj, int depthRange, out Frustum frustum);

        [LibraryImport(DLL, EntryPoint = "XG_FrustumContainsAABB")]
        [SuppressGCTransition]
        public static partial int FrustumContainsAABB(in Frustum frustum, in AABB box);

        [LibraryImport(DLL, EntryPoint = "XG_FrustumContainsSphere")]
        [SuppressGCTransition]
        public static partial int FrustumContainsSphere(in Frustum frustum, in Sphere s);

        [LibraryImport(DLL, EntryPoint = "XG_FrustumContainsOBB")]
        [SuppressGCTransition]
        public static partial int FrustumContainsOBB(in Frustum frustum, in OBB box);

        // Batch: no SuppressGCTransition, large counts would hold off the GC
        [LibraryImport(DLL, EntryPoint = "XG_FrustumCullSpheres")]
        public static partial int FrustumCullSpheres(in Frustum frustum, Sphere* spheres, int count, byte* outVisible);
    }

    // ------------------------------------------------------------
    // Public API
    // ------------------------------------------------------------
    public static class Geometry
    {
        // Closest points
        public static Vector3 ClosestPointOnSegment(Vector3 p, Vector3 a, Vector3 b)
        {
            NativeGeometry.ClosestPointOnSegment(p, a, b, out Vector3 point, out _);
            return point;
        }

        public static Vector3 ClosestPointOnSegment(Vector3 p, Vector3 a, Vector3 b, out float t)
        {
            NativeGeometry.ClosestPointOnSegment(p, a, b, out Vector3 point, out t);
            return point;
        }

        public static Vector3 ClosestPointOnTriangle(Vector3 p, in Triangle tri)
        {
            NativeGeometry.ClosestPointOnTriangle(p, tri, out Vector3 point);
            return point;
        }

        public static Vector3 ClosestPointOnOBB(Vector3 p, in OBB box)
        {
            NativeGeometry.ClosestPointOnOBB(p, box, out Vector3 point);
            return point;
        }

        // Returns p itself if p is inside the capsule
        public static Vector3 ClosestPointOnCapsule(Vector3 p, in Capsule cap)
        {
            NativeGeometry.ClosestPointOnCapsule(p, cap, out Vector3 point);
            return point;
        }

        // Returns the squared distance between the closest points
        public static float ClosestPointsSegmentSegment(in Segment a, in Segment b,
            out float s, out float t, out Vector3 pointA, out Vector3 pointB)
            => NativeGeometry.ClosestPointsSegmentSegment(a, b, out s, out t, out pointA, out pointB);

        // Overlap tests
        public static bool Intersects(in Sphere s, in Capsule cap) => NativeGeometry.IntersectsSphereCapsule(s, cap);
        public static bool Intersects(in Capsule a, in Capsule b) => NativeGeometry.IntersectsCapsuleCapsule(a, b);
        public static bool Intersects(in Sphere s, in OBB box) => NativeGeometry.IntersectsSphereOBB(s, box);
        public static bool Intersects(in OBB a, in OBB b) => NativeGeometry.IntersectsOBBOBB(a, b);
        public static bool Intersects(in AABB a, in OBB b) => NativeGeometry.IntersectsAABBOBB(a, b);
        public static bool Intersects(in Sphere s, in Triangle tri) => NativeGeometry.IntersectsSphereTriangle(s, tri);

        // 2D overlap tests (cheap, managed)
        public static bool Intersects(in Circle a, in Circle b)
        {
            float r = a.radius + b.radius;
            Vector2 d = VecMath.Sub(a.center, b.center);
            return VecMath.Dot(d, d) <= r * r;
        }

        public static bool Intersects(in Circle c, in Rect r)
        {
            Vector2 d = VecMath.Sub(r.ClosestPoint(c.center), c.center);
            return VecMath.Dot(d, d) <= c.radius * c.radius;
        }

        // Plane classification
        public static PlaneSide Classify(in Plane plane, in Sphere s) => (PlaneSide)NativeGeometry.PlaneSideSphere(plane, s);
        public static PlaneSide Classify(in Plane plane, in AABB box) => (PlaneSide)NativeGeometry.PlaneSideAABB(plane, box);
        public static PlaneSide Classify(in Plane plane, in OBB box) => (PlaneSide)NativeGeometry.PlaneSideOBB(plane, box);

        // Ray casts (t = distance along the unit ray direction; t = 0 if the ray starts inside)
        public static bool Raycast(in Ray ray, in OBB box, out float t) => NativeGeometry.RaycastOBB(ray, box, out t);
        public static bool Raycast(in Ray ray, in Capsule cap, out float t) => NativeGeometry.RaycastCapsule(ray, cap, out t);

        // Swept tests: the first shape moves by motion over t in [0, 1]
        public static bool Sweep(in Sphere moving, Vector3 motion, in Sphere target, out SweepHit hit)
            => NativeGeometry.SweepSphereSphere(moving, motion, target, out hit);
        public static bool Sweep(in Sphere moving, Vector3 motion, in Plane plane, out SweepHit hit)
            => NativeGeometry.SweepSpherePlane(moving, motion, plane, out hit);
        public static bool Sweep(in Sphere moving, Vector3 motion, in AABB box, out SweepHit hit)
            => NativeGeometry.SweepSphereAABB(moving, motion, box, out hit);
        public static bool Sweep(in AABB moving, Vector3 motion, in AABB target, out SweepHit hit)
            => NativeGeometry.SweepAABBAABB(moving, motion, target, out hit);

        // 2D
        public static bool SegmentIntersect2D(Vector2 a, Vector2 b, Vector2 c, Vector2 d, out Vector2 point, out float t)
            => NativeGeometry.SegmentIntersect2D(a, b, c, d, out point, out t);

        public static unsafe bool PointInPolygon(ReadOnlySpan<Vector2> points, Vector2 p)
        {
            fixed (Vector2* ptr = points)
                return NativeGeometry.PointInPolygon(ptr, points.Length, p);
        }

        // OBB helpers
        public static OBB OBBFromAABB(in AABB box, in Matrix4x4 world)
        {
            NativeGeometry.OBBFromAABB(box, world, out OBB result);
            return result;
        }

        public static AABB ToAABB(in OBB box)
        {
            NativeGeometry.OBBToAABB(box, out AABB result);
            return result;
        }

        // Frustum
        public static Frustum FrustumFromMatrix(in Matrix4x4 viewProj, DepthRange depthRange = DepthRange.ZeroToOne)
        {
            NativeGeometry.FrustumFromMatrix(viewProj, (int)depthRange, out Frustum f);
            return f;
        }

        public static ContainmentType Contains(in Frustum frustum, in AABB box) => (ContainmentType)NativeGeometry.FrustumContainsAABB(frustum, box);
        public static ContainmentType Contains(in Frustum frustum, in Sphere s) => (ContainmentType)NativeGeometry.FrustumContainsSphere(frustum, s);
        public static ContainmentType Contains(in Frustum frustum, in OBB box) => (ContainmentType)NativeGeometry.FrustumContainsOBB(frustum, box);

        // visible[i] = 1 if spheres[i] is at least partially inside; returns the visible count
        public static unsafe int CullSpheres(in Frustum frustum, ReadOnlySpan<Sphere> spheres, Span<byte> visible)
        {
            if (visible.Length < spheres.Length)
                throw new ArgumentException("visible must be at least as long as spheres", nameof(visible));
            fixed (Sphere* s = spheres)
            fixed (byte* v = visible)
                return NativeGeometry.FrustumCullSpheres(frustum, s, spheres.Length, v);
        }
    }

    // ------------------------------------------------------------
    // Shape extension methods (forward to Geometry)
    // ------------------------------------------------------------
    public static class ShapeExtensions
    {
        public static bool Intersects(this Sphere s, Capsule cap) => Geometry.Intersects(s, cap);
        public static bool Intersects(this Sphere s, OBB box) => Geometry.Intersects(s, box);
        public static bool Intersects(this Sphere s, Triangle tri) => Geometry.Intersects(s, tri);
        public static bool Intersects(this Capsule a, Capsule b) => Geometry.Intersects(a, b);
        public static bool Intersects(this OBB a, OBB b) => Geometry.Intersects(a, b);
        public static bool Intersects(this AABB a, OBB b) => Geometry.Intersects(a, b);
        public static bool Intersects(this Circle a, Circle b) => Geometry.Intersects(a, b);
        public static bool Intersects(this Circle c, Rect r) => Geometry.Intersects(c, r);

        public static bool Raycast(this Ray ray, OBB box, out float t) => Geometry.Raycast(ray, box, out t);
        public static bool Raycast(this Ray ray, Capsule cap, out float t) => Geometry.Raycast(ray, cap, out t);

        public static PlaneSide Classify(this Plane plane, Sphere s) => Geometry.Classify(plane, s);
        public static PlaneSide Classify(this Plane plane, AABB box) => Geometry.Classify(plane, box);
        public static PlaneSide Classify(this Plane plane, OBB box) => Geometry.Classify(plane, box);

        public static ContainmentType Contains(this Frustum frustum, AABB box) => Geometry.Contains(frustum, box);
        public static ContainmentType Contains(this Frustum frustum, Sphere s) => Geometry.Contains(frustum, s);
        public static ContainmentType Contains(this Frustum frustum, OBB box) => Geometry.Contains(frustum, box);

        public static AABB ToAABB(this OBB box) => Geometry.ToAABB(box);
    }

    // ------------------------------------------------------------
    // Layout validation against the native library
    // ------------------------------------------------------------

    // Keep in sync with xg::MathTypeId in xgGeometryInterop.h
    public enum MathTypeId : int
    {
        Vec2 = 0, Vec3, Vec4, Quat, Mat4x4, Mat3x3, Mat3x2, Transform2D,
        Vec2Int, Vec3Int, Color, Color32, Rect, RectInt,
        AABB, Sphere, Ray, Plane, Segment, Triangle, OBB, Capsule, Cylinder, Circle,
        SweepHit, Frustum, Pcg32,
    }

    public static class MathLayout
    {
        public static (MathTypeId Id, string Name, int ManagedSize)[] ManagedSizes() =>
        [
            (MathTypeId.Vec2, nameof(Vector2), Unsafe.SizeOf<Vector2>()),
            (MathTypeId.Vec3, nameof(Vector3), Unsafe.SizeOf<Vector3>()),
            (MathTypeId.Vec4, nameof(Vector4), Unsafe.SizeOf<Vector4>()),
            (MathTypeId.Quat, nameof(Quaternion), Unsafe.SizeOf<Quaternion>()),
            (MathTypeId.Mat4x4, nameof(Matrix4x4), Unsafe.SizeOf<Matrix4x4>()),
            (MathTypeId.Mat3x3, nameof(Matrix3x3), Unsafe.SizeOf<Matrix3x3>()),
            (MathTypeId.Mat3x2, nameof(Matrix3x2), Unsafe.SizeOf<Matrix3x2>()),
            (MathTypeId.Transform2D, nameof(Transform2D), Unsafe.SizeOf<Transform2D>()),
            (MathTypeId.Vec2Int, nameof(Vector2Int), Unsafe.SizeOf<Vector2Int>()),
            (MathTypeId.Vec3Int, nameof(Vector3Int), Unsafe.SizeOf<Vector3Int>()),
            (MathTypeId.Color, nameof(Color), Unsafe.SizeOf<Color>()),
            (MathTypeId.Color32, nameof(Color32), Unsafe.SizeOf<Color32>()),
            (MathTypeId.Rect, nameof(Rect), Unsafe.SizeOf<Rect>()),
            (MathTypeId.RectInt, nameof(RectInt), Unsafe.SizeOf<RectInt>()),
            (MathTypeId.AABB, nameof(AABB), Unsafe.SizeOf<AABB>()),
            (MathTypeId.Sphere, nameof(Sphere), Unsafe.SizeOf<Sphere>()),
            (MathTypeId.Ray, nameof(Ray), Unsafe.SizeOf<Ray>()),
            (MathTypeId.Plane, nameof(Plane), Unsafe.SizeOf<Plane>()),
            (MathTypeId.Segment, nameof(Segment), Unsafe.SizeOf<Segment>()),
            (MathTypeId.Triangle, nameof(Triangle), Unsafe.SizeOf<Triangle>()),
            (MathTypeId.OBB, nameof(OBB), Unsafe.SizeOf<OBB>()),
            (MathTypeId.Capsule, nameof(Capsule), Unsafe.SizeOf<Capsule>()),
            (MathTypeId.Cylinder, nameof(Cylinder), Unsafe.SizeOf<Cylinder>()),
            (MathTypeId.Circle, nameof(Circle), Unsafe.SizeOf<Circle>()),
            (MathTypeId.SweepHit, nameof(SweepHit), Unsafe.SizeOf<SweepHit>()),
            (MathTypeId.Frustum, nameof(Frustum), Unsafe.SizeOf<Frustum>()),
            (MathTypeId.Pcg32, nameof(Pcg32), Unsafe.SizeOf<Pcg32>()),
        ];

        // Throws InvalidOperationException listing every type whose managed size differs from native
        public static void Validate()
        {
            var errors = new List<string>();
            foreach (var (id, name, managed) in ManagedSizes())
            {
                int native = NativeGeometry.MathSizeOf((int)id);
                if (native != managed)
                    errors.Add($"{name}: managed {managed} bytes, native {native} bytes");
            }
            if (errors.Count > 0)
                throw new InvalidOperationException("Math interop layout mismatch: " + string.Join("; ", errors));
        }
    }
}
