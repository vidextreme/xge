// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.

// xgMathVerify.Managed - managed math tests.
// - Golden values are the same as verify/xgMathVerify/main.cpp (GOLDEN): managed mirrors must match native.
// - P/Invoke smoke tests for the XG_<Op> geometry exports and the existing Vec3/Quat interop.
// xgScriptNative.dll is loaded from gameroot/bin (found by walking up from the exe, or XG_BIN_DIR).

using System.Reflection;
using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;
using xg;
using xg.math;

internal static class Program
{
    private static int s_checks;
    private static int s_failures;

    private static void Check(bool condition, string what, [CallerLineNumber] int line = 0)
    {
        s_checks++;
        if (!condition)
        {
            s_failures++;
            Console.WriteLine($"FAIL line {line}: {what}");
        }
    }

    private static void Near(double actual, double expected, double eps, [CallerArgumentExpression(nameof(actual))] string what = "", [CallerLineNumber] int line = 0)
    {
        s_checks++;
        if (!(Math.Abs(actual - expected) <= eps))
        {
            s_failures++;
            Console.WriteLine($"FAIL line {line}: {what} = {actual:G9}, expected {expected:G9} (eps {eps})");
        }
    }

    private static void NearV(Vector3 a, Vector3 b, float eps, [CallerLineNumber] int line = 0)
    {
        s_checks++;
        if (MathF.Abs(a.x - b.x) > eps || MathF.Abs(a.y - b.y) > eps || MathF.Abs(a.z - b.z) > eps)
        {
            s_failures++;
            Console.WriteLine($"FAIL line {line}: {a} != {b}");
        }
    }

    private static Vector3 V(float x, float y, float z) => new(x, y, z);

    // Prints the section first so a native crash still shows where it happened
    private static void Run(string name, Action test)
    {
        Console.WriteLine($"== {name}");
        Console.Out.Flush();
        test();
    }

    private static int Main()
    {
        NativeLibrary.SetDllImportResolver(typeof(Geometry).Assembly, ResolveNative);

        Run(nameof(TestLayout), TestLayout);
        Run(nameof(TestGolden), TestGolden);
        Run(nameof(TestManagedMirrors), TestManagedMirrors);
        Run(nameof(TestExistingInterop), TestExistingInterop);
        Run(nameof(TestGeometryInterop), TestGeometryInterop);

        Console.WriteLine($"{s_checks} checks, {s_failures} failures");
        return s_failures == 0 ? 0 : 1;
    }

    // ------------------------------------------------------------
    // Native library lookup
    // ------------------------------------------------------------
    private static IntPtr ResolveNative(string name, Assembly assembly, DllImportSearchPath? searchPath)
    {
        if (name != "xgScriptNative")
            return IntPtr.Zero;

        string? dir = Environment.GetEnvironmentVariable("XG_BIN_DIR");
        if (string.IsNullOrEmpty(dir))
        {
            for (var d = new DirectoryInfo(AppContext.BaseDirectory); d != null; d = d.Parent)
            {
                string candidate = Path.Combine(d.FullName, "gameroot", "bin");
                if (File.Exists(Path.Combine(candidate, "xgScriptNative.dll")))
                {
                    dir = candidate;
                    break;
                }
            }
        }
        if (string.IsNullOrEmpty(dir))
            throw new DllNotFoundException("xgScriptNative.dll not found; build the engine or set XG_BIN_DIR");
        return NativeLibrary.Load(Path.Combine(dir, "xgScriptNative.dll"));
    }

    // ------------------------------------------------------------
    // Layout
    // ------------------------------------------------------------
    private static void TestLayout()
    {
        Check(Unsafe.SizeOf<Vector3>() == 16, "Vector3 is 16 bytes (C++ alignas(16))");
        try
        {
            MathLayout.Validate();
            Check(true, "layout");
        }
        catch (Exception e)
        {
            Check(false, e.Message);
        }
    }

