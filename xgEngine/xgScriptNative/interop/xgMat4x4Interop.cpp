#include "pch.h"
#include "xgMat4x4Interop.h"

namespace xg
{
    using interop::Load;

    // Creation
    interop::Mat4x4Abi XG_Mat4x4_Identity()
    {
        return interop::ToAbi(Mat4x4::Identity());
    }

    interop::Mat4x4Abi XG_Mat4x4_Translation(float x, float y, float z)
    {
        return interop::ToAbi(Mat4x4::Translation(x, y, z));
    }

    interop::Mat4x4Abi XG_Mat4x4_Scaling(float x, float y, float z)
    {
        return interop::ToAbi(Mat4x4::Scaling(x, y, z));
    }

    interop::Mat4x4Abi XG_Mat4x4_RotationX(float angle)
    {
        return interop::ToAbi(Mat4x4::RotationX(angle));
    }

    interop::Mat4x4Abi XG_Mat4x4_RotationY(float angle)
    {
        return interop::ToAbi(Mat4x4::RotationY(angle));
    }

    interop::Mat4x4Abi XG_Mat4x4_RotationZ(float angle)
    {
        return interop::ToAbi(Mat4x4::RotationZ(angle));
    }

    // Arithmetic
    interop::Mat4x4Abi XG_Mat4x4_Multiply(const Mat4x4* a, const Mat4x4* b)
    {
        return interop::ToAbi(Load(a).operator*(Load(b)));
    }

    interop::Mat4x4Abi XG_Mat4x4_Transposed(const Mat4x4* m)
    {
        return interop::ToAbi(Load(m).Transposed());
    }

    interop::Mat4x4Abi XG_Mat4x4_Inverse(const Mat4x4* m)
    {
        return interop::ToAbi(Load(m).Inverse());
    }

    // Determinant
    float XG_Mat4x4_Determinant(const Mat4x4* m)
    {
        return Load(m).Determinant();
    }

    // Transform helpers
    interop::Vec3Abi XG_Mat4x4_TransformPoint(const Mat4x4* m, const Vec3* v)
    {
        return interop::ToAbi(Load(m).TransformPoint(Load(v)));
    }

    interop::Vec3Abi XG_Mat4x4_TransformDirection(const Mat4x4* m, const Vec3* v)
    {
        return interop::ToAbi(Load(m).TransformDirection(Load(v)));
    }

    // Decompose
    void XG_Mat4x4_Decompose(
        const Mat4x4* m,
        Vec3* outScale,
        Quat* outRotation,
        Vec3* outTranslation)
    {
        Vec3 scale, translation;
        Quat rotation;
        Load(m).Decompose(scale, rotation, translation);
        interop::Store(outScale, scale);
        interop::Store(outRotation, rotation);
        interop::Store(outTranslation, translation);
    }
}
