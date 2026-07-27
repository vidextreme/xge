#include "pch.h"
#include "xgQuatInterop.h"

namespace xg
{
    Quat XG_Quat_Add(const Quat* a, const Quat* b)
    {
        return a->operator+(*b);
    }

    Quat XG_Quat_Sub(const Quat* a, const Quat* b)
    {
        return a->operator-(*b);
    }

    Quat XG_Quat_MulScalar(const Quat* q, float s)
    {
        return *q * s;
    }

    float XG_Quat_Dot(const Quat* a, const Quat* b)
    {
        return a->Dot(*b);
    }

    float XG_Quat_Length(const Quat* q)
    {
        return q->Length();
    }

    float XG_Quat_LengthSquared(const Quat* q)
    {
        return q->LengthSq();
    }

    Quat XG_Quat_Normalized(const Quat* q)
    {
        return q->Normalized();
    }

    Quat XG_Quat_Slerp(const Quat* a, const Quat* b, float t)
    {
        return Quat::Slerp(*a, *b, t);
    }

    Quat XG_Quat_Inverse(const Quat* q)
    {
        return q->Inverse();
    }

    Quat XG_Quat_Conjugate(const Quat* q)
    {
        return q->Conjugate();
    }

    Quat XG_Quat_MulQuat(const Quat* a, const Quat* b)
    {
        return *a * *b;
    }

    Vec3 XG_Quat_RotateVec3(const Quat* q, const Vec3* v)
    {
        return q->RotateVector(*v);
    }

    bool XG_Quat_IsIdentity(const Quat* q)
    {
        return q->IsApproxEqual(Quat::Identity(), 1e-6f);
    }

    bool XG_Quat_IsEqual(const Quat* a, const Quat* b, float epsilon)
    {
        return a->IsApproxEqual(*b, epsilon);
    }
}