    // ------------------------------------------------------------
    // Golden values (identical to the native suite)
    // ------------------------------------------------------------
    private static void TestGolden()
    {
        var rng = new Pcg32(42UL, 54UL);
        uint[] reference = [0xa15c02b7u, 0x7b47f409u, 0xba1d3330u, 0x83d2f293u, 0xbfa4784bu, 0xcbed606eu];
        foreach (uint expected in reference)
            Check(rng.NextUInt() == expected, $"PCG reference 0x{expected:x8}");
        uint[] bounded = [74u, 50u, 89u, 97u, 19u, 11u];
        foreach (uint expected in bounded)
            Check(rng.NextUInt(100u) == expected, $"PCG bounded {expected}");
        Near(rng.NextFloat(), 0.927618504f, 1e-7);
        Near(rng.NextFloat(), 0.219272852f, 1e-7);
        Near(rng.NextFloat(), 0.170426548f, 1e-7);

        Check(Hash.Hash32(0u) == 0x07bb2fe2u, "Hash32(0)");
        Check(Hash.Hash32(1u) == 0xa8beea3cu, "Hash32(1)");
        Check(Hash.Hash32(2u) == 0x7a7ecc88u, "Hash32(2)");
        Check(Hash.Of(3, -7, 11, 99u) == 0xbb51bf37u, "Hash(3,-7,11,99)");

        Near(Ease.Evaluate(EaseType.OutBounce, 0.3f), 0.680625081f, 1e-6);
        Near(Ease.Evaluate(EaseType.InOutBack, 0.7f), 1.07883346f, 1e-6);
        Near(Ease.Evaluate(EaseType.OutElastic, 0.2f), 1.125f, 1e-5);

        float v = 0.0f;
        Near(Motion.SmoothDamp(0.0f, 10.0f, ref v, 0.3f, 1.0f / 60.0f), 0.0559206009f, 1e-6);
        Near(v, 6.6293869f, 1e-5);

        var s = new Spring();
        s.UpdateCritical(10.0f, 0.2f, 0.1f);
        Near(s.Value, 1.5341444f, 1e-5);
        Near(s.Velocity, 24.0210762f, 1e-4);

        Color hsv = Color.FromHSV(0.3f, 0.6f, 0.8f);
        Near(hsv.r, 0.415999979f, 1e-6);
        Near(hsv.g, 0.800000012f, 1e-6);
        Near(hsv.b, 0.319999993f, 1e-6);
    }

