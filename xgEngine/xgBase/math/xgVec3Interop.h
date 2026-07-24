#pragma once
#include "xgVec3.h"

#define XG_MODULE_EXPORT extern "C" __declspec(dllexport)

namespace xg
{
    // Arithmetic
    XG_MODULE_EXPORT Vec3 XG_Vec3_Add(const Vec3* a, const Vec3* b);
    XG_MODULE_EXPORT Vec3 XG_Vec3_Sub(const Vec3* a, const Vec3* b);
    XG_MODULE_EXPORT Vec3 XG_Vec3_MulScalar(const Vec3* v, float s);
    XG_MODULE_EXPORT Vec3 XG_Vec3_DivScalar(const Vec3* v, float s);

    // Vector math
    XG_MODULE_EXPORT float XG_Vec3_Length(const Vec3* v);
    XG_MODULE_EXPORT float XG_Vec3_LengthSquared(const Vec3* v);
    XG_MODULE_EXPORT Vec3  XG_Vec3_Normalized(const Vec3* v);
    XG_MODULE_EXPORT float XG_Vec3_Dot(const Vec3* a, const Vec3* b);
    XG_MODULE_EXPORT Vec3  XG_Vec3_Cross(const Vec3* a, const Vec3* b);

    // Distance
    XG_MODULE_EXPORT float XG_Vec3_Distance(const Vec3* a, const Vec3* b);
    XG_MODULE_EXPORT float XG_Vec3_DistanceSquared(const Vec3* a, const Vec3* b);

    // Lerp / Clamp / Min / Max
    XG_MODULE_EXPORT Vec3 XG_Vec3_Lerp(const Vec3* a, const Vec3* b, float t);
    XG_MODULE_EXPORT Vec3 XG_Vec3_Min(const Vec3* a, const Vec3* b);
    XG_MODULE_EXPORT Vec3 XG_Vec3_Max(const Vec3* a, const Vec3* b);
    XG_MODULE_EXPORT Vec3 XG_Vec3_Clamp(const Vec3* v, const Vec3* min, const Vec3* max);

    // Transform helpers
    XG_MODULE_EXPORT Vec3 XG_Vec3_Transform(const Vec3* v, const Mat4x4* m);
    XG_MODULE_EXPORT Vec3 XG_Vec3_TransformDirection(const Vec3* v, const Mat4x4* m);

    // Utility
    XG_MODULE_EXPORT bool XG_Vec3_IsZero(const Vec3* v);
    XG_MODULE_EXPORT bool XG_Vec3_IsEqual(const Vec3* a, const Vec3* b, float epsilon);
}
