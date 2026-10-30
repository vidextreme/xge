#include "pch.h"
#include "xgQuatInterop.h"

namespace xg
{
    using interop::Load;

    interop::QuatAbi XG_Quat_Add(const Quat* a, const Quat* b)
    {
        return interop::ToAbi(Load(a).operator+(Load(b)));
    }

    interop::QuatAbi XG_Quat_Sub(const Quat* a, const Quat* b)
    {
        return interop::ToAbi(Load(a).operator-(Load(b)));
    }

    interop::QuatAbi XG_Quat_MulScalar(const Quat* q, float s)
    {
        return interop::ToAbi(Load(q) * s);
    }

    float XG_Quat_Dot(const Quat* a, const Quat* b)
    {
        return Load(a).Dot(Load(b));
    }

    float XG_Quat_Length(const Quat* q)
    {
        return Load(q).Length();
    }

    float XG_Quat_LengthSquared(const Quat* q)
    {
        return Load(q).LengthSq();
    }

    interop::QuatAbi XG_Quat_Normalized(const Quat* q)
    {
        return interop::ToAbi(Load(q).Normalized());
    }

    interop::QuatAbi XG_Quat_Slerp(const Quat* a, const Quat* b, float t)
    {
        return interop::ToAbi(Quat::Slerp(Load(a), Load(b), t));
    }

    interop::QuatAbi XG_Quat_Inverse(const Quat* q)
    {
        return interop::ToAbi(Load(q).Inverse());
    }

    interop::QuatAbi XG_Quat_Conjugate(const Quat* q)
    {
        return interop::ToAbi(Load(q).Conjugate());
    }

    interop::QuatAbi XG_Quat_MulQuat(const Quat* a, const Quat* b)
    {
        return interop::ToAbi(Load(a) * Load(b));
    }

    interop::Vec3Abi XG_Quat_RotateVec3(const Quat* q, const Vec3* v)
    {
        return interop::ToAbi(Load(q).RotateVector(Load(v)));
    }

    bool XG_Quat_IsIdentity(const Quat* q)
    {
        return Load(q).IsApproxEqual(Quat::Identity(), 1e-6f);
    }

    bool XG_Quat_IsEqual(const Quat* a, const Quat* b, float epsilon)
    {
        return Load(a).IsApproxEqual(Load(b), epsilon);
    }
}