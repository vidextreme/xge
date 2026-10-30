#include "pch.h"
#include "xgVec3Interop.h"

namespace xg
{
    using interop::Load;

    interop::Vec3Abi XG_Vec3_Add(const Vec3* a, const Vec3* b)
    {
        return interop::ToAbi(Load(a) + Load(b));
    }

    interop::Vec3Abi XG_Vec3_Sub(const Vec3* a, const Vec3* b)
    {
        return interop::ToAbi(Load(a) - Load(b));
    }

    interop::Vec3Abi XG_Vec3_MulScalar(const Vec3* v, float s)
    {
        return interop::ToAbi(Load(v) * s);
    }

    interop::Vec3Abi XG_Vec3_DivScalar(const Vec3* v, float s)
    {
        return interop::ToAbi(Load(v) / s);
    }

    float XG_Vec3_Length(const Vec3* v)
    {
        return Load(v).Length();
    }

    float XG_Vec3_LengthSquared(const Vec3* v)
    {
        return Load(v).LengthSq();
    }

    interop::Vec3Abi XG_Vec3_Normalized(const Vec3* v)
    {
        return interop::ToAbi(Load(v).Normalized());
    }

    float XG_Vec3_Dot(const Vec3* a, const Vec3* b)
    {
        return Load(a).Dot(Load(b));
    }

    interop::Vec3Abi XG_Vec3_Cross(const Vec3* a, const Vec3* b)
    {
        return interop::ToAbi(Load(a).Cross(Load(b)));
    }

    float XG_Vec3_Distance(const Vec3* a, const Vec3* b)
    {
        return Load(a).Distance(Load(b));
    }

    float XG_Vec3_DistanceSquared(const Vec3* a, const Vec3* b)
    {
        return Load(a).DistanceSq(Load(b));
    }

    interop::Vec3Abi XG_Vec3_Lerp(const Vec3* a, const Vec3* b, float t)
    {
        return interop::ToAbi(Vec3::Lerp(Load(a), Load(b), t));
    }

    interop::Vec3Abi XG_Vec3_Min(const Vec3* a, const Vec3* b)
    {
        return interop::ToAbi(Vec3::Min(Load(a), Load(b)));
    }

    interop::Vec3Abi XG_Vec3_Max(const Vec3* a, const Vec3* b)
    {
        return interop::ToAbi(Vec3::Max(Load(a), Load(b)));
    }

    interop::Vec3Abi XG_Vec3_Clamp(const Vec3* v, const Vec3* min, const Vec3* max)
    {
        return interop::ToAbi(Vec3::Clamp(Load(v), Load(min), Load(max)));
    }

    interop::Vec3Abi XG_Vec3_Transform(const Vec3* v, const Mat4x4* m)
    {
        return interop::ToAbi(Load(v).Transform(Load(m)));
    }

    interop::Vec3Abi XG_Vec3_TransformDirection(const Vec3* v, const Mat4x4* m)
    {
        return interop::ToAbi(Load(v).TransformDirection(Load(m)));
    }

    bool XG_Vec3_IsZero(const Vec3* v)
    {
        return Load(v).IsZero();
    }

    bool XG_Vec3_IsEqual(const Vec3* a, const Vec3* b, float epsilon)
    {
        return Load(a).IsEqual(Load(b), epsilon);
    }
}