    // ------------------------------------------------------------
    // Managed mirrors (same expectations as the native suite)
    // ------------------------------------------------------------
    private static void TestManagedMirrors()
    {
        const float Deg = MathF.PI / 180.0f;

        // Motion
        Near(Motion.DeltaAngle(359.0f * Deg, 1.0f * Deg), 2.0f * Deg, 1e-5);
        Near(Motion.WrapAngle(3.0f * MathF.PI), -MathF.PI, 1e-5);
        Near(Motion.Remap(5.0f, 0.0f, 10.0f, 100.0f, 200.0f), 150.0f, 1e-4);
        float expected = 10.0f * (1.0f - MathF.Exp(-5.0f));
        foreach (float fps in new[] { 30.0f, 60.0f, 144.0f })
        {
            float x = 0.0f;
            for (int i = 0; i < (int)(fps + 0.5f); ++i)
                x = Motion.Damp(x, 10.0f, 5.0f, 1.0f / fps);
            Near(x, expected, 1e-3);
        }

        var q0 = new Quaternion(0f, 0f, 0f, 1f);
        var q1 = VecMath.FromAxisAngle(V(0f, 1f, 0f), 90.0f * Deg);
        Near(VecMath.QuatAngle(q0, Motion.RotateTowards(q0, q1, 30.0f * Deg)), 30.0f * Deg, 1e-4);
        Vector3 dir = Motion.RotateTowards(V(2f, 0f, 0f), V(0f, 0f, 4f), 45.0f * Deg, 1.0f);
        Near(VecMath.Length(dir), 3.0f, 1e-4);

        // Easing endpoints
        for (int i = 0; i < (int)EaseType.Count; ++i)
        {
            Near(Ease.Evaluate((EaseType)i, 0.0f), 0.0f, 1e-5, $"Ease({(EaseType)i}, 0)");
            Near(Ease.Evaluate((EaseType)i, 1.0f), 1.0f, 1e-5, $"Ease({(EaseType)i}, 1)");
        }

        // Random
        var r = new Pcg32(123UL);
        bool sphereOk = true, rangeOk = true;
        for (int i = 0; i < 2000; ++i)
        {
            if (MathF.Abs(VecMath.Length(r.OnUnitSphere()) - 1.0f) > 1e-4f) sphereOk = false;
            int n = r.Range(-3, 3);
            if (n < -3 || n >= 3) rangeOk = false;
        }
        Check(sphereOk, "OnUnitSphere length 1");
        Check(rangeOk, "Range in [-3, 3)");
        Span<int> items = [0, 1, 2, 3, 4, 5, 6, 7, 8, 9];
        r.Shuffle(items);
        int mask = 0;
        foreach (int it in items) mask |= 1 << it;
        Check(mask == 0x3FF, "Shuffle is a permutation");
        Check(r.WeightedIndex([0f, 0f]) == -1, "WeightedIndex all zero");

        // Integer vectors
        Check(Vector2Int.FloorDiv(-1, 16) == -1 && Vector2Int.FloorMod(-1, 16) == 15, "FloorDiv/FloorMod");
        Check(Vector3Int.ChebyshevDistance(new Vector3Int(0, 0, 0), new Vector3Int(1, -2, 3)) == 3, "Chebyshev");
        Check(new Vector2Int(3, -4).Hash() == unchecked(3u * 73856093u ^ (uint)-4 * 19349663u), "Vector2Int hash");

        // Color
        var c = new Color(0.2f, 0.5f, 0.9f, 0.5f);
        Check(c.ToSRGB().ToLinear().IsApproxEqual(c, 1e-5f), "sRGB round trip");
        c.ToHSV(out float h, out float sat, out float val);
        Check(Color.FromHSV(h, sat, val, c.a).IsApproxEqual(c, 1e-5f), "HSV round trip");
        var c32 = new Color32(10, 20, 30, 40);
        Check(c32.PackRGBA() == 0x0A141E28u && c32.PackABGR() == 0x281E140Au, "Color32 pack");
        Check(Color.FromColor32(c32).ToColor32() == c32, "Color32 round trip");

        // Matrices
        Quaternion q = VecMath.FromAxisAngle(V(1f, 2f, 3f), 0.8f);
        Matrix3x3 rot = Matrix3x3.FromQuaternion(q);
        Vector3 pv = V(0.3f, -1.2f, 2.0f);
        NearV(rot.Transform(pv), VecMath.Rotate(q, pv), 1e-5f);
        NearV(rot.Transform(pv), q.RotateVec3(pv), 1e-5f); // native rotation agrees
        Quaternion back = rot.ToQuaternion();
        Check(MathF.Abs(MathF.Abs(VecMath.QuatDot(back, q)) - 1.0f) < 1e-5f, "Matrix3x3 ToQuaternion round trip");
        var m = new Matrix3x3(2f, 1f, 0f, 0f, 3f, 1f, 1f, 0f, 4f);
        Matrix3x3 ident = m * m.Inverse();
        Near(ident.m00 + ident.m11 + ident.m22, 3.0f, 1e-5);
        Near(ident.m01 + ident.m12 + ident.m20, 0.0f, 1e-5);
        NearV(Matrix3x3.Skew(V(1f, 2f, 3f)).Transform(V(-2f, 0.5f, 4f)), VecMath.Cross(V(1f, 2f, 3f), V(-2f, 0.5f, 4f)), 1e-5f);

        var trs = Matrix3x2.TRS(new Vector2(3f, -2f), 0.6f, new Vector2(2f, 0.5f));
        trs.Decompose(out Vector2 sc, out float ro, out Vector2 tr);
        Near(sc.x, 2.0f, 1e-5);
        Near(sc.y, 0.5f, 1e-5);
        Near(ro, 0.6f, 1e-5);
        Near(tr.x, 3.0f, 1e-5);
        var parent = new Transform2D(new Vector2(10f, 0f), MathF.PI / 2.0f, new Vector2(2f, 2f));
        Vector2 roundTrip = parent.InverseTransformPoint(parent.TransformPoint(new Vector2(3f, 4f)));
        Near(roundTrip.x, 3.0f, 1e-4);
        Near(roundTrip.y, 4.0f, 1e-4);

        // Rect
        Rect r1 = Rect.FromXYWH(0f, 0f, 10f, 10f);
        Rect inter = r1.Intersection(Rect.FromXYWH(5f, 5f, 10f, 10f));
        Check(inter.min.x == 5f && inter.max.x == 10f, "Rect intersection");
        Check(!r1.Intersection(Rect.FromXYWH(20f, 20f, 1f, 1f)).IsValid, "Rect disjoint");
    }

