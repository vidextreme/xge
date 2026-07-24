// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.

#pragma once

// xgDirectXMath.inl - DirectXMath implementation details
// This file contains all SIMD operations and conversions using XGE-native aliases

#include "xgMathCommon.h"

namespace xg::detail
{
    //============================================================
    // Vec2 <-> XGVector conversions
    //============================================================
    inline XGVector Vec2ToXM(const Vec2& v) noexcept
    {
        return XMLoadFloat2(reinterpret_cast<const XGFloat2*>(&v));
    }

    inline void XMToVec2(XGVector v, Vec2& out) noexcept
    {
        XMStoreFloat2(reinterpret_cast<XGFloat2*>(&out), v);
    }

    //============================================================
    // Vec3 <-> XGVector conversions
    //============================================================
    inline XGVector Vec3ToXM(const Vec3& v) noexcept
    {
        return XMLoadFloat3(reinterpret_cast<const XGFloat3*>(&v));
    }

    inline void XMToVec3(XGVector v, Vec3& out) noexcept
    {
        XMStoreFloat3(reinterpret_cast<XGFloat3*>(&out), v);
    }

    //============================================================
    // Vec4 <-> XGVector conversions
    //============================================================
    inline XGVector Vec4ToXM(const Vec4& v) noexcept
    {
        return XMLoadFloat4(reinterpret_cast<const XGFloat4*>(&v));
    }

    inline void XMToVec4(XGVector v, Vec4& out) noexcept
    {
        XMStoreFloat4(reinterpret_cast<XGFloat4*>(&out), v);
    }

    //============================================================
    // Quat <-> XGVector conversions
    //============================================================
    inline XGVector QuatToXM(const Quat& q) noexcept
    {
        return XMLoadFloat4(reinterpret_cast<const XGFloat4*>(&q));
    }

    inline void XMToQuat(XGVector v, Quat& out) noexcept
    {
        XMStoreFloat4(reinterpret_cast<XGFloat4*>(&out), v);
    }

    //============================================================
    // Mat4x4 <-> XGMatrix conversions
    //============================================================
    inline XGMatrix Mat4x4ToXM(const Mat4x4& m) noexcept
    {
        return XMLoadFloat4x4(reinterpret_cast<const XGFloat4x4*>(&m));
    }

    inline void XMToMat4x4(XGMatrix m, Mat4x4& out) noexcept
    {
        XMStoreFloat4x4(reinterpret_cast<XGFloat4x4*>(&out), m);
    }

    //============================================================
    // Vector operations (Vec3)
    //============================================================
    inline XGVector Vec3Normalize(XGVector v) noexcept
    {
        return XMVector3Normalize(v);
    }

    inline float Vec3Length(XGVector v) noexcept
    {
        return XMVectorGetX(XMVector3Length(v));
    }

    inline float Vec3LengthSq(XGVector v) noexcept
    {
        return XMVectorGetX(XMVector3LengthSq(v));
    }

    inline float Vec3Dot(XGVector a, XGVector b) noexcept
    {
        return XMVectorGetX(XMVector3Dot(a, b));
    }

    inline XGVector Vec3Cross(XGVector a, XGVector b) noexcept
    {
        return XMVector3Cross(a, b);
    }

    inline XGVector Vec3Lerp(XGVector a, XGVector b, float t) noexcept
    {
        return XMVectorLerp(a, b, t);
    }

    inline XGVector Vec3Negate(XGVector v) noexcept
    {
        return XMVectorNegate(v);
    }

    inline XGVector Vec3Add(XGVector a, XGVector b) noexcept
    {
        return XMVectorAdd(a, b);
    }

    inline XGVector Vec3Sub(XGVector a, XGVector b) noexcept
    {
        return XMVectorSubtract(a, b);
    }

    inline XGVector Vec3Mul(XGVector a, XGVector b) noexcept
    {
        return XMVectorMultiply(a, b);
    }

    inline XGVector Vec3Div(XGVector a, XGVector b) noexcept
    {
        return XMVectorDivide(a, b);
    }

