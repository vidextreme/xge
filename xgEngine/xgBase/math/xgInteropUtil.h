// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.

#pragma once

// xgInteropUtil.h - Safe access to memory owned by managed (C#) callers
//
// Vec3, Vec4, Quat, Mat4x4 and the shapes are alignas(16), so the compiler may use aligned
// SSE moves (movaps) when dereferencing pointers to them. .NET only guarantees 4/8-byte
// alignment for structs on the managed stack, so an aligned access to a C# argument or
// out-parameter can fault depending on where the caller's stack happens to land.
//
// Every C ABI export (XG_*) must therefore read its pointer arguments with Load() and
// write results with Store(), which compile to unaligned moves. Exports must not return
// alignas(16) types by value either: the hidden return buffer comes from the caller too.

#include <cstring>
#include <type_traits>
#include "xgVec2.h"
#include "xgVec3.h"
#include "xgQuat.h"
#include "xgMat4x4.h"

namespace xg::interop
{
    //============================================================
    // By-value return types for exports: same size and layout as the math types,
    // but plain C structs (4-byte aligned, no constructors, no C4190 warning).
    // This matters beyond alignment: under the MSVC x64 ABI a C++ type with
    // user-declared constructors is always returned through a hidden pointer, while
    // .NET returns an 8-byte blittable struct (Vector2) in RAX. A POD Vec2Abi is
    // returned in RAX too, so both sides agree.
    //============================================================
    struct Vec2Abi { float x, y; };           // Vec2 (8 bytes, returned in RAX)
    struct Vec3Abi { float x, y, z, pad; };   // Vec3 (16 bytes incl. padding)
    struct QuatAbi { float x, y, z, w; };     // Quat
    struct Mat4x4Abi { float m[16]; };        // Mat4x4

    static_assert(sizeof(Vec2Abi) == sizeof(Vec2), "Vec2Abi size");
    static_assert(sizeof(Vec3Abi) == sizeof(Vec3), "Vec3Abi size");
    static_assert(sizeof(QuatAbi) == sizeof(Quat), "QuatAbi size");
    static_assert(sizeof(Mat4x4Abi) == sizeof(Mat4x4), "Mat4x4Abi size");

    inline Vec2Abi ToAbi(const Vec2& v) noexcept { return Vec2Abi{ v.x, v.y }; }
    inline Vec3Abi ToAbi(const Vec3& v) noexcept { return Vec3Abi{ v.x, v.y, v.z, 0.0f }; }
    inline QuatAbi ToAbi(const Quat& q) noexcept { return QuatAbi{ q.x, q.y, q.z, q.w }; }
    inline Mat4x4Abi ToAbi(const Mat4x4& m) noexcept
    {
        Mat4x4Abi r;
        std::memcpy(r.m, &m, sizeof(r.m));
        return r;
    }

    template<typename T>
    inline T Load(const T* src) noexcept
    {
        static_assert(std::is_trivially_copyable_v<T>, "interop types must be trivially copyable");
        T value;
        std::memcpy(static_cast<void*>(&value), static_cast<const void*>(src), sizeof(T));
        return value;
    }

    template<typename T>
    inline void Store(T* dst, const T& value) noexcept
    {
        static_assert(std::is_trivially_copyable_v<T>, "interop types must be trivially copyable");
        std::memcpy(static_cast<void*>(dst), static_cast<const void*>(&value), sizeof(T));
    }

    // Optional out-parameter
    template<typename T>
    inline void StoreIf(T* dst, const T& value) noexcept
    {
        if (dst)
            Store(dst, value);
    }
}
