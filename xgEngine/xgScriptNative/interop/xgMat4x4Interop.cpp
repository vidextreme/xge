#include "pch.h"
#include "xgMat4x4Interop.h"

namespace xg
{
    // Creation
    Mat4x4 XG_Mat4x4_Identity()
    {
        return Mat4x4::Identity();
    }

    Mat4x4 XG_Mat4x4_Translation(float x, float y, float z)
    {
        return Mat4x4::Translation(x, y, z);
    }

    Mat4x4 XG_Mat4x4_Scaling(float x, float y, float z)
    {
        return Mat4x4::Scaling(x, y, z);
    }

    Mat4x4 XG_Mat4x4_RotationX(float angle)
    {
        return Mat4x4::RotationX(angle);
    }

    Mat4x4 XG_Mat4x4_RotationY(float angle)
    {
        return Mat4x4::RotationY(angle);
    }

    Mat4x4 XG_Mat4x4_RotationZ(float angle)
    {
        return Mat4x4::RotationZ(angle);
    }

    // Arithmetic
    Mat4x4 XG_Mat4x4_Multiply(const Mat4x4* a, const Mat4x4* b)
    {
        return a->operator*(*b);
    }

    Mat4x4 XG_Mat4x4_Transposed(const Mat4x4* m)
    {
        return m->Transposed();
    }

    Mat4x4 XG_Mat4x4_Inverse(const Mat4x4* m)
    {
        return m->Inverse();
    }

    // Determinant
    float XG_Mat4x4_Determinant(const Mat4x4* m)
    {
        return m->Determinant();
    }

    // Transform helpers
    Vec3 XG_Mat4x4_TransformPoint(const Mat4x4* m, const Vec3* v)
    {
        return m->TransformPoint(*v);
    }

    Vec3 XG_Mat4x4_TransformDirection(const Mat4x4* m, const Vec3* v)
    {
        return m->TransformDirection(*v);
    }

    // Decompose
    void XG_Mat4x4_Decompose(
        const Mat4x4* m,
        Vec3* outScale,
        Quat* outRotation,
        Vec3* outTranslation)
    {
        m->Decompose(*outScale, *outRotation, *outTranslation);
    }
}