    inline XGVector Vec3Scale(XGVector v, float s) noexcept
    {
        return XMVectorScale(v, s);
    }

    inline XGVector Vec3Min(XGVector a, XGVector b) noexcept
    {
        return XMVectorMin(a, b);
    }

    inline XGVector Vec3Max(XGVector a, XGVector b) noexcept
    {
        return XMVectorMax(a, b);
    }

    inline XGVector Vec3Clamp(XGVector v, XGVector min, XGVector max) noexcept
    {
        return XMVectorClamp(v, min, max);
    }

    inline bool Vec3NearEqual(XGVector a, XGVector b, float epsilon = constants::EPSILON_POS) noexcept
    {
        return XMVector3NearEqual(a, b, XMVectorReplicate(epsilon));
    }

    inline XGVector Vec3Abs(XGVector v) noexcept
    {
        return XMVectorAbs(v);
    }

    inline XGVector Vec3Sqrt(XGVector v) noexcept
    {
        return XMVectorSqrt(v);
    }

    inline XGVector Vec3Reciprocal(XGVector v) noexcept
    {
        return XMVectorReciprocal(v);
    }

    inline XGVector Vec3ReciprocalSqrt(XGVector v) noexcept
    {
        return XMVectorReciprocalSqrt(v);
    }

    //============================================================
    // Vector operations (Vec4)
    //============================================================
    inline XGVector Vec4Normalize(XGVector v) noexcept
    {
        return XMVector4Normalize(v);
    }

    inline float Vec4Length(XGVector v) noexcept
    {
        return XMVectorGetX(XMVector4Length(v));
    }

    inline float Vec4LengthSq(XGVector v) noexcept
    {
        return XMVectorGetX(XMVector4LengthSq(v));
    }

    inline float Vec4Dot(XGVector a, XGVector b) noexcept
    {
        return XMVectorGetX(XMVector4Dot(a, b));
    }

    inline XGVector Vec4Lerp(XGVector a, XGVector b, float t) noexcept
    {
        return XMVectorLerp(a, b, t);
    }

    inline XGVector Vec4Negate(XGVector v) noexcept
    {
        return XMVectorNegate(v);
    }

    inline XGVector Vec4Add(XGVector a, XGVector b) noexcept
    {
        return XMVectorAdd(a, b);
    }

    inline XGVector Vec4Sub(XGVector a, XGVector b) noexcept
    {
        return XMVectorSubtract(a, b);
    }

    inline XGVector Vec4Mul(XGVector a, XGVector b) noexcept
    {
        return XMVectorMultiply(a, b);
    }

    inline XGVector Vec4Div(XGVector a, XGVector b) noexcept
    {
        return XMVectorDivide(a, b);
    }

    inline XGVector Vec4Scale(XGVector v, float s) noexcept
    {
        return XMVectorScale(v, s);
    }

    // Transform vector by matrix (4x4)
    inline XGVector Vec4Transform(XGVector v, XGMatrix m) noexcept
    {
        return XMVector4Transform(v, m);
    }

    // Create vector from components
    inline XGVector Vec4Set(float x, float y, float z, float w) noexcept
    {
        return XMVectorSet(x, y, z, w);
    }

    //============================================================
    // Quaternion operations
    //============================================================
    inline XGVector QuatIdentity() noexcept
    {
        return XMQuaternionIdentity();
    }

    inline XGVector QuatNormalize(XGVector q) noexcept
    {
        return XMQuaternionNormalize(q);
    }

    inline float QuatLength(XGVector q) noexcept
    {
        return XMVectorGetX(XMQuaternionLength(q));
    }

    inline float QuatLengthSq(XGVector q) noexcept
    {
        return XMVectorGetX(XMQuaternionLengthSq(q));
    }

    inline float QuatDot(XGVector a, XGVector b) noexcept
    {
        return XMVectorGetX(XMQuaternionDot(a, b));
    }

    inline XGVector QuatConjugate(XGVector q) noexcept
    {
        return XMQuaternionConjugate(q);
    }

    inline XGVector QuatInverse(XGVector q) noexcept
    {
        return XMQuaternionInverse(q);
    }

