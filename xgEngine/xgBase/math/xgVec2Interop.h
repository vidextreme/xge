// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.

#pragma once
#include "xgVec2.h"
#include "xgBase.h"


namespace xg
{
    // Arithmetic
    XG_MODULE_EXPORT Vec2 XG_Vec2_Add(const Vec2* a, const Vec2* b);
    XG_MODULE_EXPORT Vec2 XG_Vec2_Sub(const Vec2* a, const Vec2* b);
    XG_MODULE_EXPORT Vec2 XG_Vec2_MulScalar(const Vec2* v, float s);
    XG_MODULE_EXPORT Vec2 XG_Vec2_DivScalar(const Vec2* v, float s);

    // Vector math
    XG_MODULE_EXPORT float XG_Vec2_Length(const Vec2* v);
    XG_MODULE_EXPORT float XG_Vec2_LengthSquared(const Vec2* v);
    XG_MODULE_EXPORT Vec2  XG_Vec2_Normalized(const Vec2* v);
    XG_MODULE_EXPORT float XG_Vec2_Dot(const Vec2* a, const Vec2* b);

    // Distance
    XG_MODULE_EXPORT float XG_Vec2_Distance(const Vec2* a, const Vec2* b);
    XG_MODULE_EXPORT float XG_Vec2_DistanceSquared(const Vec2* a, const Vec2* b);

    // Lerp / Clamp / Min / Max
    XG_MODULE_EXPORT Vec2 XG_Vec2_Lerp(const Vec2* a, const Vec2* b, float t);
    XG_MODULE_EXPORT Vec2 XG_Vec2_Min(const Vec2* a, const Vec2* b);
    XG_MODULE_EXPORT Vec2 XG_Vec2_Max(const Vec2* a, const Vec2* b);
    XG_MODULE_EXPORT Vec2 XG_Vec2_Clamp(const Vec2* v, const Vec2* min, const Vec2* max);

    // Utility
    XG_MODULE_EXPORT bool XG_Vec2_IsZero(const Vec2* v);
    XG_MODULE_EXPORT bool XG_Vec2_IsEqual(const Vec2* a, const Vec2* b, float epsilon);
}
