// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.

// xgMathVerify - native tests for the XGE math library (header-only, no engine dependencies).
// Golden values shared with verify/xgMathVerify.Managed are marked GOLDEN; if one changes,
// update both suites. Run with --golden to print them.

#include <cstdio>
#include <cstring>
#include "xgMath.h"

using namespace xg;

static int g_checks = 0;
static int g_failures = 0;

#define XG_CHECK(cond) \
    do { ++g_checks; if (!(cond)) { ++g_failures; printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); } } while (0)

#define XG_CHECK_NEAR(a, b, eps) \
    do { ++g_checks; double _a = (double)(a), _b = (double)(b); \
         if (!(_a - _b <= (eps) && _b - _a <= (eps))) { ++g_failures; \
         printf("FAIL %s:%d: %s = %.9g, expected %.9g (eps %g)\n", __FILE__, __LINE__, #a, _a, _b, (double)(eps)); } } while (0)

static void CheckVec3(const Vec3& a, const Vec3& b, float eps, int line)
{
    ++g_checks;
    if (Abs(a.x - b.x) > eps || Abs(a.y - b.y) > eps || Abs(a.z - b.z) > eps)
    {
        ++g_failures;
        printf("FAIL line %d: (%.6f, %.6f, %.6f) != (%.6f, %.6f, %.6f)\n", line, a.x, a.y, a.z, b.x, b.y, b.z);
    }
}
#define XG_CHECK_VEC3(a, b, eps) CheckVec3((a), (b), (eps), __LINE__)

//============================================================
// Golden values (shared with the managed suite)
//============================================================
static void PrintGolden()
{
    Pcg32 rng(42u, 54u);
    printf("PCG seed 42 stream 54:");
    for (int i = 0; i < 6; ++i)
        printf(" 0x%08x", rng.NextUInt());
    printf("\nPCG bounded(100):");
    for (int i = 0; i < 6; ++i)
        printf(" %u", rng.NextUInt(100u));
    printf("\nPCG float:");
    for (int i = 0; i < 3; ++i)
        printf(" %.9g", rng.NextFloat());
    printf("\nHash32(0..2): 0x%08x 0x%08x 0x%08x\n", Hash32(0u), Hash32(1u), Hash32(2u));
    printf("Hash(3,-7,11,99): 0x%08x\n", Hash(3, -7, 11, 99u));
    printf("Ease OutBounce(0.3) %.9g InOutBack(0.7) %.9g OutElastic(0.2) %.9g\n",
        Ease(EaseType::OutBounce, 0.3f), Ease(EaseType::InOutBack, 0.7f), Ease(EaseType::OutElastic, 0.2f));
    float v = 0.0f;
    float x = SmoothDamp(0.0f, 10.0f, v, 0.3f, 1.0f / 60.0f);
    printf("SmoothDamp(0->10, 0.3, 1/60) %.9g v %.9g\n", x, v);
    Spring s;
    s.UpdateCritical(10.0f, 0.2f, 0.1f);
    printf("Spring critical %.9g v %.9g\n", s.Value, s.Velocity);
    Color hsv = Color::FromHSV(0.3f, 0.6f, 0.8f);
    printf("HSV(0.3,0.6,0.8) %.9g %.9g %.9g\n", hsv.r, hsv.g, hsv.b);
}

//============================================================
// Existing-type fixes
//============================================================
static void TestExistingFixes()
{
    // Mat4x4::TransformPoint must apply translation
    Mat4x4 t = Mat4x4::Translation(1.0f, 2.0f, 3.0f);
    XG_CHECK_VEC3(t.TransformPoint(Vec3(1.0f, 1.0f, 1.0f)), Vec3(2.0f, 3.0f, 4.0f), 1e-6f);
    XG_CHECK_VEC3(t.TransformDirection(Vec3(1.0f, 1.0f, 1.0f)), Vec3(1.0f, 1.0f, 1.0f), 1e-6f);

    // AABB transform
    AABB box(Vec3(-1.0f), Vec3(1.0f));
    AABB moved = box.Transform(t);
    XG_CHECK_VEC3(moved.min, Vec3(0.0f, 1.0f, 2.0f), 1e-5f);
    XG_CHECK_VEC3(moved.max, Vec3(2.0f, 3.0f, 4.0f), 1e-5f);

    // AABB <-> Sphere, Ray tests now defined
    XG_CHECK(box.Intersects(Sphere(Vec3(1.5f, 0.0f, 0.0f), 0.6f)));
    XG_CHECK(!box.Intersects(Sphere(Vec3(1.7f, 1.7f, 0.0f), 0.6f)));
    float tn, tf;
    XG_CHECK(box.Intersects(Ray(Vec3(-5.0f, 0.0f, 0.0f), Vec3(1.0f, 0.0f, 0.0f)), tn, tf));
    XG_CHECK_NEAR(tn, 4.0f, 1e-5f);

    // Frustum: D3D (0..1 depth) near plane and sphere straddling one plane but outside another
    Mat4x4 proj = Mat4x4::PerspectiveFovLH(HALF_PI, 1.0f, 1.0f, 100.0f);
    Frustum f = Frustum::FromMatrix(proj);
    XG_CHECK(f.Contains(Vec3(0.0f, 0.0f, 50.0f)) == Frustum::ContainmentType::Contains);
    XG_CHECK(f.Contains(Vec3(0.0f, 0.0f, 0.75f)) == Frustum::ContainmentType::Disjoint);   // in front of near
    XG_CHECK(f.Contains(Vec3(0.0f, 0.0f, 150.0f)) == Frustum::ContainmentType::Disjoint);  // past far
    XG_CHECK(f.Contains(Sphere(Vec3(-5.0f, 0.0f, 1.0f), 0.5f)) == Frustum::ContainmentType::Disjoint);
    XG_CHECK(f.Contains(Sphere(Vec3(0.0f, 0.0f, 50.0f), 1.0f)) == Frustum::ContainmentType::Contains);
    XG_CHECK(f.Contains(Sphere(Vec3(0.0f, 0.0f, 1.0f), 0.5f)) == Frustum::ContainmentType::Intersects);
    XG_CHECK(!f.Intersects(Sphere(Vec3(-5.0f, 0.0f, 1.0f), 0.5f)));
    XG_CHECK(f.Contains(AABB(Vec3(-1.0f, -1.0f, 10.0f), Vec3(1.0f, 1.0f, 12.0f))) == Frustum::ContainmentType::Contains);
    XG_CHECK(f.Contains(OBB(Vec3(0.0f, 0.0f, 20.0f), Vec3(1.0f), Quat::FromAxisAngle(Vec3::UnitY(), 0.7f))) == Frustum::ContainmentType::Contains);
    XG_CHECK(f.Contains(OBB(Vec3(-200.0f, 0.0f, 20.0f), Vec3(1.0f), Quat())) == Frustum::ContainmentType::Disjoint);
}

//============================================================
// Motion
//============================================================
static float SimulateDamp(float fps, float seconds)
{
    float x = 0.0f;
    float dt = 1.0f / fps;
    int steps = static_cast<int>(fps * seconds + 0.5f);
    for (int i = 0; i < steps; ++i)
        x = Damp(x, 10.0f, 5.0f, dt);
    return x;
}

static float SimulateSmoothDamp(float fps, float seconds, float& maxSeen)
{
    float x = 0.0f, v = 0.0f;
    float dt = 1.0f / fps;
    int steps = static_cast<int>(fps * seconds + 0.5f);
    maxSeen = 0.0f;
    for (int i = 0; i < steps; ++i)
    {
        x = SmoothDamp(x, 10.0f, v, 0.3f, dt);
        maxSeen = Max(maxSeen, x);
    }
    return x;
}

static void TestMotion()
{
    XG_CHECK_NEAR(InverseLerp(2.0f, 4.0f, 3.0f), 0.5f, 1e-6f);
    XG_CHECK_NEAR(Remap(5.0f, 0.0f, 10.0f, 100.0f, 200.0f), 150.0f, 1e-4f);
    XG_CHECK_NEAR(RemapClamped(20.0f, 0.0f, 10.0f, 100.0f, 200.0f), 200.0f, 1e-4f);
    XG_CHECK_NEAR(MoveTowards(0.0f, 10.0f, 3.0f), 3.0f, 1e-6f);

    // Angles
    XG_CHECK_NEAR(DeltaAngle(DegToRad(359.0f), DegToRad(1.0f)), DegToRad(2.0f), 1e-5f);
    XG_CHECK_NEAR(DeltaAngle(DegToRad(1.0f), DegToRad(359.0f)), DegToRad(-2.0f), 1e-5f);
    XG_CHECK_NEAR(WrapAngle(3.0f * PI), -PI, 1e-5f);
    XG_CHECK_NEAR(LerpAngle(DegToRad(350.0f), DegToRad(10.0f), 0.5f), DegToRad(360.0f), 1e-4f);
    XG_CHECK_NEAR(MoveTowardsAngle(0.0f, DegToRad(-90.0f), DegToRad(10.0f)), DegToRad(-10.0f), 1e-5f);

    // Damp is frame-rate independent
    float expected = 10.0f * (1.0f - expf(-5.0f));
    XG_CHECK_NEAR(SimulateDamp(30.0f, 1.0f), expected, 1e-3f);
    XG_CHECK_NEAR(SimulateDamp(60.0f, 1.0f), expected, 1e-3f);
    XG_CHECK_NEAR(SimulateDamp(144.0f, 1.0f), expected, 1e-3f);

    // SmoothDamp converges similarly at any frame rate and never overshoots
    float m30, m60, m144;
    float s30 = SimulateSmoothDamp(30.0f, 0.5f, m30);
    float s60 = SimulateSmoothDamp(60.0f, 0.5f, m60);
    float s144 = SimulateSmoothDamp(144.0f, 0.5f, m144);
    XG_CHECK_NEAR(s30, s144, 0.1f);
    XG_CHECK_NEAR(s60, s144, 0.05f);
    XG_CHECK(m30 <= 10.0f && m60 <= 10.0f && m144 <= 10.0f);
    SimulateSmoothDamp(60.0f, 5.0f, m60);
    XG_CHECK_NEAR(m60, 10.0f, 1e-3f);

    Vec3 vel;
    Vec3 p = Vec3::Zero();
    for (int i = 0; i < 300; ++i)
        p = SmoothDamp(p, Vec3(10.0f, 0.0f, 0.0f), vel, 0.3f, 1.0f / 60.0f);
    XG_CHECK_VEC3(p, Vec3(10.0f, 0.0f, 0.0f), 1e-3f);

    // Springs
    Spring crit;
    float critMax = 0.0f;
    for (int i = 0; i < 120; ++i)
    {
        crit.UpdateCritical(10.0f, 0.2f, 1.0f / 60.0f);
        critMax = Max(critMax, crit.Value);
    }
    XG_CHECK(critMax <= 10.0f + 1e-4f);
    XG_CHECK_NEAR(crit.Value, 10.0f, 1e-2f);

    Spring bouncy;
    float bouncyMax = 0.0f;
    for (int i = 0; i < 240; ++i)
    {
        bouncy.Update(10.0f, 2.0f, 0.2f, 1.0f / 60.0f);
        bouncyMax = Max(bouncyMax, bouncy.Value);
    }
    XG_CHECK(bouncyMax > 10.5f); // under-damped overshoots

    Spring stiff;
    for (int i = 0; i < 20; ++i)
        stiff.Update(10.0f, 50.0f, 1.0f, 1.0f); // huge dt stays stable
    XG_CHECK(stiff.Value == stiff.Value && Abs(stiff.Value - 10.0f) < 1.0f);

    SpringVec3 sv;
    for (int i = 0; i < 120; ++i)
        sv.UpdateCritical(Vec3(1.0f, 2.0f, 3.0f), 0.1f, 1.0f / 60.0f);
    XG_CHECK_VEC3(sv.Value, Vec3(1.0f, 2.0f, 3.0f), 1e-3f);

    // RotateTowards
    Quat q0 = Quat();
    Quat q1 = Quat::FromAxisAngle(Vec3::UnitY(), DegToRad(90.0f));
    Quat step = RotateTowards(q0, q1, DegToRad(30.0f));
    XG_CHECK_NEAR(q0.Angle(step), DegToRad(30.0f), 1e-4f);
    XG_CHECK(RotateTowards(q0, q1, DegToRad(120.0f)) == q1);

    Vec3 dir = RotateTowards(Vec3(2.0f, 0.0f, 0.0f), Vec3(0.0f, 0.0f, 4.0f), DegToRad(45.0f), 1.0f);
    XG_CHECK_NEAR(dir.Length(), 3.0f, 1e-4f);
    XG_CHECK_NEAR(acosf(dir.Normalized().Dot(Vec3::UnitX())), DegToRad(45.0f), 1e-4f);
    Vec3 opposite = RotateTowards(Vec3(1.0f, 0.0f, 0.0f), Vec3(-1.0f, 0.0f, 0.0f), DegToRad(90.0f), 0.0f);
    XG_CHECK_NEAR(opposite.Dot(Vec3::UnitX()), 0.0f, 1e-4f);
}

//============================================================
// Easing
//============================================================
static void TestEasing()
{
    for (int i = 0; i < static_cast<int>(EaseType::Count); ++i)
    {
        EaseType e = static_cast<EaseType>(i);
        XG_CHECK_NEAR(Ease(e, 0.0f), 0.0f, 1e-5f);
        XG_CHECK_NEAR(Ease(e, 1.0f), 1.0f, 1e-5f);
    }

    // Monotonic curves (everything except Back / Elastic / Bounce)
    const EaseType monotonic[] = {
        EaseType::Linear, EaseType::InQuad, EaseType::OutQuad, EaseType::InOutQuad,
        EaseType::InCubic, EaseType::OutCubic, EaseType::InOutCubic, EaseType::InQuart,
        EaseType::OutQuart, EaseType::InOutQuart, EaseType::InQuint, EaseType::OutQuint,
        EaseType::InOutQuint, EaseType::InSine, EaseType::OutSine, EaseType::InOutSine,
        EaseType::InExpo, EaseType::OutExpo, EaseType::InOutExpo, EaseType::InCirc,
        EaseType::OutCirc, EaseType::InOutCirc };
    for (EaseType e : monotonic)
    {
        float prev = 0.0f;
        bool ok = true;
        for (int s = 1; s <= 100; ++s)
        {
            float v = Ease(e, s / 100.0f);
            if (v < prev - 1e-6f) ok = false;
            prev = v;
        }
        XG_CHECK(ok);
    }

    XG_CHECK(ease::InBack(0.2f) < 0.0f);       // anticipation
    XG_CHECK(ease::OutBack(0.8f) > 1.0f);      // overshoot
    XG_CHECK_NEAR(ease::InOutQuad(0.5f), 0.5f, 1e-6f);
    XG_CHECK_NEAR(Tween(10.0f, 20.0f, 0.5f, EaseType::Linear), 15.0f, 1e-5f);

    // GOLDEN
    XG_CHECK_NEAR(Ease(EaseType::OutBounce, 0.3f), 0.680625081f, 1e-6f);
    XG_CHECK_NEAR(Ease(EaseType::InOutBack, 0.7f), 1.07883346f, 1e-6f);
    XG_CHECK_NEAR(Ease(EaseType::OutElastic, 0.2f), 1.125f, 1e-5f);
}

//============================================================
// Golden values shared with the managed suite
//============================================================
static void TestGolden()
{
    // GOLDEN
    Pcg32 rng(42u, 54u);
    for (int i = 0; i < 6; ++i)
        rng.NextUInt();
    const uint32_t bounded[6] = { 74u, 50u, 89u, 97u, 19u, 11u };
    for (uint32_t expectedValue : bounded)
        XG_CHECK(rng.NextUInt(100u) == expectedValue);
    XG_CHECK_NEAR(rng.NextFloat(), 0.927618504f, 1e-7f);
    XG_CHECK_NEAR(rng.NextFloat(), 0.219272852f, 1e-7f);
    XG_CHECK_NEAR(rng.NextFloat(), 0.170426548f, 1e-7f);

    XG_CHECK(Hash32(0u) == 0x07bb2fe2u);
    XG_CHECK(Hash32(1u) == 0xa8beea3cu);
    XG_CHECK(Hash32(2u) == 0x7a7ecc88u);
    XG_CHECK(Hash(3, -7, 11, 99u) == 0xbb51bf37u);

    float v = 0.0f;
    XG_CHECK_NEAR(SmoothDamp(0.0f, 10.0f, v, 0.3f, 1.0f / 60.0f), 0.0559206009f, 1e-6f);
    XG_CHECK_NEAR(v, 6.6293869f, 1e-5f);

    Spring s;
    s.UpdateCritical(10.0f, 0.2f, 0.1f);
    XG_CHECK_NEAR(s.Value, 1.5341444f, 1e-5f);
    XG_CHECK_NEAR(s.Velocity, 24.0210762f, 1e-4f);

    Color hsv = Color::FromHSV(0.3f, 0.6f, 0.8f);
    XG_CHECK_NEAR(hsv.r, 0.415999979f, 1e-6f);
    XG_CHECK_NEAR(hsv.g, 0.800000012f, 1e-6f);
    XG_CHECK_NEAR(hsv.b, 0.319999993f, 1e-6f);
}

//============================================================
// Random
//============================================================
static void TestRandom()
{
    // GOLDEN: reference PCG32 output (pcg32-demo, seed 42 / stream 54)
    Pcg32 rng(42u, 54u);
    const uint32_t reference[6] = { 0xa15c02b7u, 0x7b47f409u, 0xba1d3330u, 0x83d2f293u, 0xbfa4784bu, 0xcbed606eu };
    for (uint32_t expectedValue : reference)
        XG_CHECK(rng.NextUInt() == expectedValue);

    Pcg32 a(7u), b(7u), c(8u);
    XG_CHECK(a.NextUInt() == b.NextUInt());
    XG_CHECK(a.NextUInt() != c.NextUInt());

    Pcg32 r(123u);
    bool boundedOk = true, rangeOk = true, floatOk = true, sphereOk = true, coneOk = true, triOk = true;
    int counts[4] = {};
    for (int i = 0; i < 10000; ++i)
    {
        if (r.NextUInt(7u) >= 7u) boundedOk = false;
        int v = r.Range(-3, 3);
        if (v < -3 || v >= 3) rangeOk = false;
        float fv = r.NextFloat();
        if (fv < 0.0f || fv >= 1.0f) floatOk = false;
        if (Abs(r.OnUnitSphere().Length() - 1.0f) > 1e-4f) sphereOk = false;
        Vec3 d = r.InCone(Vec3::UnitZ(), DegToRad(20.0f));
        if (acosf(Clamp(d.Dot(Vec3::UnitZ()), -1.0f, 1.0f)) > DegToRad(20.0f) + 1e-3f) coneOk = false;
        Vec3 pt = r.InTriangle(Vec3(0.0f), Vec3(1.0f, 0.0f, 0.0f), Vec3(0.0f, 1.0f, 0.0f));
        if (pt.x < -1e-6f || pt.y < -1e-6f || pt.x + pt.y > 1.0f + 1e-5f) triOk = false;
        const float weights[4] = { 1.0f, 0.0f, 3.0f, -2.0f };
        counts[r.WeightedIndex(weights, 4)]++;
    }
    XG_CHECK(boundedOk);
    XG_CHECK(rangeOk);
    XG_CHECK(floatOk);
    XG_CHECK(sphereOk);
    XG_CHECK(coneOk);
    XG_CHECK(triOk);
    XG_CHECK(counts[1] == 0 && counts[3] == 0);
    XG_CHECK(counts[2] > 2 * counts[0]);
    XG_CHECK(r.WeightedIndex(nullptr, 0) == -1);

    XG_CHECK_NEAR(r.Rotation().Length(), 1.0f, 1e-4f);

    int items[10] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9 };
    r.Shuffle(items, 10);
    int sum = 0, mask = 0;
    for (int it : items) { sum += it; mask |= 1 << it; }
    XG_CHECK(sum == 45 && mask == 0x3FF);

    double mean = 0.0;
    for (int i = 0; i < 20000; ++i)
        mean += r.Gaussian(5.0f, 2.0f);
    XG_CHECK_NEAR(mean / 20000.0, 5.0, 0.1);

    XG_CHECK(Hash(1, 2, 3) == Hash(1, 2, 3));
    XG_CHECK(Hash(1, 2, 3) != Hash(1, 2, 4));
    XG_CHECK(HashToFloat(0xFFFFFFFFu) < 1.0f);
}

//============================================================
// Integer vectors, color, rect
//============================================================
static void TestValueTypes()
{
    XG_CHECK(FloorDiv(-1, 16) == -1);
    XG_CHECK(FloorMod(-1, 16) == 15);
    XG_CHECK(FloorDiv(16, 16) == 1);
    XG_CHECK(FloorMod(-17, 16) == 15);
    XG_CHECK(Vec2Int(-1, 33).FloorDiv(16) == Vec2Int(-1, 2));
    XG_CHECK(Vec3Int::ManhattanDistance(Vec3Int(0, 0, 0), Vec3Int(1, -2, 3)) == 6);
    XG_CHECK(Vec3Int::ChebyshevDistance(Vec3Int(0, 0, 0), Vec3Int(1, -2, 3)) == 3);
    XG_CHECK(FloorToVec2Int(Vec2(-0.5f, 1.5f)) == Vec2Int(-1, 1));
    XG_CHECK(Vec3Int::RoundToInt(Vec3(1.5f, -1.5f, 0.4f)) == Vec3Int(2, -2, 0));

    // Color
    Color c(0.2f, 0.5f, 0.9f, 0.5f);
    XG_CHECK(c.ToSRGB().ToLinear().IsApproxEqual(c, 1e-5f));
    float h, s, v;
    c.ToHSV(h, s, v);
    XG_CHECK(Color::FromHSV(h, s, v, c.a).IsApproxEqual(c, 1e-5f));
    XG_CHECK(Color::FromHSV(0.0f, 1.0f, 1.0f) == Color::Red());
    Color32 c32(10, 20, 30, 40);
    XG_CHECK(c32.PackRGBA() == 0x0A141E28u);
    XG_CHECK(c32.PackABGR() == 0x281E140Au);
    XG_CHECK(Color32::UnpackRGBA(c32.PackRGBA()) == c32);
    XG_CHECK(Color32::UnpackABGR(c32.PackABGR()) == c32);
    XG_CHECK(Color::FromColor32(c32).ToColor32() == c32);
    XG_CHECK(Color(1.5f, -1.0f, 0.5f, 1.0f).ToColor32() == Color32(255, 0, 128, 255));
    XG_CHECK_NEAR(Color::White().Luminance(), 1.0f, 1e-5f);

    // Rect
    Rect r1 = Rect::FromXYWH(0.0f, 0.0f, 10.0f, 10.0f);
    Rect r2 = Rect::FromXYWH(5.0f, 5.0f, 10.0f, 10.0f);
    XG_CHECK(r1.Overlaps(r2));
    XG_CHECK(r1.Intersection(r2) == Rect::FromXYWH(5.0f, 5.0f, 5.0f, 5.0f));
    XG_CHECK(!r1.Intersection(Rect::FromXYWH(20.0f, 20.0f, 1.0f, 1.0f)).IsValid());
    XG_CHECK(r1.NormalizedToPoint(Vec2(0.25f, 0.5f)) == Vec2(2.5f, 5.0f));
    XG_CHECK(r1.PointToNormalized(Vec2(2.5f, 5.0f)) == Vec2(0.25f, 0.5f));
    RectInt ri = RectInt::FromXYWH(0, 0, 4, 4);
    XG_CHECK(ri.Contains(Vec2Int(3, 3)) && !ri.Contains(Vec2Int(4, 0)));
    XG_CHECK(!ri.Overlaps(RectInt::FromXYWH(4, 0, 2, 2)));
}

//============================================================
// Matrices
//============================================================
static void TestMatrices()
{
    Quat q = Quat::FromAxisAngle(Vec3(1.0f, 2.0f, 3.0f), 0.8f);
    Mat3x3 rot = Mat3x3::FromQuat(q);
    Vec3 v(0.3f, -1.2f, 2.0f);
    XG_CHECK_VEC3(rot.Transform(v), q.RotateVector(v), 1e-5f);
    XG_CHECK_VEC3(rot.Transform(v), Mat4x4::RotationQuaternion(q).TransformDirection(v), 1e-5f);
    XG_CHECK(rot.ToQuat().IsApproxEqual(q, 1e-5f) || rot.ToQuat().IsApproxEqual(-q, 1e-5f));
    XG_CHECK_NEAR(rot.Determinant(), 1.0f, 1e-5f);

    Mat3x3 m(2.0f, 1.0f, 0.0f, 0.0f, 3.0f, 1.0f, 1.0f, 0.0f, 4.0f);
    XG_CHECK((m * m.Inverse()).IsApproxEqual(Mat3x3::Identity(), 1e-5f));
    Mat3x3 singular = Mat3x3::Zero();
    Mat3x3 out;
    XG_CHECK(!singular.Inverse(out));

    Vec3 a(1.0f, 2.0f, 3.0f), b(-2.0f, 0.5f, 4.0f);
    XG_CHECK_VEC3(Mat3x3::Skew(a).Transform(b), a.Cross(b), 1e-5f);
    XG_CHECK_NEAR(Mat3x3::OuterProduct(a, b).m[1][2], a.y * b.z, 1e-6f);

    // Normal matrix keeps normals perpendicular under non-uniform scale
    Mat4x4 world = Mat4x4::Scaling(1.0f, 4.0f, 1.0f);
    Vec3 tangent = Vec3(1.0f, 1.0f, 0.0f);
    Vec3 normal = Vec3(1.0f, -1.0f, 0.0f);
    Vec3 tw = world.TransformDirection(tangent);
    Vec3 nw = Mat3x3::NormalMatrix(world).Transform(normal);
    XG_CHECK_NEAR(tw.Dot(nw), 0.0f, 1e-5f);

    // Mat3x2 / Transform2D
    Mat3x2 trs = Mat3x2::TRS(Vec2(3.0f, -2.0f), 0.6f, Vec2(2.0f, 0.5f));
    XG_CHECK((trs * trs.Inverse()).IsApproxEqual(Mat3x2::Identity(), 1e-5f));
    Vec2 sc, tr;
    float ro;
    trs.Decompose(sc, ro, tr);
    XG_CHECK_NEAR(sc.x, 2.0f, 1e-5f);
    XG_CHECK_NEAR(sc.y, 0.5f, 1e-5f);
    XG_CHECK_NEAR(ro, 0.6f, 1e-5f);
    XG_CHECK_NEAR(tr.x, 3.0f, 1e-5f);
    XG_CHECK(Mat3x2::Rotation(HALF_PI).TransformPoint(Vec2(1.0f, 0.0f)).y > 0.999f); // CCW

    Transform2D parent(Vec2(10.0f, 0.0f), HALF_PI, Vec2(2.0f, 2.0f));
    Transform2D child(Vec2(1.0f, 0.0f));
    Vec2 viaCombine = parent.Combine(child).TransformPoint(Vec2(0.0f, 0.0f));
    Vec2 viaChain = parent.TransformPoint(child.TransformPoint(Vec2(0.0f, 0.0f)));
    XG_CHECK_NEAR(viaCombine.x, viaChain.x, 1e-4f);
    XG_CHECK_NEAR(viaCombine.y, viaChain.y, 1e-4f);
    XG_CHECK_NEAR(viaChain.y, 2.0f, 1e-4f);
    Vec2 back = parent.InverseTransformPoint(parent.TransformPoint(Vec2(3.0f, 4.0f)));
    XG_CHECK_NEAR(back.x, 3.0f, 1e-4f);
    XG_CHECK_NEAR(back.y, 4.0f, 1e-4f);
}

//============================================================
// Geometry
//============================================================
static void TestGeometry()
{
    // Closest points
    float t;
    XG_CHECK_VEC3(ClosestPointOnSegment(Vec3(0.5f, 1.0f, 0.0f), Vec3(0.0f), Vec3(1.0f, 0.0f, 0.0f), &t), Vec3(0.5f, 0.0f, 0.0f), 1e-6f);
    XG_CHECK_NEAR(t, 0.5f, 1e-6f);
    XG_CHECK_VEC3(ClosestPointOnSegment(Vec3(-3.0f, 0.0f, 0.0f), Vec3(0.0f), Vec3(1.0f, 0.0f, 0.0f)), Vec3(0.0f), 1e-6f);

    float s, u;
    Vec3 c1, c2;
    float d2 = ClosestPointsSegmentSegment(Vec3(-1.0f, 0.0f, 0.0f), Vec3(1.0f, 0.0f, 0.0f),
        Vec3(0.0f, -1.0f, 2.0f), Vec3(0.0f, 1.0f, 2.0f), s, u, c1, c2);
    XG_CHECK_NEAR(d2, 4.0f, 1e-5f);
    XG_CHECK_NEAR(s, 0.5f, 1e-5f);
    XG_CHECK_NEAR(u, 0.5f, 1e-5f);

    Triangle tri(Vec3(0.0f), Vec3(1.0f, 0.0f, 0.0f), Vec3(0.0f, 1.0f, 0.0f));
    XG_CHECK_VEC3(ClosestPointOnTriangle(Vec3(0.2f, 0.2f, 5.0f), tri), Vec3(0.2f, 0.2f, 0.0f), 1e-6f);   // face
    XG_CHECK_VEC3(ClosestPointOnTriangle(Vec3(-1.0f, -1.0f, 0.0f), tri), Vec3(0.0f), 1e-6f);             // vertex
    XG_CHECK_VEC3(ClosestPointOnTriangle(Vec3(1.0f, 1.0f, 0.0f), tri), Vec3(0.5f, 0.5f, 0.0f), 1e-6f);   // edge
    Vec3 bary = tri.Barycentric(Vec3(0.25f, 0.25f, 0.0f));
    XG_CHECK_VEC3(bary, Vec3(0.5f, 0.25f, 0.25f), 1e-6f);

    OBB rotated(Vec3(0.0f), Vec3(1.0f), Quat::FromAxisAngle(Vec3::UnitZ(), QUARTER_PI));
    Vec3 q = ClosestPointOnOBB(Vec3(5.0f, 0.0f, 0.0f), rotated);
    XG_CHECK_NEAR(q.x, constants::SQRT_2, 1e-5f); // corner points along +x after 45 degree rotation
    XG_CHECK(rotated.Contains(Vec3(1.3f, 0.0f, 0.0f)));
    XG_CHECK(!rotated.Contains(Vec3(1.0f, 1.0f, 0.0f)));
    AABB rb = rotated.ToAABB();
    XG_CHECK_NEAR(rb.max.x, constants::SQRT_2, 1e-5f);

    Capsule cap(Vec3(0.0f, -1.0f, 0.0f), Vec3(0.0f, 1.0f, 0.0f), 0.5f);
    XG_CHECK_VEC3(ClosestPointOnCapsule(Vec3(3.0f, 0.0f, 0.0f), cap), Vec3(0.5f, 0.0f, 0.0f), 1e-6f);
    XG_CHECK_VEC3(ClosestPointOnCapsule(Vec3(0.1f, 0.0f, 0.0f), cap), Vec3(0.1f, 0.0f, 0.0f), 1e-6f);

    // Overlaps
    XG_CHECK(Intersects(Sphere(Vec3(0.9f, 0.0f, 0.0f), 0.5f), cap));
    XG_CHECK(!Intersects(Sphere(Vec3(1.1f, 0.0f, 0.0f), 0.5f), cap));
    XG_CHECK(Intersects(cap, Capsule(Vec3(0.9f, 0.0f, -1.0f), Vec3(0.9f, 0.0f, 1.0f), 0.5f)));
    XG_CHECK(!Intersects(cap, Capsule(Vec3(1.1f, 0.0f, -1.0f), Vec3(1.1f, 0.0f, 1.0f), 0.5f)));
    XG_CHECK(Intersects(Sphere(Vec3(0.1f, 0.1f, 0.4f), 0.5f), tri));
    XG_CHECK(!Intersects(Sphere(Vec3(0.1f, 0.1f, 0.6f), 0.5f), tri));

    // OBB vs OBB separating axis test
    OBB unit(Vec3(0.0f), Vec3(1.0f), Quat());
    XG_CHECK(Intersects(unit, OBB(Vec3(1.9f, 0.0f, 0.0f), Vec3(1.0f), Quat())));                  // overlapping
    XG_CHECK(Intersects(unit, OBB(Vec3(2.0f, 0.0f, 0.0f), Vec3(1.0f), Quat())));                  // touching
    XG_CHECK(!Intersects(unit, OBB(Vec3(2.1f, 0.0f, 0.0f), Vec3(1.0f), Quat())));                 // separated
    Quat r45 = Quat::FromAxisAngle(Vec3::UnitZ(), QUARTER_PI);
    OBB diag(Vec3(1.9f, 1.9f, 0.0f), Vec3(1.0f), r45);
    XG_CHECK(unit.ToAABB().Intersects(diag.ToAABB()));                                             // AABBs overlap...
    XG_CHECK(!Intersects(unit, diag));                                                             // ...but the boxes don't
    XG_CHECK(Intersects(unit, OBB(Vec3(2.3f, 0.0f, 0.0f), Vec3(1.0f), r45)));                     // rotated corner pokes in
    XG_CHECK(!Intersects(AABB(Vec3(-1.0f), Vec3(1.0f)), diag));
    XG_CHECK(Intersects(Sphere(Vec3(1.8f, 0.0f, 0.0f), 0.5f), rotated));

    // Plane classification
    Plane ground = Plane::FromPointNormal(Vec3(0.0f), Vec3::UnitY());
    XG_CHECK(Classify(ground, Sphere(Vec3(0.0f, 2.0f, 0.0f), 1.0f)) == PlaneSide::Front);
    XG_CHECK(Classify(ground, Sphere(Vec3(0.0f, -2.0f, 0.0f), 1.0f)) == PlaneSide::Back);
    XG_CHECK(Classify(ground, AABB(Vec3(-1.0f), Vec3(1.0f))) == PlaneSide::Intersecting);
    XG_CHECK(Classify(ground, OBB(Vec3(0.0f, 1.3f, 0.0f), Vec3(1.0f), r45)) == PlaneSide::Intersecting);
    XG_CHECK(Classify(ground, OBB(Vec3(0.0f, 1.5f, 0.0f), Vec3(1.0f), r45)) == PlaneSide::Front);
    XG_CHECK_NEAR(Plane(Vec3(0.0f, 2.0f, 0.0f), -4.0f).Distance(Vec3(0.0f, 5.0f, 0.0f)), 3.0f, 1e-6f);

    // Ray casts
    float hitT;
    XG_CHECK(Raycast(Ray(Vec3(-5.0f, 0.0f, 0.0f), Vec3::UnitX()), rotated, hitT));
    XG_CHECK_NEAR(hitT, 5.0f - constants::SQRT_2, 1e-4f);
    XG_CHECK(!Raycast(Ray(Vec3(-5.0f, 3.0f, 0.0f), Vec3::UnitX()), rotated, hitT));
    XG_CHECK(Raycast(Ray(Vec3(-5.0f, 0.0f, 0.0f), Vec3::UnitX()), cap, hitT));
    XG_CHECK_NEAR(hitT, 4.5f, 1e-4f);
    XG_CHECK(Raycast(Ray(Vec3(0.0f, 5.0f, 0.0f), -Vec3::UnitY()), cap, hitT));
    XG_CHECK_NEAR(hitT, 3.5f, 1e-4f);
    XG_CHECK(Raycast(Ray(Vec3(0.0f, 0.0f, 0.0f), Vec3::UnitX()), cap, hitT));
    XG_CHECK_NEAR(hitT, 0.0f, 1e-6f);
    XG_CHECK(!Raycast(Ray(Vec3(-5.0f, 0.0f, 0.0f), -Vec3::UnitX()), cap, hitT));

    // Sweeps
    SweepHit hit;
    XG_CHECK(Sweep(Sphere(Vec3(0.0f), 1.0f), Vec3(10.0f, 0.0f, 0.0f), Sphere(Vec3(5.0f, 0.0f, 0.0f), 1.0f), hit));
    XG_CHECK_NEAR(hit.t, 0.3f, 1e-5f);
    XG_CHECK_VEC3(hit.normal, Vec3(-1.0f, 0.0f, 0.0f), 1e-5f);
    XG_CHECK_VEC3(hit.point, Vec3(4.0f, 0.0f, 0.0f), 1e-5f);
    XG_CHECK(!Sweep(Sphere(Vec3(0.0f), 1.0f), Vec3(2.0f, 0.0f, 0.0f), Sphere(Vec3(5.0f, 0.0f, 0.0f), 1.0f), hit));
    XG_CHECK(!Sweep(Sphere(Vec3(0.0f), 1.0f), Vec3(-10.0f, 0.0f, 0.0f), Sphere(Vec3(5.0f, 0.0f, 0.0f), 1.0f), hit));

    XG_CHECK(Sweep(Sphere(Vec3(0.0f, 5.0f, 0.0f), 1.0f), Vec3(0.0f, -10.0f, 0.0f), ground, hit));
    XG_CHECK_NEAR(hit.t, 0.4f, 1e-5f);
    XG_CHECK_VEC3(hit.point, Vec3(0.0f), 1e-5f);
    XG_CHECK(!Sweep(Sphere(Vec3(0.0f, 5.0f, 0.0f), 1.0f), Vec3(0.0f, 10.0f, 0.0f), ground, hit));

    AABB cube(Vec3(-1.0f), Vec3(1.0f));
    XG_CHECK(Sweep(Sphere(Vec3(5.0f, 0.0f, 0.0f), 0.5f), Vec3(-10.0f, 0.0f, 0.0f), cube, hit));     // face
    XG_CHECK_NEAR(hit.t, 0.35f, 1e-5f);
    XG_CHECK_VEC3(hit.normal, Vec3(1.0f, 0.0f, 0.0f), 1e-5f);
    XG_CHECK(Sweep(Sphere(Vec3(5.0f, 5.0f, 0.0f), 0.5f), Vec3(-10.0f, -10.0f, 0.0f), cube, hit));   // edge
    XG_CHECK_NEAR(hit.t, (5.0f - (1.0f + 0.5f * constants::INV_SQRT_2)) / 10.0f, 1e-4f);
    XG_CHECK(Sweep(Sphere(Vec3(5.0f, 5.0f, 5.0f), 0.5f), Vec3(-10.0f, -10.0f, -10.0f), cube, hit)); // vertex
    XG_CHECK_NEAR(hit.t, (5.0f - (1.0f + 0.5f * constants::INV_SQRT_3)) / 10.0f, 1e-4f);
    XG_CHECK(!Sweep(Sphere(Vec3(5.0f, 3.0f, 0.0f), 0.5f), Vec3(-10.0f, 0.0f, 0.0f), cube, hit));    // misses

    XG_CHECK(Sweep(AABB(Vec3(0.0f), Vec3(1.0f)), Vec3(10.0f, 0.0f, 0.0f), AABB(Vec3(5.0f, 0.0f, 0.0f), Vec3(6.0f, 1.0f, 1.0f)), hit));
    XG_CHECK_NEAR(hit.t, 0.4f, 1e-5f);
    XG_CHECK_VEC3(hit.normal, Vec3(-1.0f, 0.0f, 0.0f), 1e-6f);
    XG_CHECK(!Sweep(AABB(Vec3(0.0f), Vec3(1.0f)), Vec3(10.0f, 0.0f, 0.0f), AABB(Vec3(5.0f, 2.0f, 0.0f), Vec3(6.0f, 3.0f, 1.0f)), hit));

    // 2D
    Vec2 ip;
    XG_CHECK(SegmentIntersect2D(Vec2(0.0f, 0.0f), Vec2(1.0f, 1.0f), Vec2(0.0f, 1.0f), Vec2(1.0f, 0.0f), &ip));
    XG_CHECK_NEAR(ip.x, 0.5f, 1e-6f);
    XG_CHECK(!SegmentIntersect2D(Vec2(0.0f, 0.0f), Vec2(1.0f, 0.0f), Vec2(0.0f, 1.0f), Vec2(1.0f, 1.0f)));
    const Vec2 uShape[] = { Vec2(0.0f, 0.0f), Vec2(3.0f, 0.0f), Vec2(3.0f, 3.0f), Vec2(2.0f, 3.0f),
                            Vec2(2.0f, 1.0f), Vec2(1.0f, 1.0f), Vec2(1.0f, 3.0f), Vec2(0.0f, 3.0f) };
    XG_CHECK(PointInPolygon(uShape, 8, Vec2(0.5f, 2.0f)));
    XG_CHECK(!PointInPolygon(uShape, 8, Vec2(1.5f, 2.0f))); // inside the notch
    XG_CHECK(PointInPolygon(uShape, 8, Vec2(1.5f, 0.5f)));
    XG_CHECK(PointInTriangle2D(Vec2(0.2f, 0.2f), Vec2(0.0f, 0.0f), Vec2(1.0f, 0.0f), Vec2(0.0f, 1.0f)));
    XG_CHECK(!PointInTriangle2D(Vec2(0.8f, 0.8f), Vec2(0.0f, 0.0f), Vec2(1.0f, 0.0f), Vec2(0.0f, 1.0f)));
    XG_CHECK(Intersects(Circle(Vec2(0.0f, 0.0f), 1.0f), Circle(Vec2(1.9f, 0.0f), 1.0f)));
    XG_CHECK(Intersects(Circle(Vec2(-0.5f, 5.0f), 1.0f), Rect::FromXYWH(0.0f, 0.0f, 10.0f, 10.0f)));
    XG_CHECK(!Intersects(Circle(Vec2(-1.5f, 5.0f), 1.0f), Rect::FromXYWH(0.0f, 0.0f, 10.0f, 10.0f)));
}

int main(int argc, char** argv)
{
    if (argc > 1 && strcmp(argv[1], "--golden") == 0)
    {
        PrintGolden();
        return 0;
    }

    TestExistingFixes();
    TestMotion();
    TestEasing();
    TestGolden();
    TestRandom();
    TestValueTypes();
    TestMatrices();
    TestGeometry();

    printf("%d checks, %d failures\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
