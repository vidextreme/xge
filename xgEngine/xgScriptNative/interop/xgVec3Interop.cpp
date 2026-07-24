#include "pch.h"
#include "xgVec3Interop.h"

namespace xg
{
    Vec3 XG_Vec3_Add(const Vec3* a, const Vec3* b)
    {
        return *a + *b;
    }

    Vec3 XG_Vec3_Sub(const Vec3* a, const Vec3* b)
    {
        return *a - *b;
    }

    Vec3 XG_Vec3_MulScalar(const Vec3* v, float s)
    {
        return *v * s;
    }

    Vec3 XG_Vec3_DivScalar(const Vec3* v, float s)
    {
        return *v / s;
    }

    float XG_Vec3_Length(const Vec3* v)
    {
        return v->Length();
    }

    float XG_Vec3_LengthSquared(const Vec3* v)
    {
        return v->LengthSq();
    }

    Vec3 XG_Vec3_Normalized(const Vec3* v)
    {
        return v->Normalized();
    }

    float XG_Vec3_Dot(const Vec3* a, const Vec3* b)
    {
        return a->Dot(*b);
    }

    Vec3 XG_Vec3_Cross(const Vec3* a, const Vec3* b)
    {
        return a->Cross(*b);
    }

    float XG_Vec3_Distance(const Vec3* a, const Vec3* b)
    {
        return a->Distance(*b);
    }

    float XG_Vec3_DistanceSquared(const Vec3* a, const Vec3* b)
    {
        return a->DistanceSq(*b);
    }

    Vec3 XG_Vec3_Lerp(const Vec3* a, const Vec3* b, float t)
    {
        return Vec3::Lerp(*a, *b, t);
    }

    Vec3 XG_Vec3_Min(const Vec3* a, const Vec3* b)
    {
        return Vec3::Min(*a, *b);
    }

    Vec3 XG_Vec3_Max(const Vec3* a, const Vec3* b)
    {
        return Vec3::Max(*a, *b);
    }

    Vec3 XG_Vec3_Clamp(const Vec3* v, const Vec3* min, const Vec3* max)
    {
        return Vec3::Clamp(*v, *min, *max);
    }

    Vec3 XG_Vec3_Transform(const Vec3* v, const Mat4x4* m)
    {
        return v->Transform(*m);
    }

    Vec3 XG_Vec3_TransformDirection(const Vec3* v, const Mat4x4* m)
    {
        return v->TransformDirection(*m);
    }

    bool XG_Vec3_IsZero(const Vec3* v)
    {
        return v->IsZero();
    }

    bool XG_Vec3_IsEqual(const Vec3* a, const Vec3* b, float epsilon)
    {
        return a->IsEqual(*b, epsilon);
    }
}
