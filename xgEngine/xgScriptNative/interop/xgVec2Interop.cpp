#include "pch.h"
#include "xgVec2Interop.h"

namespace xg
{
    Vec2 XG_Vec2_Add(const Vec2* a, const Vec2* b)
    {
        return *a + *b;
    }

    Vec2 XG_Vec2_Sub(const Vec2* a, const Vec2* b)
    {
        return *a - *b;
    }

    Vec2 XG_Vec2_MulScalar(const Vec2* v, float s)
    {
        return *v * s;
    }

    Vec2 XG_Vec2_DivScalar(const Vec2* v, float s)
    {
        return *v / s;
    }

    float XG_Vec2_Length(const Vec2* v)
    {
        return v->Length();
    }

    float XG_Vec2_LengthSquared(const Vec2* v)
    {
        return v->LengthSq();
    }

    Vec2 XG_Vec2_Normalized(const Vec2* v)
    {
        return v->Normalized();
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

    Vec2 XG_Vec2_Lerp(const Vec2* a, const Vec2* b, float t)
    {
        return Vec2::Lerp(*a, *b, t);
    }

    Vec2 XG_Vec2_Min(const Vec2* a, const Vec2* b)
    {
        return Vec2::Min(*a, *b);
    }

    Vec2 XG_Vec2_Max(const Vec2* a, const Vec2* b)
    {
        return Vec2::Max(*a, *b);
    }

    Vec2 XG_Vec2_Clamp(const Vec2* v, const Vec2* min, const Vec2* max)
    {
        return Vec2::Clamp(*v, *min, *max);
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