    // ------------------------------------------------------------
    // Existing Vector3 / Quaternion interop (LibraryImport + 16-byte Vector3)
    // ------------------------------------------------------------
    private struct Guarded
    {
        public Vector3 value;
        public uint sentinel;
    }

    private static void TestExistingInterop()
    {
        var g = new Guarded { sentinel = 0xDEADBEEF };
        g.value = V(1f, 0f, 0f).Cross(V(0f, 1f, 0f));
        NearV(g.value, V(0f, 0f, 1f), 1e-6f);
        Check(g.sentinel == 0xDEADBEEF, "sentinel after native Vec3 return");
        g.value = V(1f, 2f, 3f) + V(4f, 5f, 6f);
        NearV(g.value, V(5f, 7f, 9f), 1e-6f);
        Check(g.sentinel == 0xDEADBEEF, "sentinel after native Vec3 add");

        // Vector2 (8 bytes, returned in RAX on both sides)
        Vector2 sum2 = new Vector2(1f, 2f).Add(new Vector2(3f, 4f));
        Check(sum2.x == 4f && sum2.y == 6f, $"native Vec2 add returned {sum2}");
        Vector2 n2 = new Vector2(3f, 4f).Normalized();
        Near(n2.x, 0.6f, 1e-6);
        Near(n2.y, 0.8f, 1e-6);

        // Matrix and quaternion returns
        Matrix4x4 tm = Matrix4x4Extensions.Translation(1f, 2f, 3f);
        Near(tm.m30 + tm.m31 + tm.m32, 6.0f, 1e-6);
        NearV(new Quaternion(0f, 0f, 0f, 1f).RotateVec3(V(1f, 2f, 3f)), V(1f, 2f, 3f), 1e-6f);

        Check(V(0f, 0f, 0f).IsZero(), "IsZero(zero) == true");
        Check(!V(1f, 0f, 0f).IsZero(), "IsZero(x) == false");
        Check(new Quaternion(0f, 0f, 0f, 1f).IsIdentity(), "Quaternion IsIdentity");
        Check(!new Quaternion(0f, 1f, 0f, 0f).IsIdentity(), "Quaternion not identity");
    }

    // ------------------------------------------------------------
    // Geometry exports (same expectations as the native suite)
    // ------------------------------------------------------------
    private static Matrix4x4 PerspectiveFovLH(float fovY, float aspect, float near, float far)
    {
        float h = 1.0f / MathF.Tan(fovY * 0.5f);
        float w = h / aspect;
        float range = far / (far - near);
        return new Matrix4x4
        {
            m00 = w,
            m11 = h,
            m22 = range,
            m23 = 1.0f,
            m32 = -range * near,
        };
    }

