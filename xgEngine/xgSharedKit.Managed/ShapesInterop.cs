// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.

using System.Runtime.InteropServices;

// Blittable mirrors of the native shapes (xgBounds.h, xgRay.h, xgShapes.h).
// Vector3 is 16 bytes (matches C++ alignas(16)), so a float after a Vector3 sits at offset 16
// and the native structs are padded to a multiple of 16. Explicit Size values below match the
// C++ static_asserts; MathLayout.Validate() checks them against XG_MathSizeOf at startup.
// Queries on these shapes are in GeometryInterop.cs.

namespace xg.math
{
    [StructLayout(LayoutKind.Sequential)]
    public struct Vector4
    {
        public float x;
        public float y;
        public float z;
        public float w;

        public Vector4(float x, float y, float z, float w)
        {
            this.x = x;
            this.y = y;
            this.z = z;
            this.w = w;
        }

        public override string ToString() => $"Vector4({x:F4}, {y:F4}, {z:F4}, {w:F4})";
    }

    public enum PlaneSide : int
    {
        Back = 0,
        Front = 1,
        Intersecting = 2,
    }

    public enum ContainmentType : int
    {
        Disjoint = 0,
        Contains = 1,
        Intersects = 2,
    }

    public enum DepthRange : int
    {
        ZeroToOne = 0,     // D3D / DirectXMath / Vulkan
        MinusOneToOne = 1, // OpenGL
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct AABB
    {
        public Vector3 min;
        public Vector3 max;

        public AABB(Vector3 min, Vector3 max)
        {
            this.min = min;
            this.max = max;
        }

        public static AABB FromCenterExtents(Vector3 center, Vector3 extents)
            => new(VecMath.Sub(center, extents), VecMath.Add(center, extents));

        public Vector3 Center => VecMath.Scale(VecMath.Add(min, max), 0.5f);
        public Vector3 Extents => VecMath.Scale(VecMath.Sub(max, min), 0.5f);
    }

    [StructLayout(LayoutKind.Sequential, Size = 32)]
    public struct Sphere
    {
        public Vector3 center;
        public float radius;

        public Sphere(Vector3 center, float radius)
        {
            this.center = center;
            this.radius = radius;
        }
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct Ray
    {
        public Vector3 origin;
        public Vector3 direction; // unit length

        // Normalizes direction (same as the C++ constructor)
        public Ray(Vector3 origin, Vector3 direction)
        {
            this.origin = origin;
            this.direction = VecMath.Normalize(direction);
        }

        public Vector3 GetPoint(float t) => VecMath.Add(origin, VecMath.Scale(direction, t));
    }

    // dot(normal, p) + distance = 0, normal is unit length
    [StructLayout(LayoutKind.Sequential, Size = 32)]
    public struct Plane
    {
        public Vector3 normal;
        public float distance;

        // Normalizes (normal, distance) together, like the C++ constructor
        public Plane(Vector3 normal, float distance)
        {
            float len = VecMath.Length(normal);
            float inv = len > VecMath.EpsilonPos ? 1.0f / len : 0.0f;
            this.normal = VecMath.Scale(normal, inv);
            this.distance = distance * inv;
        }

        public static Plane FromPointNormal(Vector3 point, Vector3 normal)
        {
            Vector3 n = VecMath.Normalize(normal);
            return new Plane { normal = n, distance = -VecMath.Dot(n, point) };
        }

        public static Plane FromPoints(Vector3 a, Vector3 b, Vector3 c)
            => FromPointNormal(a, VecMath.Cross(VecMath.Sub(b, a), VecMath.Sub(c, a)));

        public float Distance(Vector3 p) => VecMath.Dot(normal, p) + distance;
        public Vector3 Project(Vector3 p) => VecMath.Sub(p, VecMath.Scale(normal, Distance(p)));
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct Segment
    {
        public Vector3 a;
        public Vector3 b;

        public Segment(Vector3 a, Vector3 b)
        {
            this.a = a;
            this.b = b;
        }
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct Triangle
    {
        public Vector3 a;
        public Vector3 b;
        public Vector3 c;

        public Triangle(Vector3 a, Vector3 b, Vector3 c)
        {
            this.a = a;
            this.b = b;
            this.c = c;
        }

        public Vector3 Normal => VecMath.Normalize(VecMath.Cross(VecMath.Sub(b, a), VecMath.Sub(c, a)));
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct OBB
    {
        public Vector3 center;
        public Vector3 halfExtents;
        public Quaternion orientation;

        public OBB(Vector3 center, Vector3 halfExtents, Quaternion orientation)
        {
            this.center = center;
            this.halfExtents = halfExtents;
            this.orientation = orientation;
        }

        // Axis-aligned (identity orientation)
        public OBB(Vector3 center, Vector3 halfExtents)
            : this(center, halfExtents, new Quaternion(0f, 0f, 0f, 1f)) { }
    }

    [StructLayout(LayoutKind.Sequential, Size = 48)]
    public struct Capsule
    {
        public Vector3 a;
        public Vector3 b;
        public float radius;

        public Capsule(Vector3 a, Vector3 b, float radius)
        {
            this.a = a;
            this.b = b;
            this.radius = radius;
        }
    }

    [StructLayout(LayoutKind.Sequential, Size = 48)]
    public struct Cylinder
    {
        public Vector3 a;
        public Vector3 b;
        public float radius;

        public Cylinder(Vector3 a, Vector3 b, float radius)
        {
            this.a = a;
            this.b = b;
            this.radius = radius;
        }
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct Circle
    {
        public Vector2 center;
        public float radius;

        public Circle(Vector2 center, float radius)
        {
            this.center = center;
            this.radius = radius;
        }
    }

    // Result of a swept (moving) shape test
    [StructLayout(LayoutKind.Sequential, Size = 48)]
    public struct SweepHit
    {
        public Vector3 point;  // contact point at time of impact
        public Vector3 normal; // points away from the static shape
        public float t;        // time of impact in [0, 1] along the motion
    }

    // Six planes (x, y, z = normal, w = distance), same order as xg::Frustum::PlaneIndex
    [StructLayout(LayoutKind.Sequential)]
    public struct Frustum
    {
        public Vector4 near;
        public Vector4 far;
        public Vector4 left;
        public Vector4 right;
        public Vector4 top;
        public Vector4 bottom;
    }
}