    inline XGVector QuatMultiply(XGVector a, XGVector b) noexcept
    {
        return XMQuaternionMultiply(a, b);
    }

    inline XGVector QuatSlerp(XGVector a, XGVector b, float t) noexcept
    {
        return XMQuaternionSlerp(a, b, t);
    }

    inline XGVector QuatSlerpV(XGVector a, XGVector b, XGVector t) noexcept
    {
        return XMQuaternionSlerpV(a, b, t);
    }

    inline XGVector QuatLerp(XGVector a, XGVector b, float t) noexcept
    {
        return XMVectorLerp(a, b, t);
    }

    inline XGVector QuatRotationAxis(XGVector axis, float angle) noexcept
    {
        return XMQuaternionRotationAxis(axis, angle);
    }

    inline XGVector QuatRotationRollPitchYaw(float pitch, float yaw, float roll) noexcept
    {
        return XMQuaternionRotationRollPitchYaw(pitch, yaw, roll);
    }

    inline XGVector QuatRotationMatrix(XGMatrix m) noexcept
    {
        return XMQuaternionRotationMatrix(m);
    }

    inline void QuatToAxisAngle(XGVector q, XGVector& axis, float& angle) noexcept
    {
        XMQuaternionToAxisAngle(&axis, &angle, q);
    }

    inline XGVector QuatRotateVector(XGVector q, XGVector v) noexcept
    {
        return XMVector3Rotate(v, q);
    }

    //============================================================
    // Matrix operations (Mat4x4)
    //============================================================
    inline XGMatrix Mat4x4Identity() noexcept
    {
        return XMMatrixIdentity();
    }

    inline XGMatrix Mat4x4Transpose(XGMatrix m) noexcept
    {
        return XMMatrixTranspose(m);
    }

    inline XGMatrix Mat4x4Multiply(XGMatrix a, XGMatrix b) noexcept
    {
        return XMMatrixMultiply(a, b);
    }

    inline XGMatrix Mat4x4MultiplyTranspose(XGMatrix a, XGMatrix b) noexcept
    {
        return XMMatrixMultiplyTranspose(a, b);
    }

    inline XGMatrix Mat4x4Inverse(XGMatrix m, XGVector* det = nullptr) noexcept
    {
        return XMMatrixInverse(det, m);
    }

    inline XGVector Mat4x4Determinant(XGMatrix m) noexcept
    {
        return XMMatrixDeterminant(m);
    }

    inline float DeterminantX(XGMatrix m) noexcept
    {
        return DirectX::XMVectorGetX(detail::Mat4x4Determinant(m));
    }

    inline XGMatrix Mat4x4Translation(float x, float y, float z) noexcept
    {
        return XMMatrixTranslation(x, y, z);
    }

    inline XGMatrix Mat4x4Translation(XGVector v) noexcept
    {
        return XMMatrixTranslationFromVector(v);
    }

    inline XGMatrix Mat4x4RotationX(float angle) noexcept
    {
        return XMMatrixRotationX(angle);
    }

    inline XGMatrix Mat4x4RotationY(float angle) noexcept
    {
        return XMMatrixRotationY(angle);
    }

    inline XGMatrix Mat4x4RotationZ(float angle) noexcept
    {
        return XMMatrixRotationZ(angle);
    }

    inline XGMatrix Mat4x4RotationRollPitchYaw(float pitch, float yaw, float roll) noexcept
    {
        return XMMatrixRotationRollPitchYaw(pitch, yaw, roll);
    }

    inline XGMatrix Mat4x4RotationAxis(XGVector axis, float angle) noexcept
    {
        return XMMatrixRotationAxis(axis, angle);
    }

    inline XGMatrix Mat4x4RotationQuaternion(XGVector q) noexcept
    {
        return XMMatrixRotationQuaternion(q);
    }

    inline XGMatrix Mat4x4Scaling(float x, float y, float z) noexcept
    {
        return XMMatrixScaling(x, y, z);
    }

    inline XGMatrix Mat4x4Scaling(XGVector v) noexcept
    {
        return XMMatrixScalingFromVector(v);
    }

