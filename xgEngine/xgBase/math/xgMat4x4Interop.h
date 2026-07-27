#pragma once
#include "xgBase.h"
#include "xgMat4x4.h"
#include "xgVec3.h"
#include "xgQuat.h"

namespace xg
{
    // Creation
    XG_MODULE_EXPORT Mat4x4 XG_Mat4x4_Identity();
    XG_MODULE_EXPORT Mat4x4 XG_Mat4x4_Translation(float x, float y, float z);
    XG_MODULE_EXPORT Mat4x4 XG_Mat4x4_Scaling(float x, float y, float z);
    XG_MODULE_EXPORT Mat4x4 XG_Mat4x4_RotationX(float angle);
    XG_MODULE_EXPORT Mat4x4 XG_Mat4x4_RotationY(float angle);
    XG_MODULE_EXPORT Mat4x4 XG_Mat4x4_RotationZ(float angle);

    // Arithmetic
    XG_MODULE_EXPORT Mat4x4 XG_Mat4x4_Multiply(const Mat4x4* a, const Mat4x4* b);
    XG_MODULE_EXPORT Mat4x4 XG_Mat4x4_Transposed(const Mat4x4* m);
    XG_MODULE_EXPORT Mat4x4 XG_Mat4x4_Inverse(const Mat4x4* m);

    // Determinant
    XG_MODULE_EXPORT float XG_Mat4x4_Determinant(const Mat4x4* m);

    // Transform helpers
    XG_MODULE_EXPORT Vec3 XG_Mat4x4_TransformPoint(const Mat4x4* m, const Vec3* v);
    XG_MODULE_EXPORT Vec3 XG_Mat4x4_TransformDirection(const Mat4x4* m, const Vec3* v);

    // Decompose
    XG_MODULE_EXPORT void XG_Mat4x4_Decompose(
        const Mat4x4* m,
        Vec3* outScale,
        Quat* outRotation,
        Vec3* outTranslation);
}
