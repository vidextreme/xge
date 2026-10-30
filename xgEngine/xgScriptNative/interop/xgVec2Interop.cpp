#include "pch.h"
#include "xgVec2Interop.h"

namespace xg
{
    interop::Vec2Abi XG_Vec2_Add(const Vec2* a, const Vec2* b)
    {
        return interop::ToAbi(*a + *b);
    }

    interop::Vec2Abi XG_Vec2_Sub(const Vec2* a, const Vec2* b)
    {
        return interop::ToAbi(*a - *b);
    }

    interop::Vec2Abi XG_Vec2_MulScalar(const Vec2* v, float s)
    {
        return interop::ToAbi(*v * s);
    }

    interop::Vec2Abi XG_Vec2_DivScalar(const Vec2* v, float s)
    {
        return interop::ToAbi(*v / s);
    }

    float XG_Vec2_Length(const Vec2* v)
    {
        return v->Length();
    }

    float XG_Vec2_LengthSquared(const Vec2* v)
    {
        return v->LengthSq();
    }

    interop::Vec2Abi XG_Vec2_Normalized(const Vec2* v)
    {
        return interop::ToAbi(v->Normalized());
    }

    float XG_Vec2_Dot(const Vec2* a, const Vec2* b)
    {
        return a->Dot(*b);
    }

    float XG_Vec2_Distance(const Vec2* a, const Vec2* b)
    {
        return a->Distance(*b);
    }

    float XG_Vec2_DistanceSquared(const Vec2* a, const Vec2* b)
    {
        return a->DistanceSq(*b);
    }

    interop::Vec2Abi XG_Vec2_Lerp(const Vec2* a, const Vec2* b, float t)
    {
        return interop::ToAbi(Vec2::Lerp(*a, *b, t));
    }

    interop::Vec2Abi XG_Vec2_Min(const Vec2* a, const Vec2* b)
    {
        return interop::ToAbi(Vec2::Min(*a, *b));
    }

    interop::Vec2Abi XG_Vec2_Max(const Vec2* a, const Vec2* b)
    {
        return interop::ToAbi(Vec2::Max(*a, *b));
    }

    interop::Vec2Abi XG_Vec2_Clamp(const Vec2* v, const Vec2* min, const Vec2* max)
    {
        return interop::ToAbi(Vec2::Clamp(*v, *min, *max));
    }

    bool XG_Vec2_IsZero(const Vec2* v)
    {
        return v->IsZero();
    }

    bool XG_Vec2_IsEqual(const Vec2* a, const Vec2* b, float epsilon)
    {
        return a->IsEqual(*b, epsilon);
    }
}