    inline XGMatrix Mat4x4AffineTransformation(XGVector scaling, XGVector rotationOrigin, XGVector rotation, XGVector translation) noexcept
    {
        return XMMatrixAffineTransformation(scaling, rotationOrigin, rotation, translation);
    }

    inline XGMatrix Mat4x4LookAtLH(XGVector eye, XGVector at, XGVector up) noexcept
    {
        return XMMatrixLookAtLH(eye, at, up);
    }

    inline XGMatrix Mat4x4LookAtRH(XGVector eye, XGVector at, XGVector up) noexcept
    {
        return XMMatrixLookAtRH(eye, at, up);
    }

    inline XGMatrix Mat4x4PerspectiveFovLH(float fovY, float aspect, float nearZ, float farZ) noexcept
    {
        return XMMatrixPerspectiveFovLH(fovY, aspect, nearZ, farZ);
    }

    inline XGMatrix Mat4x4PerspectiveFovRH(float fovY, float aspect, float nearZ, float farZ) noexcept
    {
        return XMMatrixPerspectiveFovRH(fovY, aspect, nearZ, farZ);
    }

    inline XGMatrix Mat4x4OrthographicLH(float width, float height, float nearZ, float farZ) noexcept
    {
        return XMMatrixOrthographicLH(width, height, nearZ, farZ);
    }

    inline XGMatrix Mat4x4OrthographicRH(float width, float height, float nearZ, float farZ) noexcept
    {
        return XMMatrixOrthographicRH(width, height, nearZ, farZ);
    }

    inline XGMatrix Mat4x4OrthographicOffCenterLH(float left, float right, float bottom, float top, float nearZ, float farZ) noexcept
    {
        return XMMatrixOrthographicOffCenterLH(left, right, bottom, top, nearZ, farZ);
    }

    inline XGMatrix Mat4x4OrthographicOffCenterRH(float left, float right, float bottom, float top, float nearZ, float farZ) noexcept
    {
        return XMMatrixOrthographicOffCenterRH(left, right, bottom, top, nearZ, farZ);
    }

    inline XGVector Mat4x4GetTranslation(XGMatrix m) noexcept
    {
        return m.r[3];
    }

    inline XGVector Mat4x4GetScale(XGMatrix m) noexcept
    {
        return XMVectorSet(
            XMVectorGetX(XMVector3Length(m.r[0])),
            XMVectorGetX(XMVector3Length(m.r[1])),
            XMVectorGetX(XMVector3Length(m.r[2])),
            0.0f
        );
    }

