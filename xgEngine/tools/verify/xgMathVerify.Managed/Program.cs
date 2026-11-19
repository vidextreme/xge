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
using xg.editor;
using xg.math;

internal static class Program
{
    private static int _checks;
    private static int _failures;

    private static void Check(bool condition, string what, [CallerLineNumber] int line = 0)
    {
        _checks++;
        if (!condition)
        {
            _failures++;
            Console.WriteLine($"FAIL line {line}: {what}");
        }
    }

    private static void Near(double actual, double expected, double eps, [CallerArgumentExpression(nameof(actual))] string what = "", [CallerLineNumber] int line = 0)
    {
        _checks++;
        if (!(Math.Abs(actual - expected) <= eps))
        {
            _failures++;
            Console.WriteLine($"FAIL line {line}: {what} = {actual:G9}, expected {expected:G9} (eps {eps})");
        }
    }

    private static void NearV(Vector3 a, Vector3 b, float eps, [CallerLineNumber] int line = 0)
    {
        _checks++;
        if (MathF.Abs(a.x - b.x) > eps || MathF.Abs(a.y - b.y) > eps || MathF.Abs(a.z - b.z) > eps)
        {
            _failures++;
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
        NativeLibrary.SetDllImportResolver(typeof(Program).Assembly, ResolveNative);

        Run(nameof(TestLayout), TestLayout);
        Run(nameof(TestGolden), TestGolden);
        Run(nameof(TestManagedMirrors), TestManagedMirrors);
        Run(nameof(TestExistingInterop), TestExistingInterop);
        Run(nameof(TestGeometryInterop), TestGeometryInterop);
        Run(nameof(TestEventX), TestEventX);
        Run(nameof(TestAssetMeta), TestAssetMeta);
        Run(nameof(TestAssetItems), TestAssetItems);
        Run(nameof(TestAssetMetaMigration), TestAssetMetaMigration);
        Run(nameof(TestChunkFile), TestChunkFile);
        Run(nameof(TestAssetPipeline), TestAssetPipeline);
        Run(nameof(TestTextureImport), TestTextureImport);
        Run(nameof(TestResourceID), TestResourceID);
        Run(nameof(TestToolSelection), TestToolSelection);

        Console.WriteLine($"{_checks} checks, {_failures} failures");
        return _failures == 0 ? 0 : 1;
    }

    // ------------------------------------------------------------
    // Resource IDs
    // ------------------------------------------------------------
    private static void TestResourceID()
    {
        Check(Unsafe.SizeOf<ResourceGUID>() == 16, "ResourceGUID size");
        Check(Unsafe.SizeOf<ResourceID>() == 32, "ResourceID size");

        const string file = "0123456789abcdef0011223344556677";
        const string chunk = "fedcba98765432107766554433221100";
        Check(ResourceID.TryParse(file, out ResourceID fileOnly), "file-only parse");
        Check(fileOnly.File.Hi == 0x0123456789abcdefUL && fileOnly.File.Lo == 0x0011223344556677UL, "text order maps to Hi, Lo");
        Check(fileOnly.Chunk.IsZero, "file-only chunk is zero");
        Check(fileOnly.ToString() == file, "file-only format");

        string breadcrumb = file + "/" + chunk;
        Check(ResourceID.TryParse(breadcrumb, out ResourceID full), "breadcrumb parse");
        Check(full.Chunk.ToString() == chunk, "chunk text");
        Check(full.ToString() == breadcrumb, "breadcrumb format");
        Check(ResourceID.TryParse(breadcrumb.ToUpperInvariant(), out ResourceID upper) && upper.Equals(full), "uppercase parse");

        string assetGuid = AssetMeta.NewGuid();
        Check(ResourceID.TryParse(assetGuid, out ResourceID asset) && asset.ToString() == assetGuid && asset.File.ToString() == assetGuid, "asset meta GUID");

        string zero = new('0', 32);
        string[] invalid =
        {
            "", file[..31], file + "0", new string('g', 32), file + "/", file + "/" + chunk[..31], file + "/" + chunk + "0",
            "/" + chunk, file + "-" + chunk, zero, zero + "/" + chunk, file + "/" + zero,
        };
        foreach (string text in invalid)
            Check(!ResourceID.TryParse(text, out _), $"rejects '{text}'");
        Check(!ResourceID.TryParse(null, out _), "rejects null");
    }

    // ------------------------------------------------------------
    // Native library lookup
    // ------------------------------------------------------------
    private static IntPtr ResolveNative(string name, Assembly assembly, DllImportSearchPath? searchPath)
    {
        if (name != "xgScriptNative" && name != "xgEditorKernel")
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
        return NativeLibrary.Load(Path.Combine(dir, name + ".dll"));
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
    // ------------------------------------------------------------
    // XFG.Core.EventX
    // ------------------------------------------------------------
    private enum EvKey { A, B, Alloc, Threads, Throw }

    private static long _sink;

    private static void TestEventX()
    {
        int calls = 0;
        Action<int> add = v => calls += v;
        XFG.Core.EventX<EvKey, int>.Subscribe(EvKey.A, add);
        XFG.Core.EventX<EvKey, int>.Subscribe(EvKey.A, add);
        Check(XFG.Core.EventX<EvKey, int>.Count(EvKey.A) == 2, "duplicate subscription counted twice");
        XFG.Core.EventX<EvKey, int>.Broadcast(EvKey.A, 3);
        Check(calls == 6, "broadcast reaches both registrations");
        Check(XFG.Core.EventX<EvKey, int>.Unsubscribe(EvKey.A, add), "unsubscribe returns true");
        XFG.Core.EventX<EvKey, int>.Broadcast(EvKey.A, 1);
        Check(calls == 7, "one registration left");
        Check(XFG.Core.EventX<EvKey, int>.Unsubscribe(EvKey.A, add) && !XFG.Core.EventX<EvKey, int>.Unsubscribe(EvKey.A, add), "second unsubscribe true, third false");
        Check(!XFG.Core.EventX<EvKey, int>.HasEvent(EvKey.A), "no handlers left");

        bool threw = false;
        try
        {
            XFG.Core.EventX<EvKey, int>.Subscribe(EvKey.A, null!);
        }
        catch (ArgumentNullException)
        {
            threw = true;
        }
        Check(threw && XFG.Core.EventX<EvKey, int>.Count(EvKey.A) == 0, "null handler rejected without creating an entry");

        int order = 0, second = 0;
        Action<int>? selfRemoving = null;
        selfRemoving = _ =>
        {
            order++;
            XFG.Core.EventX<EvKey, int>.Unsubscribe(EvKey.B, selfRemoving!);
        };
        XFG.Core.EventX<EvKey, int>.Subscribe(EvKey.B, selfRemoving);
        XFG.Core.EventX<EvKey, int>.Subscribe(EvKey.B, _ => second++);
        XFG.Core.EventX<EvKey, int>.Broadcast(EvKey.B, 0);
        XFG.Core.EventX<EvKey, int>.Broadcast(EvKey.B, 0);
        Check(order == 1 && second == 2, "unsubscribing during a broadcast applies from the next one");

        var sub = XFG.Core.EventX<EvKey, int, string>.Subscribe(EvKey.B, (_, _) => { });
        Check(XFG.Core.EventX<EvKey, int, string>.Count(EvKey.B) == 1, "subscription handle registered");
        sub.Dispose();
        Check(XFG.Core.EventX<EvKey, int, string>.Count(EvKey.B) == 0, "Subscription.Dispose unsubscribes");
        default(XFG.Core.Subscription<EvKey, Action>).Dispose();

        int after = 0;
        XFG.Core.EventX<EvKey>.Subscribe(EvKey.Throw, () => throw new InvalidOperationException("expected test exception from EventX handler"));
        XFG.Core.EventX<EvKey>.Subscribe(EvKey.Throw, () => after++);
        XFG.Core.EventX<EvKey>.Broadcast(EvKey.Throw);
        Check(after == 1, "a throwing handler does not stop the others");
        XFG.Core.EventX.ThrowOnHandlerException = true;
        bool propagated = false;
        try
        {
            XFG.Core.EventX<EvKey>.Broadcast(EvKey.Throw);
        }
        catch (InvalidOperationException)
        {
            propagated = true;
        }
        XFG.Core.EventX.ThrowOnHandlerException = false;
        Check(propagated, "ThrowOnHandlerException propagates");

        XFG.Core.EventX<EvKey>.Subscribe(EvKey.Alloc, static () => _sink++);
        XFG.Core.EventX<EvKey, int>.Subscribe(EvKey.Alloc, static a => _sink += a);
        XFG.Core.EventX<EvKey, int, float>.Subscribe(EvKey.Alloc, static (a, b) => _sink += a + (long)b);
        XFG.Core.EventX<EvKey, int, float, Vector3>.Subscribe(EvKey.Alloc, static (a, b, c) => _sink += a + (long)c.x);
        XFG.Core.EventX<EvKey, int, float, Vector3, long>.Subscribe(EvKey.Alloc, static (a, b, c, d) => _sink += d);
        for (int pass = 0; pass < 2; pass++)
        {
            long before = GC.GetAllocatedBytesForCurrentThread();
            for (int i = 0; i < 10_000; i++)
            {
                XFG.Core.EventX<EvKey>.Broadcast(EvKey.Alloc);
                XFG.Core.EventX<EvKey, int>.Broadcast(EvKey.Alloc, i);
                XFG.Core.EventX<EvKey, int, float>.Broadcast(EvKey.Alloc, i, 1.0f);
                XFG.Core.EventX<EvKey, int, float, Vector3>.Broadcast(EvKey.Alloc, i, 1.0f, V(1f, 2f, 3f));
                XFG.Core.EventX<EvKey, int, float, Vector3, long>.Broadcast(EvKey.Alloc, i, 1.0f, V(1f, 2f, 3f), 4L);
            }
            long allocated = GC.GetAllocatedBytesForCurrentThread() - before;
            if (pass == 1)
                Check(allocated == 0, $"broadcast allocates nothing (allocated {allocated} bytes over 50,000 broadcasts)");
        }

        const int threads = 8, perThread = 1000;
        Parallel.For(0, threads, new ParallelOptions { MaxDegreeOfParallelism = threads }, t =>
        {
            Action<int>[] handlers = new Action<int>[perThread];
            for (int i = 0; i < perThread; i++)
            {
                int captured = t * perThread + i;
                handlers[i] = v => _ = v + captured;
                XFG.Core.EventX<EvKey, int>.Subscribe(EvKey.Threads, handlers[i]);
            }
            for (int i = 0; i < perThread; i += 2)
                XFG.Core.EventX<EvKey, int>.Unsubscribe(EvKey.Threads, handlers[i]);
        });
        Check(XFG.Core.EventX<EvKey, int>.Count(EvKey.Threads) == threads * perThread / 2, $"parallel subscribe/unsubscribe keeps every registration ({XFG.Core.EventX<EvKey, int>.Count(EvKey.Threads)})");

        XFG.Core.EventX.ClearAllSystems();
        Check(XFG.Core.EventX<EvKey, int>.Count(EvKey.Threads) == 0 && XFG.Core.EventX<EvKey>.Count(EvKey.Alloc) == 0, "ClearAllSystems clears every bus");
    }

    // ------------------------------------------------------------
    // Asset meta files and reconciling them with asset files
    // ------------------------------------------------------------
    private static void TestAssetMeta()
    {
        AssetMeta meta = new() { Guid = "0123456789abcdef0123456789abcdef", Tags = ["forest", "enemy", "enemy", ""] };
        string json = meta.ToJSON();
        string expected = "{\r\n  \"version\": 2,\r\n  \"guid\": \"0123456789abcdef0123456789abcdef\",\r\n  \"tags\": [\r\n    \"enemy\",\r\n    \"forest\"\r\n  ],\r\n  \"importer\": {\r\n    \"import\": {},\r\n    \"process\": {}\r\n  },\r\n  \"items\": []\r\n}\r\n";
        Check(json == expected, "meta JSON is pretty-printed with sorted, distinct tags:\n" + json);
        AssetMeta parsed = AssetMeta.Parse("{ \"guid\": \"0123456789abcdef0123456789abcdef\", \"tags\": [\"a\"], \"importer\": { \"scale\": 2 }, \"custom\": true }");
        Check(parsed.ImportSettings["scale"]!.GetValue<int>() == 2 && parsed.Importer["scale"] is null, "version 1 importer settings move into import");
        string round = parsed.ToJSON();
        Check(round.Contains("\"custom\": true") && round.Contains("\"scale\": 2"), "unknown keys and importer kept on rewrite");
        Check(AssetMeta.Parse(round).ToJSON() == round, "meta round trip is stable");
        Check(AssetMeta.IsValidGuid(AssetMeta.NewGuid()) && !AssetMeta.IsValidGuid("xyz") && !AssetMeta.IsValidGuid(null), "GUID validation");
        Check(AssetMeta.AssetPathOf("Assets/a.png.xmd") == "Assets/a.png" && AssetMeta.IsMetaPath("A.PNG.XMD") && !AssetMeta.IsMetaPath("a.png.xgm") && AssetMeta.IsLegacyMetaPath("A.PNG.XGM"), "meta paths");

        AssetMove folder = new("Assets/A", "Assets/B", true);
        Check(folder.Apply("Assets/A") == "Assets/B" && folder.Apply("Assets/A/x.png") == "Assets/B/x.png", "folder move applies inside");
        Check(folder.Apply("Assets/AB/x.png") == "Assets/AB/x.png" && folder.Apply("Assets/a/y") == "Assets/B/y", "folder move prefix edges");
        Check(new AssetMove("Assets/A", "Assets/B", false).Apply("Assets/A/x") == "Assets/A/x", "file move doesn't touch children");

        const string g1 = "11111111111111111111111111111111";
        const string g2 = "22222222222222222222222222222222";
        const string g3 = "33333333333333333333333333333333";
        Dictionary<string, AssetCacheState> noCache = new();
        static string? NoHash(string path) => null;
        static string Describe(List<AssetReconcileAction> actions) => string.Join("; ", actions.Select(a => $"{a.Kind} {a.Path} {a.From} {a.Guid}"));

        var actions = AssetReconciler.Reconcile([new("Assets/new.png", 10, 1)], [], noCache, NoHash);
        Check(actions.Count == 1 && actions[0].Kind == AssetReconcileKind.Create, "new asset gets a meta: " + Describe(actions));

        actions = AssetReconciler.Reconcile([new("Assets/b/tree.png", 10, 1)], [new("Assets/a/tree.png", g1)], noCache, NoHash);
        Check(actions.Count == 1 && actions[0] is { Kind: AssetReconcileKind.Recover, Path: "Assets/b/tree.png", From: "Assets/a/tree.png", Guid: g1 },
            "moved without its meta, same name: recovered: " + Describe(actions));

        Dictionary<string, AssetCacheState> cache = new() { [g1] = new("Assets/a/tree.png", 99, 1, "aa") };
        actions = AssetReconciler.Reconcile([new("Assets/b/tree.png", 10, 1)], [new("Assets/a/tree.png", g1)], cache, NoHash);
        Check(actions.Count == 2 && actions.Any(a => a.Kind == AssetReconcileKind.Create) && actions.Any(a => a.Kind == AssetReconcileKind.Delete),
            "moved and edited (size differs): no recovery: " + Describe(actions));

        cache = new() { [g1] = new("Assets/a/tree.png", 10, 1, "aa") };
        actions = AssetReconciler.Reconcile([new("Assets/b/oak.png", 10, 2)], [new("Assets/a/tree.png", g1)], cache, p => p == "Assets/b/oak.png" ? "AA" : null);
        Check(actions.Count == 1 && actions[0] is { Kind: AssetReconcileKind.Recover, Guid: g1 }, "moved and renamed, same hash: recovered: " + Describe(actions));

        actions = AssetReconciler.Reconcile([new("Assets/b/tree.png", 10, 1), new("Assets/c/tree.png", 10, 1)], [new("Assets/a/tree.png", g1)], noCache, NoHash);
        Check(actions.Count(a => a.Kind == AssetReconcileKind.Create) == 2 && actions.Count(a => a.Kind == AssetReconcileKind.Delete) == 1,
            "ambiguous match: two new metas, orphan deleted: " + Describe(actions));

        cache = new() { [g2] = new("Assets/orig.png", 5, 1, null) };
        actions = AssetReconciler.Reconcile([new("Assets/orig.png", 5, 1), new("Assets/copy.png", 5, 1)], [new("Assets/orig.png", g2), new("Assets/copy.png", g2)], cache, NoHash);
        Check(actions.Count == 1 && actions[0] is { Kind: AssetReconcileKind.Regenerate, Path: "Assets/copy.png" }, "duplicate GUID: the copy gets a new one: " + Describe(actions));

        actions = AssetReconciler.Reconcile([new("Assets/x.png", 5, 1)], [new("Assets/x.png", "bad")], noCache, NoHash);
        Check(actions.Count == 1 && actions[0].Kind == AssetReconcileKind.Regenerate, "invalid GUID regenerated: " + Describe(actions));

        actions = AssetReconciler.Reconcile([new("Assets/k.png", 5, 1), new("Assets/m/k.png", 5, 1)], [new("Assets/k.png", g3), new("Assets/old/k.png", g3)], noCache, NoHash);
        Check(actions.Count == 2 && actions.Any(a => a is { Kind: AssetReconcileKind.Create, Path: "Assets/m/k.png" }) && actions.Any(a => a is { Kind: AssetReconcileKind.Delete, Path: "Assets/old/k.png" }),
            "orphan whose GUID is still live is never recovered: " + Describe(actions));

        actions = AssetReconciler.Reconcile([new("Assets/gone.png", 1, 1)], [new("Assets/gone.png", g1), new("Assets/deleted.png", g2)], noCache, NoHash);
        Check(actions.Count == 1 && actions[0] is { Kind: AssetReconcileKind.Delete, Path: "Assets/deleted.png" }, "deleted asset: orphan meta deleted: " + Describe(actions));
    }

    private static void TestAssetItems()
    {
        int next = 0;
        string NewGuid() => (++next).ToString("x32");
        ImportedAssetItem[] imported = [new("mesh", "Root/Body"), new("mesh", "Root/Body"), new("hierarchy", "Root")];

        AssetItemUpdate first = AssetItemTable.Update([], imported, NewGuid);
        Check(first.Changed && first.Items.Count == 3 && first.Assigned.Count == 3, "first import assigns every item");
        Check(first.Assigned[0].Occurrence == 0 && first.Assigned[1].Occurrence == 1 && first.Assigned[0].Guid != first.Assigned[1].Guid, "duplicates fall back to occurrence order");
        Check(first.Warnings.Count == 1 && first.Warnings[0].Contains("share the name"), "duplicate names are reported");
        Check(first.Items[0].Kind == "hierarchy" && first.Items[1].Occurrence == 0 && first.Items[2].Occurrence == 1, "items are sorted");

        AssetItemUpdate again = AssetItemTable.Update(first.Items, imported, NewGuid);
        Check(!again.Changed && again.Assigned.SequenceEqual(first.Assigned), "reimport keeps every GUID");

        AssetItemUpdate renamed = AssetItemTable.Update(first.Items, [new("mesh", "Root/Torso"), new("hierarchy", "Root")], NewGuid);
        Check(renamed.Changed && renamed.Items.Count == 4 && renamed.Items.Count(i => i.Missing) == 2, "renamed item is new, old entries stay as missing");
        Check(renamed.Assigned[0].Guid != first.Assigned[0].Guid && renamed.Assigned[1].Guid == first.Assigned[2].Guid, "rename gets a new GUID, others keep theirs");

        AssetItemUpdate restored = AssetItemTable.Update(renamed.Items, imported, NewGuid);
        Check(restored.Assigned.SequenceEqual(first.Assigned) && restored.Items.Single(i => i.Path == "Root/Torso").Missing, "returning items recover their GUIDs");

        string shared = 7.ToString("x32");
        AssetItemUpdate repaired = AssetItemTable.Update(
            [new AssetItem { Kind = "mesh", Path = "A", Guid = shared }, new AssetItem { Kind = "mesh", Path = "B", Guid = shared }, new AssetItem { Kind = "mesh", Path = "C", Guid = new string('0', 32) }, new AssetItem { Kind = "mesh", Path = "C", Guid = 9.ToString("x32") }],
            [new("mesh", "A"), new("mesh", "B"), new("mesh", "C"), new("", "ignored")], NewGuid);
        Check(repaired.Assigned.Count == 3 && repaired.Assigned.Select(a => a.Guid).Distinct().Count() == 3 && repaired.Assigned.All(a => AssetItemTable.IsValidChunkGuid(a.Guid)), "bad stored GUIDs are repaired");
        Check(repaired.Warnings.Count == 4, "repairs and ignored items are reported: " + string.Join("; ", repaired.Warnings));

        AssetMeta meta = new() { Guid = 1.ToString("x32"), Items = restored.Items };
        AssetMeta parsed = AssetMeta.Parse(meta.ToJSON());
        Check(parsed.ToJSON() == meta.ToJSON() && parsed.Items.Count == restored.Items.Count, "items round trip through the meta");
    }

    private sealed class CountingImporter : IAssetImporter
    {
        public int Calls;
        public string Id => "test.import";
        public int Version { get; set; } = 1;
        public string DataFolder => "Models";
        public string DataExtension => ".xdv";
        public bool Accepts(string extension) => extension == ".src";
        public IReadOnlyList<AssetDataFile> Import(string sourcePath, System.Text.Json.Nodes.JsonObject settings)
        {
            Calls++;
            string text = File.ReadAllText(sourcePath);
            if (text == "bad")
                throw new InvalidDataException("bad source");
            byte[] data = System.Text.Encoding.UTF8.GetBytes(text.Trim());
            return [new AssetDataFile(".xrv", ChunkTag.Make("XRV "), [new AssetDataItem("mesh", "Root/Body", [new ChunkData(ChunkTag.Make("PVEC"), data)])])];
        }
    }

    private sealed class CountingProcessor : IAssetProcessor
    {
        public int Calls;
        public string Id => "test.process";
        public int Version => 1;
        public bool Accepts(string rawExtension) => rawExtension == ".xrv";
        public AssetDataFile Process(AssetDataFile raw, System.Text.Json.Nodes.JsonObject settings)
        {
            Calls++;
            return new AssetDataFile(".xdv", ChunkTag.Make("XDV "), raw.Items.Select(i => new AssetDataItem(i.Kind, i.Path, [new ChunkData(ChunkTag.Make("VTXS"), i.Chunks[0].Data)])).ToList(),
                Tags: ["@test:" + (settings["quality"]?.ToString() ?? "base"), "@Test:Mesh"]);
        }
    }

    private static void TestChunkFile()
    {
        const string fileGuid = "0123456789abcdef0011223344556677";
        const string itemGuid = "fedcba98765432107766554433221100";
        uint positions = ChunkTag.Make("PVEC");
        uint normals = ChunkTag.Make("NVEC");
        Check(positions == 0x43455650u && ChunkTag.Name(positions) == "PVEC", "chunk tag bits match XGCHUNK order");
        ChunkFile file = new()
        {
            Kind = ChunkTag.Make("XRV "),
            FileGuid = fileGuid,
            Chunks = { new ChunkData(ChunkTag.Make("INFO"), [9]) },
            Items = { new ChunkItem(itemGuid, [new ChunkData(positions, [1, 2, 3], 7, 42), new ChunkData(normals, [4, 5, 6, 7, 8, 9, 10, 11])]) },
        };
        byte[] bytes = file.ToBytes();
        Check(bytes.Length % 8 == 0, "chunks are padded to 8 bytes");
        ChunkFile parsed = ChunkFile.Parse(bytes);
        Check(parsed.FileGuid == fileGuid && parsed.Kind == file.Kind && parsed.Items[0].Guid == itemGuid, "managed header round trip");
        Check(parsed.Items[0].Chunks[0] is { Flags: 7, Extra: 42 } && parsed.Items[0].Chunks[0].Data.SequenceEqual(new byte[] { 1, 2, 3 }), "managed chunk round trip");
        Check(parsed.ToBytes().SequenceEqual(bytes), "writing is stable");

        Check(Resources.ValidateChunkFile(bytes) == ChunkFileStatus.Valid, "native reader accepts managed output");
        ResourceGUID item = ResourceID.Parse(fileGuid + "/" + itemGuid).Chunk;
        Check(Resources.FindChunk(bytes, item, normals, out ulong offset, out ulong size) && size == 8 && bytes[(int)offset] == 4, "native reader finds an item's chunk");
        Check(Resources.FindChunk(bytes, default, ChunkTag.Make("INFO"), out offset, out size) && size == 1 && bytes[(int)offset] == 9, "native reader finds a file-level chunk");
        Check(!Resources.FindChunk(bytes, item, ChunkTag.Make("NONE"), out _, out _), "missing type isn't found");

        void Rejected(byte[] data, ChunkFileStatus status, string what)
        {
            Check(Resources.ValidateChunkFile(data) == status, $"native rejects {what}");
            bool threw = false;
            try { ChunkFile.Parse(data); }
            catch (InvalidDataException) { threw = true; }
            Check(threw, $"managed rejects {what}");
        }
        Rejected(bytes[..31], ChunkFileStatus.TooSmall, "a short file");
        byte[] broken = (byte[])bytes.Clone();
        broken[0] = 0;
        Rejected(broken, ChunkFileStatus.BadMagic, "a bad magic");
        broken = (byte[])bytes.Clone();
        broken[4] = 2;
        Rejected(broken, ChunkFileStatus.UnsupportedVersion, "a newer version");
        Rejected(bytes[..^8], ChunkFileStatus.Truncated, "a truncated file");
        broken = (byte[])bytes.Clone();
        BitConverter.TryWriteBytes(broken.AsSpan(40), ulong.MaxValue);
        Rejected(broken, ChunkFileStatus.Truncated, "an oversized chunk");
        ChunkFile repeated = new() { Kind = 1, FileGuid = fileGuid, Items = { new ChunkItem(itemGuid, []), new ChunkItem(itemGuid, []) } };
        Rejected(repeated.ToBytes(), ChunkFileStatus.BadItem, "a repeated item GUID");
    }

    private static void TestAssetPipeline()
    {
        string root = Path.Combine(Path.GetTempPath(), "xge-pipeline-" + Guid.NewGuid().ToString("N"));
        Directory.CreateDirectory(root);
        try
        {
            string library = Path.Combine(root, "Library", "Data");
            string source = Path.Combine(root, "dog.src");
            const string guid = "abcdef00000000000000000000000001";
            File.WriteAllText(source, "one");
            AssetMeta meta = new() { Guid = guid };
            CountingImporter importer = new();
            CountingProcessor processor = new();
            AssetPipeline NewPipeline()
            {
                AssetPipeline created = new(library);
                created.Register(importer);
                created.Register(processor);
                return created;
            }
            AssetPipeline pipeline = NewPipeline();
            AssetBuildStatus Build()
            {
                AssetBuildRequest request = new(guid, source, null, meta.ImportSettings, meta.ProcessSettings, meta.Items, items =>
                {
                    AssetItemUpdate update = AssetItemTable.Update(meta.Items, items);
                    meta.Items = update.Items;
                    return update;
                });
                return pipeline.Build(request).Status;
            }

            Check(pipeline.CanImport("a/dog.SRC") == false && pipeline.CanImport("a/dog.src") && Build() == AssetBuildStatus.Imported, "first build imports");
            string chunkGuid = meta.Items.Single().Guid;
            string raw = pipeline.FindFile(guid, ".xrv")!;
            string processed = pipeline.FindFile(guid, ".xdv")!;
            string models = Path.Combine(library, "Models");
            Check(Path.GetFileName(raw) == guid + ".xrv" && Path.GetFileName(processed) == guid + ".xdv" && Path.GetDirectoryName(processed) == models, "Library is addressed by GUID inside a type folder");
            Check(File.Exists(processed + ".json") && Directory.GetDirectories(library).Length == 1 && Directory.GetDirectories(models).Length == 0 && pipeline.ManifestOf(guid)?.Source == "dog.src", "the manifest sits beside the data, with no per-asset folders");
            Check(Resources.ValidateChunkFile(File.ReadAllBytes(processed)) == ChunkFileStatus.Valid, "processed file is valid for the native reader");
            Check(ChunkFile.Parse(File.ReadAllBytes(processed)).Items.Single().Guid == chunkGuid, "processed item uses the kept chunk GUID");
            byte[] first = File.ReadAllBytes(processed);

            Check(pipeline.ManifestOf(guid)!.Tags.SequenceEqual(["@test:base", "@test:mesh"]), "processed tags are recorded in the manifest, lower case and in order");
            Check(pipeline.ReadAllTags().TryGetValue(guid, out IReadOnlyList<string>? allTags) && allTags.Count == 2, "every manifest's tags can be read at once");
            Check(Build() == AssetBuildStatus.UpToDate && importer.Calls == 1 && processor.Calls == 1, "unchanged inputs do nothing");

            meta.ProcessSettings["quality"] = 2;
            Check(Build() == AssetBuildStatus.Processed && importer.Calls == 1 && processor.Calls == 2, "process settings reuse stored raw data");
            Check(pipeline.ManifestOf(guid)!.Tags.Contains("@test:2"), "tags follow the process settings");

            meta.ImportSettings["scale"] = 2;
            Check(Build() == AssetBuildStatus.ManifestUpdated && importer.Calls == 2 && processor.Calls == 2, "import settings with identical raw output skip processing");
            Check(pipeline.ManifestOf(guid)!.Tags.Contains("@test:2"), "reused processed output keeps its tags");

            File.WriteAllText(source, "two");
            Check(Build() == AssetBuildStatus.Imported && importer.Calls == 3 && processor.Calls == 3, "source change rebuilds both stages");
            Check(meta.Items.Single().Guid == chunkGuid, "rebuild keeps the chunk GUID");

            File.WriteAllText(source, "bad");
            byte[] good = File.ReadAllBytes(processed);
            int calls = importer.Calls;
            Check(Build() == AssetBuildStatus.Failed && File.ReadAllBytes(processed).SequenceEqual(good), "failure keeps the last good output");
            Check(Build() == AssetBuildStatus.PreviouslyFailed && importer.Calls == calls + 1, "unchanged failure isn't retried");
            File.WriteAllText(source, "one");
            meta.ImportSettings.Remove("scale");
            meta.ProcessSettings.Remove("quality");
            Check(Build() == AssetBuildStatus.Imported, "fixing the source recovers");

            Directory.Delete(Path.Combine(root, "Library"), true);
            pipeline = NewPipeline();
            Check(Build() == AssetBuildStatus.Imported && meta.Items.Single().Guid == chunkGuid, "deleting Library keeps references");
            Check(File.ReadAllBytes(pipeline.FindFile(guid, ".xdv")!).SequenceEqual(first), "rebuild is byte-identical");

            importer.Version = 2;
            Check(Build() == AssetBuildStatus.ManifestUpdated, "importer version is part of the key");

            string temporary = processed + ".build~";
            string legacy = Path.Combine(library, "ab", guid);
            File.WriteAllText(temporary, "partial");
            Directory.CreateDirectory(legacy);
            File.WriteAllText(Path.Combine(legacy, "manifest.json"), "{}");
            Check(pipeline.RemoveOrphans(new HashSet<string> { guid }) == 0 && !File.Exists(temporary) && !Directory.Exists(Path.Combine(library, "ab")) && File.Exists(pipeline.FindFile(guid, ".xdv")), "live data is kept, temporary files and old folders go");
            Check(pipeline.RemoveOrphans(new HashSet<string>()) == 1 && !Directory.Exists(models), "orphaned data is removed");

            const string failingGuid = "abcdef00000000000000000000000002";
            File.WriteAllText(source, "bad");
            AssetMeta failingMeta = new() { Guid = failingGuid };
            AssetBuildResult failedFirst = pipeline.Build(new AssetBuildRequest(failingGuid, source, null, failingMeta.ImportSettings, failingMeta.ProcessSettings, failingMeta.Items, _ => null));
            Check(failedFirst.Status == AssetBuildStatus.Failed && File.Exists(Path.Combine(models, failingGuid + ".xdv.json")) && pipeline.ManifestOf(failingGuid)?.Error is not null && pipeline.FindFile(failingGuid, ".xdv") is null, "a failed first build records its error");

            AssetPipeline textures = new(Path.Combine(root, "Textures"));
            TextureAssets.Register(textures, _ => new DecodedImage(2, 2, [255, 0, 0, 255, 0, 0, 0, 255, 0, 0, 0, 255, 0, 255, 0, 255]));
            string image = Path.Combine(root, "cat.png");
            File.WriteAllText(image, "image");
            const string textureGuid = "cdef0000000000000000000000000001";
            AssetMeta defaults = AssetMeta.Create();
            textures.WriteDefaultSettings("Assets/new.png", defaults);
            AssetMeta savedDefaults = AssetMeta.Parse(defaults.ToJSON());
            Check(savedDefaults.ProcessSettings["texture"]?["format"]?.GetValue<string>() == "standard" && savedDefaults.ImportSettings.Count == 0 && TextureSettings.From(savedDefaults.ProcessSettings) == new TextureSettings(), "new texture metas store the default settings");
            AssetMeta unknown = AssetMeta.Create();
            textures.WriteDefaultSettings("Assets/readme.txt", unknown);
            Check(unknown.ProcessSettings.Count == 0, "other files get no texture settings");
            AssetMeta textureMeta = new() { Guid = textureGuid };
            textureMeta.ImportSettings["flipY"] = true;
            textureMeta.ProcessSettings["mipmaps"] = true;
            textureMeta.ProcessSettings["texture"] = new System.Text.Json.Nodes.JsonObject { ["format"] = "uncompressed" };
            AssetBuildResult built = textures.Build(new AssetBuildRequest(textureGuid, image, null, textureMeta.ImportSettings, textureMeta.ProcessSettings, textureMeta.Items, items =>
            {
                AssetItemUpdate update = AssetItemTable.Update(textureMeta.Items, items);
                textureMeta.Items = update.Items;
                return update;
            }));
            Check(built.Status == AssetBuildStatus.Imported, "texture builds: " + built.Error);
            Check(textures.FindFile(textureGuid, TextureAssets.RawExtension) is null, "texture raw data isn't stored");
            ChunkItem texture = ChunkFile.Parse(File.ReadAllBytes(textures.FindFile(textureGuid, TextureAssets.ProcessedExtension)!)).Items.Single();
            byte[] header = texture.Chunks.Single(c => c.Type == TextureAssets.HeaderChunk).Data;
            byte[] pixels = texture.Chunks.Single(c => c.Type == TextureAssets.PixelsChunk).Data;
            Check(BitConverter.ToInt32(header, 0) == 2 && BitConverter.ToInt32(header, 4) == 2 && BitConverter.ToInt32(header, 8) == 2 && BitConverter.ToInt32(header, 12) == 0 && BitConverter.ToInt32(header, 20) == TextureAssets.LayoutVersion, "texture header");
            Check(pixels.Length == 20 && pixels[5] == 255 && pixels[8] == 255 && pixels[16] == 137 && pixels[17] == 137 && pixels[18] == 0 && pixels[19] == 255, "flipY and sRGB-correct mips");
        }
        finally
        {
            Directory.Delete(root, true);
        }
    }

    private static byte[] TextureBytes(TextureSettings settings, DecodedImage image, out ChunkData header, out ChunkData pixels)
    {
        string root = Path.Combine(Path.GetTempPath(), "xge-texture-" + Guid.NewGuid().ToString("N"));
        Directory.CreateDirectory(root);
        try
        {
            string source = Path.Combine(root, "image.png");
            File.WriteAllText(source, "source");
            const string guid = "abcd0000000000000000000000000002";
            AssetMeta meta = new() { Guid = guid };
            settings.WriteTo(meta.ProcessSettings);
            AssetPipeline pipeline = new(Path.Combine(root, "Library"));
            TextureAssets.Register(pipeline, _ => image);
            AssetBuildResult result = pipeline.Build(new AssetBuildRequest(guid, source, null, meta.ImportSettings, meta.ProcessSettings, meta.Items, items =>
            {
                AssetItemUpdate update = AssetItemTable.Update(meta.Items, items);
                meta.Items = update.Items;
                return update;
            }));
            Check(result.Status == AssetBuildStatus.Imported, "texture build: " + result.Error);
            byte[] file = File.ReadAllBytes(pipeline.FindFile(guid, TextureAssets.ProcessedExtension)!);
            Check(Resources.ValidateChunkFile(file) == ChunkFileStatus.Valid, "native reader accepts the texture");
            ChunkItem item = ChunkFile.Parse(file).Items.Single();
            header = item.Chunks.Single(c => c.Type == TextureAssets.HeaderChunk);
            pixels = item.Chunks.Single(c => c.Type == TextureAssets.PixelsChunk);
            return file;
        }
        finally
        {
            Directory.Delete(root, true);
        }
    }

    private static void DecodeAlphaBlock(ReadOnlySpan<byte> block, Span<byte> output)
    {
        Span<int> values = stackalloc int[8];
        int a = block[0], b = block[1];
        values[0] = a;
        values[1] = b;
        for (int i = 1; i < (a > b ? 7 : 5); i++)
            values[i + 1] = a > b ? ((7 - i) * a + i * b) / 7 : ((5 - i) * a + i * b) / 5;
        if (a <= b)
        {
            values[6] = 0;
            values[7] = 255;
        }
        ulong bits = 0;
        for (int i = 5; i >= 0; i--)
            bits = bits << 8 | block[2 + i];
        for (int i = 0; i < 16; i++)
            output[i] = (byte)values[(int)(bits >> (i * 3)) & 7];
    }

    private static void DecodeColorBlock(ReadOnlySpan<byte> block, Span<byte> rgb)
    {
        int c0 = block[0] | block[1] << 8, c1 = block[2] | block[3] << 8;
        Span<int> colors = stackalloc int[12];
        for (int e = 0; e < 2; e++)
        {
            int c = e == 0 ? c0 : c1;
            colors[e * 3] = (c >> 11 & 31) * 255 / 31;
            colors[e * 3 + 1] = (c >> 5 & 63) * 255 / 63;
            colors[e * 3 + 2] = (c & 31) * 255 / 31;
        }
        for (int ch = 0; ch < 3; ch++)
        {
            colors[6 + ch] = c0 > c1 ? (2 * colors[ch] + colors[3 + ch]) / 3 : (colors[ch] + colors[3 + ch]) / 2;
            colors[9 + ch] = c0 > c1 ? (colors[ch] + 2 * colors[3 + ch]) / 3 : 0;
        }
        uint bits = BitConverter.ToUInt32(block[4..8]);
        for (int i = 0; i < 16; i++)
        {
            int index = (int)(bits >> (i * 2)) & 3;
            for (int ch = 0; ch < 3; ch++)
                rgb[i * 3 + ch] = (byte)colors[index * 3 + ch];
        }
    }

    // Average absolute error per channel between the first encoded mip and the expected RGBA values.
    private static double BlockError(TextureHardwareFormat format, byte[] data, byte[] expected, int width, int height, int channels)
    {
        int blockBytes = TextureFormats.BytesPerBlock(format);
        long total = 0, count = 0;
        Span<byte> rgb = stackalloc byte[48];
        Span<byte> first = stackalloc byte[16];
        Span<byte> second = stackalloc byte[16];
        for (int by = 0; by < height / 4; by++)
        {
            for (int bx = 0; bx < width / 4; bx++)
            {
                ReadOnlySpan<byte> block = data.AsSpan((by * (width / 4) + bx) * blockBytes, blockBytes);
                if (format == TextureHardwareFormat.BC1)
                    DecodeColorBlock(block, rgb);
                else if (format == TextureHardwareFormat.BC3)
                {
                    DecodeAlphaBlock(block, first);
                    DecodeColorBlock(block[8..], rgb);
                }
                else if (format == TextureHardwareFormat.BC4)
                    DecodeAlphaBlock(block, first);
                else
                {
                    DecodeAlphaBlock(block, first);
                    DecodeAlphaBlock(block[8..], second);
                }
                for (int i = 0; i < 16; i++)
                {
                    int source = ((by * 4 + i / 4) * width + bx * 4 + i % 4) * 4;
                    for (int c = 0; c < channels; c++)
                    {
                        int actual = format switch
                        {
                            TextureHardwareFormat.BC4 => first[i],
                            TextureHardwareFormat.BC5 => c == 0 ? first[i] : second[i],
                            TextureHardwareFormat.BC3 when c == 3 => first[i],
                            _ => rgb[i * 3 + c],
                        };
                        total += Math.Abs(actual - expected[source + c]);
                        count++;
                    }
                }
            }
        }
        return (double)total / count;
    }

    private static void TestTextureImport()
    {
        TextureSettings color = new();
        Check(TextureFormats.Resolve(color, false) == TextureHardwareFormat.BC1 && TextureFormats.Resolve(color, true) == TextureHardwareFormat.BC3, "color resolves by alpha");
        Check(TextureFormats.Resolve(color with { Alpha = TextureAlpha.None }, true) == TextureHardwareFormat.BC1, "no alpha forces BC1");
        Check(TextureFormats.Resolve(color with { Alpha = TextureAlpha.Keep }, false) == TextureHardwareFormat.BC3, "keep alpha forces BC3");
        Check(TextureFormats.Resolve(color with { Type = TextureUsage.Normal }, true) == TextureHardwareFormat.BC5, "normal resolves to BC5");
        Check(TextureFormats.Resolve(color with { Type = TextureUsage.Luminance }, true) == TextureHardwareFormat.BC4, "luminance resolves to BC4");
        Check(TextureFormats.Resolve(color with { Format = TextureCompression.Uncompressed, Type = TextureUsage.Normal }, true) == TextureHardwareFormat.RGBA8, "uncompressed resolves to RGBA8");

        TextureBuildPlan plan = TextureFormats.Plan(color, 330, 130, true);
        Check(plan is { Format: TextureHardwareFormat.BC3, Width: 332, Height: 132, MipCount: 9 }, $"block sizes round to multiples of 4: {plan}");
        plan = TextureFormats.Plan(color with { Scale = TextureScale.Half, MipFilter = TextureMipFilter.None }, 512, 512, false);
        Check(plan is { Format: TextureHardwareFormat.BC1, Width: 256, Height: 256, MipCount: 1, ByteSize: 32768 }, $"scale and size: {plan}");
        Check(TextureFormats.ImportTags(color, plan).SequenceEqual(["@texture:color", "@texture:bc1", "@texture:256x256"]), "import tags: usage, format, size");
        TextureSettings alphaColor = color with { Alpha = TextureAlpha.Keep };
        Check(TextureFormats.ImportTags(alphaColor, TextureFormats.Plan(alphaColor, 64, 32, true)).SequenceEqual(["@texture:color", "@texture:bc3", "@texture:64x32", "@texture:alpha"]), "import tags include alpha when kept");
        TextureSettings normal = color with { Type = TextureUsage.Normal, Format = TextureCompression.Uncompressed };
        Check(TextureFormats.ImportTags(normal, TextureFormats.Plan(normal, 30, 30, true)).SequenceEqual(["@texture:normal", "@texture:rgba8", "@texture:30x30"]), "import tags of an uncompressed normal map");

        System.Text.Json.Nodes.JsonObject process = new() { ["mipmaps"] = false, ["custom"] = 1 };
        TextureSettings legacy = TextureSettings.From(process);
        Check(legacy.MipFilter == TextureMipFilter.None, "old mipmaps flag is read");
        legacy.WriteTo(process);
        Check(process["mipmaps"] is null && process["custom"] is not null && TextureSettings.From(process) == legacy, "settings round trip and drop the old flag");
        Check(new TextureSettings().WithText("format=uncompressed,type=normal,flipGreen=true,mipFilter=none") is { Format: TextureCompression.Uncompressed, Type: TextureUsage.Normal, FlipGreen: true, MipFilter: TextureMipFilter.None }, "text settings parse");

        TextureImportDraft draft = new(color);
        draft.Draft = color with { Type = TextureUsage.Normal };
        Check(draft.IsDirty, "editing makes the draft dirty");
        draft.Revert();
        Check(!draft.IsDirty && draft.Draft == color, "revert restores the saved settings");
        draft.Draft = color with { Quality = TextureQuality.High };
        draft.MarkApplied();
        Check(!draft.IsDirty && draft.Saved.Quality == TextureQuality.High, "apply makes the draft the saved state");

        const int size = 32;
        byte[] rgba = new byte[size * size * 4];
        for (int y = 0; y < size; y++)
            for (int x = 0; x < size; x++)
            {
                int i = (y * size + x) * 4;
                rgba[i] = (byte)(x * 255 / (size - 1));
                rgba[i + 1] = (byte)(y * 255 / (size - 1));
                rgba[i + 2] = (byte)((x + y) * 255 / (2 * size - 2));
                rgba[i + 3] = (byte)(255 - x * 255 / (size - 1));
            }
        DecodedImage image = new(size, size, rgba);
        byte[] opaque = (byte[])rgba.Clone();
        for (int i = 3; i < opaque.Length; i += 4)
            opaque[i] = 255;

        foreach ((TextureSettings settings, DecodedImage source, TextureHardwareFormat format, int channels) in new[]
        {
            (color, new DecodedImage(size, size, opaque), TextureHardwareFormat.BC1, 3),
            (color, image, TextureHardwareFormat.BC3, 4),
            (color with { Type = TextureUsage.Normal, MipFilter = TextureMipFilter.None }, new DecodedImage(size, size, opaque), TextureHardwareFormat.BC5, 0),
        })
        {
            TextureBytes(settings, source, out ChunkData header, out ChunkData pixels);
            Check(pixels.Extra == (ulong)format, $"{format} is recorded in TXPX");
            TextureBuildPlan expected = TextureFormats.Plan(settings, size, size, TextureFormats.HasAlpha(source.Pixels));
            Check(pixels.Data.Length == expected.ByteSize && BitConverter.ToInt32(header.Data, 8) == expected.MipCount, $"{format} size and mip count");
            if (channels > 0)
            {
                double error = BlockError(format, pixels.Data, source.Pixels, size, size, channels);
                Check(error < 8.0, $"{format} average error {error:F2}");
            }
        }

        byte[] gray = new byte[size * size * 4];
        for (int i = 0; i < gray.Length; i += 4)
        {
            gray[i] = gray[i + 1] = gray[i + 2] = (byte)((i / 4 % size) * 255 / (size - 1));
            gray[i + 3] = 255;
        }
        TextureBytes(color with { Type = TextureUsage.Luminance, MipFilter = TextureMipFilter.None }, new DecodedImage(size, size, gray), out _, out ChunkData luminance);
        TextureBytes(color with { Type = TextureUsage.Luminance, Format = TextureCompression.Uncompressed, MipFilter = TextureMipFilter.None }, new DecodedImage(size, size, gray), out _, out ChunkData luminanceReference);
        double luminanceError = BlockError(TextureHardwareFormat.BC4, luminance.Data, luminanceReference.Data, size, size, 1);
        Check(luminance.Extra == (ulong)TextureHardwareFormat.BC4 && luminanceError < 6.0, $"BC4 average error {luminanceError:F2}");

        byte[] normals = new byte[8 * 8 * 4];
        for (int i = 0; i < normals.Length; i += 4)
        {
            bool left = (i / 4 % 8) < 4;
            normals[i] = left ? (byte)217 : (byte)38;
            normals[i + 1] = 128;
            normals[i + 2] = 217;
            normals[i + 3] = 255;
        }
        TextureBytes(color with { Type = TextureUsage.Normal, Format = TextureCompression.Uncompressed }, new DecodedImage(8, 8, normals), out _, out ChunkData normalPixels);
        int offset = normalPixels.Data.Length - 4;
        Vector3 last = V(normalPixels.Data[offset] / 255.0f * 2.0f - 1.0f, normalPixels.Data[offset + 1] / 255.0f * 2.0f - 1.0f, normalPixels.Data[offset + 2] / 255.0f * 2.0f - 1.0f);
        Near(last.Length(), 1.0, 0.02);
    }

    private static void TestAssetMetaMigration()
    {
        string root = Path.Combine(Path.GetTempPath(), "xge-meta-migration-" + Guid.NewGuid().ToString("N"));
        try
        {
            string folder = Path.Combine(root, "Models");
            Directory.CreateDirectory(folder);
            const string g1 = "11111111111111111111111111111111";
            const string g2 = "22222222222222222222222222222222";
            string V1(string guid) => $"{{ \"version\": 1, \"guid\": \"{guid}\", \"tags\": [\"a\"], \"importer\": {{ \"scale\": 2 }}, \"custom\": true }}";
            File.WriteAllText(Path.Combine(folder, "dog.fbx.xgm"), V1(g1));
            File.WriteAllText(Path.Combine(folder, "same.png.xgm"), V1(g1));
            File.WriteAllText(Path.Combine(folder, "same.png.xmd"), V1(g1));
            File.WriteAllText(Path.Combine(folder, "clash.png.xgm"), V1(g1));
            File.WriteAllText(Path.Combine(folder, "clash.png.xmd"), V1(g2));
            File.WriteAllText(Path.Combine(folder, "broken.png.xgm"), "{ not json");

            AssetMetaMigrationResult result = AssetMetaMigration.Run(root);
            Check(result.Migrated == 2 && result.Unreadable == 1 && result.Problems.Count == 1, $"migration counts {result.Migrated}/{result.Unreadable}/{result.Problems.Count}");
            AssetMeta dog = AssetMeta.Read(Path.Combine(folder, "dog.fbx.xmd"));
            Check(!File.Exists(Path.Combine(folder, "dog.fbx.xgm")) && dog.Guid == g1 && dog.Tags.SequenceEqual(["a"]), "GUID and tags survive migration");
            Check(dog.Version == AssetMeta.CurrentVersion && dog.ImportSettings["scale"]!.GetValue<int>() == 2 && dog.ProcessSettings.Count == 0, "settings move into import");
            Check(dog.ToJSON().Contains("\"custom\": true"), "unknown keys survive migration");
            Check(!File.Exists(Path.Combine(folder, "same.png.xgm")), "repeated run removes a finished legacy file");
            Check(File.Exists(Path.Combine(folder, "clash.png.xgm")) && AssetMeta.Read(Path.Combine(folder, "clash.png.xmd")).Guid == g2, "conflict leaves both files");
            Check(File.ReadAllText(Path.Combine(folder, "broken.png.xmd")) == "{ not json", "unreadable meta moves unchanged");
            Check(AssetMetaMigration.Run(root) is { Migrated: 0, Unreadable: 0 }, "second run has nothing to do");
        }
        finally
        {
            if (Directory.Exists(root))
                Directory.Delete(root, true);
        }
    }

    // ------------------------------------------------------------
    // Tool selections and drivers
    // ------------------------------------------------------------
    private sealed class CountingDriver : IToolDriver
    {
        public CountingDriver(object key) => Key = key;
        public object Key { get; }
        public bool Dirty;
        public int Applied;
        public bool Disposed;
        public bool HasPendingChanges => Dirty;
        public void Apply() { Applied++; Dirty = false; }
        public void Revert() => Dirty = false;
        public void Dispose() => Disposed = true;
    }

    private sealed class ItemTool : ITool
    {
        public readonly List<CountingDriver> Created = new();
        public int Asked;
        public ToolUsability OnSelectItems(object[] selection)
        {
            Asked++;
            return selection.OfType<string>().Any() ? ToolUsability.Usable : ToolUsability.NotApplicable;
        }
        public object? DriverKey(ToolSelection selection) => selection.Last<string>();
        public IToolDriver? CreateDriver(ToolSelection selection)
        {
            CountingDriver driver = new(selection.Last<string>()!);
            Created.Add(driver);
            return driver;
        }
    }

    private sealed class WholeTool : ITool
    {
        public readonly List<CountingDriver> Created = new();
        public ToolUsability OnSelectItems(object[] selection) => ToolUsability.Usable;
        public IToolDriver? CreateDriver(ToolSelection selection)
        {
            CountingDriver driver = new(selection);
            Created.Add(driver);
            return driver;
        }
    }

    private sealed class ThrowingTool : ITool
    {
        public ToolUsability OnSelectItems(object[] selection) => throw new InvalidOperationException("usability");
        public IToolDriver? CreateDriver(ToolSelection selection) => throw new InvalidOperationException("driver");
    }

    private sealed class CountingState : IDisposable
    {
        public int Value;
        public bool Disposed;
        public void Dispose() => Disposed = true;
    }

    private static void TestToolSelection()
    {
        List<Exception> errors = new();
        ToolSelection.ErrorHandler = errors.Add;
        try
        {
            ItemTool item = new();
            WholeTool whole = new();

            ToolSelection a = new(["A"]);
            Check(item.Created.Count == 0, "drivers are created lazily");
            CountingDriver driverA = (CountingDriver)a.DriverFor(item)!;
            Check(ReferenceEquals(a.DriverFor(item), driverA) && item.Created.Count == 1, "one driver per tool and key");
            Check(a.Usability(item) == ToolUsability.Usable && a.Usability(item) == ToolUsability.Usable && item.Asked == 1, "usability is asked once");
            CountingDriver wholeA = (CountingDriver)a.DriverFor(whole)!;

            ToolSelection ab = a.Next(["A", 7]);
            a.Release();
            Check(!driverA.Disposed && wholeA.Disposed, "A -> A+7 keeps the item driver and drops the whole-selection one");
            Check(ReferenceEquals(ab.DriverFor(item), driverA) && item.Created.Count == 1, "carried driver is reused, not created again");
            CountingDriver wholeAB = (CountingDriver)ab.DriverFor(whole)!;
            Check(!ReferenceEquals(wholeAB, wholeA) && whole.Created.Count == 2, "whole-selection driver is rebuilt on every change");

            ToolSelection abb = ab.Next(["A", 7, "B"]);
            ab.Release();
            CountingDriver driverB = (CountingDriver)abb.DriverFor(item)!;
            Check(driverB.Key.Equals("B") && !driverA.Disposed, "a new active item gets its own driver, the old one stays while selected");
            Check(abb.Drivers.Count() == 2, "both item drivers are listed");

            driverA.Dirty = true;
            Check(abb.HasPendingChanges, "pending changes are seen");
            Check(abb.PendingOnDeselect(["A", "C"]).Count == 0, "keeping A loses nothing");
            Check(abb.PendingOnDeselect(["B"]).SequenceEqual([driverA]), "deselecting A would lose its draft");

            abb.Retain();
            ToolSelection b = abb.Next(["B"]);
            abb.Release();
            Check(!driverA.Disposed && !abb.IsReleased, "a retained (locked) selection keeps deselected drivers");
            Check(abb.HasPendingChanges && ReferenceEquals(abb.DriverFor(item), driverB), "the locked selection still works");
            foreach (IToolDriver driver in abb.PendingOnDeselect(b.Objects))
                driver.Apply();
            Check(driverA.Applied == 1 && !abb.HasPendingChanges, "pending drivers apply through the selection");
            abb.Release();
            Check(driverA.Disposed && !driverB.Disposed && abb.IsReleased, "the last release disposes only the drivers nobody else holds");
            Check(abb.DriverFor(item) is null, "a released selection makes no drivers");

            b.Release();
            Check(driverB.Disposed, "releasing the last selection disposes the rest");

            ToolSelection empty = new(Array.Empty<object>());
            Check(empty.DriverFor(item) is null && item.Created.Count == 2, "no key, no driver");
            Check(empty.Usability(item) == ToolUsability.NotApplicable, "usability comes from the tool");
            empty.Release();

            ThrowingTool throwing = new();
            ToolSelection t = new(["X"]);
            Check(t.Usability(throwing) == ToolUsability.NotApplicable && t.DriverFor(throwing) is null && errors.Count == 2, "tool exceptions are reported, not thrown");
            t.Release();

            ToolView view = new();
            CountingState first = view.State<CountingState>();
            first.Value = 3;
            CountingState scoped = view.Scope("tool").State<CountingState>();
            Check(ReferenceEquals(view.State<CountingState>(), first) && !ReferenceEquals(scoped, first) && scoped.Value == 0, "view scopes keep separate state");
            Check(ReferenceEquals(view.Scope("tool").State<CountingState>(), scoped), "a scope is kept per key");
            view.Dispose();
            Check(first.Disposed && scoped.Disposed, "disposing a view disposes its states and scopes");
        }
        finally
        {
            ToolSelection.ErrorHandler = null;
        }
    }
}