    private static void TestGeometryInterop()
    {
        NearV(Geometry.ClosestPointOnSegment(V(0.5f, 1f, 0f), V(0f, 0f, 0f), V(1f, 0f, 0f), out float t), V(0.5f, 0f, 0f), 1e-6f);
        Near(t, 0.5f, 1e-6);

        float d2 = Geometry.ClosestPointsSegmentSegment(new Segment(V(-1f, 0f, 0f), V(1f, 0f, 0f)),
            new Segment(V(0f, -1f, 2f), V(0f, 1f, 2f)), out float s, out float u, out _, out _);
        Near(d2, 4.0f, 1e-5);
        Near(s, 0.5f, 1e-5);
        Near(u, 0.5f, 1e-5);

        var tri = new Triangle(V(0f, 0f, 0f), V(1f, 0f, 0f), V(0f, 1f, 0f));
        NearV(Geometry.ClosestPointOnTriangle(V(1f, 1f, 0f), tri), V(0.5f, 0.5f, 0f), 1e-6f);

        float sqrt2 = MathF.Sqrt(2.0f);
        var r45 = VecMath.FromAxisAngle(V(0f, 0f, 1f), MathF.PI / 4.0f);
        var rotated = new OBB(V(0f, 0f, 0f), V(1f, 1f, 1f), r45);
        Near(Geometry.ClosestPointOnOBB(V(5f, 0f, 0f), rotated).x, sqrt2, 1e-5);
        Near(rotated.ToAABB().max.x, sqrt2, 1e-5);

        var cap = new Capsule(V(0f, -1f, 0f), V(0f, 1f, 0f), 0.5f);
        NearV(Geometry.ClosestPointOnCapsule(V(3f, 0f, 0f), cap), V(0.5f, 0f, 0f), 1e-6f);

        // Overlaps
        Check(new Sphere(V(0.9f, 0f, 0f), 0.5f).Intersects(cap), "sphere-capsule hit");
        Check(!new Sphere(V(1.1f, 0f, 0f), 0.5f).Intersects(cap), "sphere-capsule miss");
        Check(cap.Intersects(new Capsule(V(0.9f, 0f, -1f), V(0.9f, 0f, 1f), 0.5f)), "capsule-capsule hit");
        Check(new Sphere(V(0.1f, 0.1f, 0.4f), 0.5f).Intersects(tri), "sphere-triangle hit");
        var unit = new OBB(V(0f, 0f, 0f), V(1f, 1f, 1f));
        var diag = new OBB(V(1.9f, 1.9f, 0f), V(1f, 1f, 1f), r45);
        Check(unit.Intersects(new OBB(V(2f, 0f, 0f), V(1f, 1f, 1f))), "OBB touching");
        Check(!unit.Intersects(new OBB(V(2.1f, 0f, 0f), V(1f, 1f, 1f))), "OBB separated");
        Check(!unit.Intersects(diag), "OBB diagonal separated (SAT)");
        Check(unit.Intersects(new OBB(V(2.3f, 0f, 0f), V(1f, 1f, 1f), r45)), "OBB rotated corner overlaps");
        Check(!new AABB(V(-1f, -1f, -1f), V(1f, 1f, 1f)).Intersects(diag), "AABB-OBB separated");

        // Plane classification
        Plane ground = Plane.FromPointNormal(V(0f, 0f, 0f), V(0f, 1f, 0f));
        Check(ground.Classify(new Sphere(V(0f, 2f, 0f), 1f)) == PlaneSide.Front, "plane front");
        Check(ground.Classify(new Sphere(V(0f, -2f, 0f), 1f)) == PlaneSide.Back, "plane back");
        Check(ground.Classify(new AABB(V(-1f, -1f, -1f), V(1f, 1f, 1f))) == PlaneSide.Intersecting, "plane AABB straddle");
        Check(ground.Classify(new OBB(V(0f, 1.5f, 0f), V(1f, 1f, 1f), r45)) == PlaneSide.Front, "plane OBB front");

        // Ray casts
        Check(new Ray(V(-5f, 0f, 0f), V(1f, 0f, 0f)).Raycast(rotated, out float hitT), "raycast OBB");
        Near(hitT, 5.0f - sqrt2, 1e-4);
        Check(new Ray(V(-5f, 0f, 0f), V(1f, 0f, 0f)).Raycast(cap, out hitT), "raycast capsule side");
        Near(hitT, 4.5f, 1e-4);
        Check(new Ray(V(0f, 5f, 0f), V(0f, -1f, 0f)).Raycast(cap, out hitT), "raycast capsule cap");
        Near(hitT, 3.5f, 1e-4);
        Check(!new Ray(V(-5f, 0f, 0f), V(-1f, 0f, 0f)).Raycast(cap, out _), "raycast capsule miss");

        // Sweeps
        Check(Geometry.Sweep(new Sphere(V(0f, 0f, 0f), 1f), V(10f, 0f, 0f), new Sphere(V(5f, 0f, 0f), 1f), out SweepHit hit), "sweep sphere-sphere");
        Near(hit.t, 0.3f, 1e-5);
        NearV(hit.normal, V(-1f, 0f, 0f), 1e-5f);
        NearV(hit.point, V(4f, 0f, 0f), 1e-5f);
        Check(Geometry.Sweep(new Sphere(V(0f, 5f, 0f), 1f), V(0f, -10f, 0f), ground, out hit), "sweep sphere-plane");
        Near(hit.t, 0.4f, 1e-5);
        var cube = new AABB(V(-1f, -1f, -1f), V(1f, 1f, 1f));
        Check(Geometry.Sweep(new Sphere(V(5f, 5f, 0f), 0.5f), V(-10f, -10f, 0f), cube, out hit), "sweep sphere-AABB edge");
        Near(hit.t, (5.0f - (1.0f + 0.5f / sqrt2)) / 10.0f, 1e-4);
        Check(!Geometry.Sweep(new Sphere(V(5f, 3f, 0f), 0.5f), V(-10f, 0f, 0f), cube, out _), "sweep sphere-AABB miss");
        Check(Geometry.Sweep(new AABB(V(0f, 0f, 0f), V(1f, 1f, 1f)), V(10f, 0f, 0f), new AABB(V(5f, 0f, 0f), V(6f, 1f, 1f)), out hit), "sweep AABB-AABB");
        Near(hit.t, 0.4f, 1e-5);
        NearV(hit.normal, V(-1f, 0f, 0f), 1e-6f);

        // 2D
        Check(Geometry.SegmentIntersect2D(new Vector2(0f, 0f), new Vector2(1f, 1f), new Vector2(0f, 1f), new Vector2(1f, 0f), out Vector2 ip, out _), "segment 2D hit");
        Near(ip.x, 0.5f, 1e-6);
        Vector2[] uShape = [new(0f, 0f), new(3f, 0f), new(3f, 3f), new(2f, 3f), new(2f, 1f), new(1f, 1f), new(1f, 3f), new(0f, 3f)];
        Check(Geometry.PointInPolygon(uShape, new Vector2(0.5f, 2f)), "point in U arm");
        Check(!Geometry.PointInPolygon(uShape, new Vector2(1.5f, 2f)), "point in U notch");
        Check(new Circle(new Vector2(0f, 0f), 1f).Intersects(new Circle(new Vector2(1.9f, 0f), 1f)), "circle-circle");

        // OBB helpers
        OBB moved = Geometry.OBBFromAABB(cube, Matrix4x4Extensions.Translation(1f, 2f, 3f));
        NearV(moved.center, V(1f, 2f, 3f), 1e-5f);
        NearV(moved.halfExtents, V(1f, 1f, 1f), 1e-5f);

        // Frustum
        Frustum f = Geometry.FrustumFromMatrix(PerspectiveFovLH(MathF.PI / 2.0f, 1.0f, 1.0f, 100.0f));
        Check(f.Contains(new Sphere(V(0f, 0f, 50f), 1f)) == ContainmentType.Contains, "frustum contains");
        Check(f.Contains(new Sphere(V(-5f, 0f, 1f), 0.5f)) == ContainmentType.Disjoint, "frustum straddle-near, outside-left");
        Check(f.Contains(new AABB(V(-1f, -1f, 10f), V(1f, 1f, 12f))) == ContainmentType.Contains, "frustum AABB");
        Check(f.Contains(new OBB(V(-200f, 0f, 20f), V(1f, 1f, 1f))) == ContainmentType.Disjoint, "frustum OBB outside");
        Sphere[] spheres = [new(V(0f, 0f, 50f), 1f), new(V(0f, 0f, 0.5f), 0.1f), new(V(0f, 0f, 1f), 0.5f)];
        Span<byte> visible = stackalloc byte[3];
        Check(Geometry.CullSpheres(f, spheres, visible) == 2, "batch cull count");
        Check(visible[0] == 1 && visible[1] == 0 && visible[2] == 1, "batch cull flags");
    }
}
