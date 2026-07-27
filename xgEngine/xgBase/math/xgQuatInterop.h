// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.
#pragma message(">>> entering <header name>")

#pragma once
#include "xgQuat.h"
#include "xgBase.h"

namespace xg
{
    // Arithmetic
    XG_MODULE_EXPORT Quat XG_Quat_Add(const Quat* a, const Quat* b);
    XG_MODULE_EXPORT Quat XG_Quat_Sub(const Quat* a, const Quat* b);
    XG_MODULE_EXPORT Quat XG_Quat_MulScalar(const Quat* q, float s);

    // Quaternion math
    XG_MODULE_EXPORT float XG_Quat_Dot(const Quat* a, const Quat* b);
    XG_MODULE_EXPORT float XG_Quat_Length(const Quat* q);
    XG_MODULE_EXPORT float XG_Quat_LengthSquared(const Quat* q);
    XG_MODULE_EXPORT Quat  XG_Quat_Normalized(const Quat* q);
    XG_MODULE_EXPORT Quat  XG_Quat_Slerp(const Quat* a, const Quat* b, float t);
    XG_MODULE_EXPORT Quat  XG_Quat_Inverse(const Quat* q);
    XG_MODULE_EXPORT Quat  XG_Quat_Conjugate(const Quat* q);
    XG_MODULE_EXPORT Quat  XG_Quat_MulQuat(const Quat* a, const Quat* b);

    // Transform helpers
    XG_MODULE_EXPORT Vec3 XG_Quat_RotateVec3(const Quat* q, const Vec3* v);

    // Utility
    XG_MODULE_EXPORT bool XG_Quat_IsIdentity(const Quat* q);
    XG_MODULE_EXPORT bool XG_Quat_IsEqual(const Quat* a, const Quat* b, float epsilon);
}
#pragma message("<<< leaving <header name>")