    inline XGVector Mat4x4GetRotation(XGMatrix m) noexcept
    {
        XGMatrix rot = m;
        // Remove translation
        rot.r[3] = XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f);
        // Remove scale
        XGVector scale = Mat4x4GetScale(m);
        rot.r[0] = XMVectorDivide(rot.r[0], scale);
        rot.r[1] = XMVectorDivide(rot.r[1], scale);
        rot.r[2] = XMVectorDivide(rot.r[2], scale);
        return XMQuaternionRotationMatrix(rot);
    }

    inline void Mat4x4Decompose(XGMatrix m, XGVector& scale, XGVector& rotation, XGVector& translation) noexcept
    {
        XMMatrixDecompose(&scale, &rotation, &translation, m);
    }

    inline bool Mat4x4IsIdentity(XGMatrix m) noexcept
    {
        return XMMatrixIsIdentity(m);
    }

    inline bool Mat4x4IsInfinite(XGMatrix m) noexcept
    {
        return XMMatrixIsInfinite(m);
    }

    inline bool Mat4x4IsNaN(XGMatrix m) noexcept
    {
        return XMMatrixIsNaN(m);
    }

    //============================================================
    // Bounds operations
    //============================================================
    inline bool BoundsIntersectsAABB(XGVector minA, XGVector maxA, XGVector minB, XGVector maxB) noexcept
    {
        XGVector less = XMVectorLess(maxA, minB);
        XGVector greater = XMVectorGreater(minA, maxB);
        XGVector disjoint = XMVectorOrInt(less, greater);
        return XMVector4EqualInt(disjoint, XMVectorZero());
    }

    inline bool BoundsIntersectsSphere(XGVector center, float radius, XGVector min, XGVector max) noexcept
    {
        // Find closest point on AABB to sphere center
        XGVector closest = XMVectorClamp(center, min, max);
        XGVector diff = XMVectorSubtract(closest, center);
        float distSq = XMVectorGetX(XMVector3Dot(diff, diff));
        return distSq <= radius * radius;
    }

    inline bool BoundsContainsPoint(XGVector point, XGVector min, XGVector max) noexcept
    {
        XGVector greaterEqual = XMVectorGreaterOrEqual(point, min);
        XGVector lessEqual = XMVectorLessOrEqual(point, max);
        XGVector inside = XMVectorAndInt(greaterEqual, lessEqual);
        return XMVector4EqualInt(inside, XMVectorTrueInt());
    }

    inline void BoundsMerge(XGVector minA, XGVector maxA, XGVector minB, XGVector maxB, XGVector& outMin, XGVector& outMax) noexcept
    {
        outMin = XMVectorMin(minA, minB);
        outMax = XMVectorMax(maxA, maxB);
    }

    inline void BoundsExpand(XGVector min, XGVector max, XGVector point, XGVector& outMin, XGVector& outMax) noexcept
    {
        outMin = XMVectorMin(min, point);
        outMax = XMVectorMax(max, point);
    }

    inline void BoundsTransform(XGVector min, XGVector max, XGMatrix m, XGVector& outMin, XGVector& outMax) noexcept
    {
        // Transform all 8 corners
        XGVector corners[8] = {
            XMVectorSet(XMVectorGetX(min), XMVectorGetY(min), XMVectorGetZ(min), 1.0f),
            XMVectorSet(XMVectorGetX(max), XMVectorGetY(min), XMVectorGetZ(min), 1.0f),
            XMVectorSet(XMVectorGetX(min), XMVectorGetY(max), XMVectorGetZ(min), 1.0f),
            XMVectorSet(XMVectorGetX(max), XMVectorGetY(max), XMVectorGetZ(min), 1.0f),
            XMVectorSet(XMVectorGetX(min), XMVectorGetY(min), XMVectorGetZ(max), 1.0f),
            XMVectorSet(XMVectorGetX(max), XMVectorGetY(min), XMVectorGetZ(max), 1.0f),
            XMVectorSet(XMVectorGetX(min), XMVectorGetY(max), XMVectorGetZ(max), 1.0f),
            XMVectorSet(XMVectorGetX(max), XMVectorGetY(max), XMVectorGetZ(max), 1.0f),
        };

        outMin = XMVectorReplicate(FLT_MAX);
        outMax = XMVectorReplicate(-FLT_MAX);

        for (int i = 0; i < 8; ++i)
        {
            XGVector transformed = XMVector4Transform(corners[i], m);
            transformed = XMVectorDivide(transformed, XMVectorSplatW(transformed)); // Perspective divide
            outMin = XMVectorMin(outMin, transformed);
            outMax = XMVectorMax(outMax, transformed);
        }
    }

    //============================================================
    // Ray operations
    //============================================================
    inline bool RayIntersectsAABB(XGVector origin, XGVector direction, XGVector min, XGVector max, float& tNear, float& tFar) noexcept
    {
        // Slab method
        XGVector invDir = XMVectorReciprocal(direction);
        XGVector t1 = XMVectorMultiply(XMVectorSubtract(min, origin), invDir);
        XGVector t2 = XMVectorMultiply(XMVectorSubtract(max, origin), invDir);

        XGVector tMin = XMVectorMin(t1, t2);
        XGVector tMax = XMVectorMax(t1, t2);

        float tminX = XMVectorGetX(tMin);
        float tminY = XMVectorGetY(tMin);
        float tminZ = XMVectorGetZ(tMin);
        float tmaxX = XMVectorGetX(tMax);
        float tmaxY = XMVectorGetY(tMax);
        float tmaxZ = XMVectorGetZ(tMax);

        tNear = fmaxf(fmaxf(tminX, tminY), tminZ);
        tFar = fminf(fminf(tmaxX, tmaxY), tmaxZ);

        return tNear <= tFar && tFar >= 0.0f;
    }

    inline bool RayIntersectsSphere(XGVector origin, XGVector direction, XGVector center, float radius, float& tNear, float& tFar) noexcept
    {
        XGVector oc = XMVectorSubtract(origin, center);
        float a = XMVectorGetX(XMVector3Dot(direction, direction));
        float b = 2.0f * XMVectorGetX(XMVector3Dot(oc, direction));
        float c = XMVectorGetX(XMVector3Dot(oc, oc)) - radius * radius;

        float discriminant = b * b - 4.0f * a * c;
        if (discriminant < 0.0f)
            return false;

        float sqrtDisc = sqrtf(discriminant);
        float inv2a = 1.0f / (2.0f * a);
        tNear = (-b - sqrtDisc) * inv2a;
        tFar = (-b + sqrtDisc) * inv2a;

        return tFar >= 0.0f;
    }

    inline bool RayIntersectsPlane(XGVector origin, XGVector direction, XGVector plane, float& t) noexcept
    {
        // plane = (normal.x, normal.y, normal.z, d) where dot(normal, point) + d = 0
        XGVector normal = XMVectorSet(XMVectorGetX(plane), XMVectorGetY(plane), XMVectorGetZ(plane), 0.0f);
        float denom = XMVectorGetX(XMVector3Dot(normal, direction));
        if (fabsf(denom) < constants::EPSILON_POS)
            return false;

        float numer = XMVectorGetX(XMVector3Dot(normal, origin)) + XMVectorGetW(plane);
        t = -numer / denom;
        return true;
    }

    inline bool RayIntersectsTriangle(XGVector origin, XGVector direction, XGVector v0, XGVector v1, XGVector v2, float& t, float& u, float& v) noexcept
    {
        // Möller-Trumbore algorithm
        XGVector edge1 = XMVectorSubtract(v1, v0);
        XGVector edge2 = XMVectorSubtract(v2, v0);
        XGVector h = XMVector3Cross(direction, edge2);
        float a = XMVectorGetX(XMVector3Dot(edge1, h));

        if (fabsf(a) < constants::EPSILON_POS)
            return false;

        float f = 1.0f / a;
        XGVector s = XMVectorSubtract(origin, v0);
        u = f * XMVectorGetX(XMVector3Dot(s, h));
        if (u < 0.0f || u > 1.0f)
            return false;

        XGVector q = XMVector3Cross(s, edge1);
        v = f * XMVectorGetX(XMVector3Dot(direction, q));
        if (v < 0.0f || u + v > 1.0f)
            return false;

        t = f * XMVectorGetX(XMVector3Dot(edge2, q));
        return t >= 0.0f;
    }

    // Mat4x4 → XGMatrix (assuming Mat4x4 stores float[16] or XGFloat4x4)
    inline XGMatrix ToXGMatrix(const Mat4x4& m)
    {
        return XMLoadFloat4x4(reinterpret_cast<const XGFloat4x4*>(&m));
    }

    inline XGVector TransformPointSIMD(XGVector v, const Mat4x4& m)
    {
        return XMVector3Transform(v, ToXGMatrix(m));
    }

    inline XGVector TransformDirectionSIMD(XGVector v, const Mat4x4& m)
    {
        return XMVector3TransformNormal(v, ToXGMatrix(m));
    }

    inline bool IsZeroSIMD(XGVector v, float epsilon = xg::constants::EPSILON_POS)
    {
        // Compute length of the vector
        XGVector len = XMVector3Length(v);

        // Compare length to epsilon
        return XMVectorGetX(len) <= epsilon;
    }

    inline bool IsEqualSIMD(const XGVector& a, const XGVector& b, float epsilon)
    {
        XGVector diff = XMVectorSubtract(a, b);
        XGVector len = XMVector3Length(diff);
        return XMVectorGetX(len) <= epsilon;
    }


} // namespace xg::detail