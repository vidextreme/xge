// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.
#pragma once
#include "xgBase.h"

namespace xg
{

    //
    // ------------------------------------------------------------
    // ABI-STABLE HANDLE TYPES
    // ------------------------------------------------------------
    //

    struct XG_API xgUniqueHandle
    {
        void* ptr;
        void (*destroy)(void*);
    };

    struct XG_API xgRefHandle
    {
        void* ptr;
        void (*add_ref)(void*);
        void (*release)(void*);
    };

    struct XG_API xgWeakHandle
    {
        void* ptr;
        xgRefHandle(*lock)(void*);
        xgUniqueHandle(*promote)(void*);
    };


    //
    // ------------------------------------------------------------
    // UNIQUE OWNERSHIP WRAPPER
    // ------------------------------------------------------------
    //

    template<class T>
    class xgUnique
    {
    public:
        XG_FORCEINLINE xgUnique() : _handle{ nullptr, nullptr } {}
        XG_FORCEINLINE explicit xgUnique(xgUniqueHandle handle) : _handle(handle) {}

        xgUnique(const xgUnique&) = delete;
        xgUnique& operator=(const xgUnique&) = delete;

        XG_FORCEINLINE xgUnique(xgUnique&& other) noexcept : _handle(other._handle)
        {
            other._handle = { nullptr, nullptr };
        }

        XG_FORCEINLINE xgUnique& operator=(xgUnique&& other) noexcept
        {
            if (this != &other)
            {
                reset();
                _handle = other._handle;
                other._handle = { nullptr, nullptr };
            }
            return *this;
        }

        XG_FORCEINLINE ~xgUnique()
        {
            reset();
        }

        XG_FORCEINLINE T* get() const { return static_cast<T*>(_handle.ptr); }
        XG_FORCEINLINE T* operator->() const { return get(); }
        XG_FORCEINLINE T& operator*() const { return *get(); }

        XG_FORCEINLINE void reset()
        {
            if (_handle.ptr)
            {
                _handle.destroy(_handle.ptr);
                _handle.ptr = nullptr;
            }
        }
        XG_FORCEINLINE bool valid() const { return _handle.ptr != nullptr; }
        XG_FORCEINLINE explicit operator bool() const { return valid(); }

    private:
        xgUniqueHandle _handle;
    };


    //
    // ------------------------------------------------------------
    // REFCOUNTED OWNERSHIP WRAPPER
    // ------------------------------------------------------------
    //

    template<class T>
    class xgRef
    {
    public:
        XG_FORCEINLINE xgRef() : _handle{ nullptr, nullptr, nullptr } {}

        XG_FORCEINLINE explicit xgRef(xgRefHandle handle) : _handle(handle)
        {
            if (_handle.ptr)
                _handle.add_ref(_handle.ptr);
        }

        XG_FORCEINLINE xgRef(const xgRef& other) : _handle(other._handle)
        {
            if (_handle.ptr)
                _handle.add_ref(_handle.ptr);
        }

        XG_FORCEINLINE xgRef& operator=(const xgRef& other)
        {
            if (this != &other)
            {
                reset();
                _handle = other._handle;
                if (_handle.ptr)
                    _handle.add_ref(_handle.ptr);
            }
            return *this;
        }

        XG_FORCEINLINE xgRef(xgRef&& other) noexcept : _handle(other._handle)
        {
            other._handle = { nullptr, nullptr, nullptr };
        }

        XG_FORCEINLINE xgRef& operator=(xgRef&& other) noexcept
        {
            if (this != &other)
            {
                reset();
                _handle = other._handle;
                other._handle = { nullptr, nullptr, nullptr };
            }
            return *this;
        }

        XG_FORCEINLINE ~xgRef()
        {
            reset();
        }

        XG_FORCEINLINE T* get() const { return static_cast<T*>(_handle.ptr); }
        XG_FORCEINLINE T* operator->() const { return get(); }
        XG_FORCEINLINE T& operator*() const { return *get(); }

        XG_FORCEINLINE void reset()
        {
            if (_handle.ptr)
            {
                _handle.release(_handle.ptr);
                _handle.ptr = nullptr;
            }
        }

        XG_FORCEINLINE bool valid() const { return _handle.ptr != nullptr; }
        XG_FORCEINLINE explicit operator bool() const { return valid(); }

    private:
        xgRefHandle _handle;
    };


    //
    // ------------------------------------------------------------
    // WEAK HANDLE WRAPPER
    // ------------------------------------------------------------
    //

    template<class T>
    class xgWeak
    {
    public:
        XG_FORCEINLINE xgWeak() : _handle{ nullptr, nullptr, nullptr } {}
        XG_FORCEINLINE explicit xgWeak(xgWeakHandle handle) : _handle(handle) {}

        XG_FORCEINLINE xgWeak(const xgWeak& other) : _handle(other._handle) {}
        XG_FORCEINLINE xgWeak& operator=(const xgWeak& other)
        {
            _handle = other._handle;
            return *this;
        }

        XG_FORCEINLINE xgWeak(xgWeak&& other) noexcept : _handle(other._handle)
        {
            other._handle = { nullptr, nullptr, nullptr };
        }

        XG_FORCEINLINE xgWeak& operator=(xgWeak&& other) noexcept
        {
            if (this != &other)
            {
                _handle = other._handle;
                other._handle = { nullptr, nullptr, nullptr };
            }
            return *this;
        }

        XG_FORCEINLINE xgRef<T> lock() const
        {
            if (!_handle.ptr || !_handle.lock)
                return xgRef<T>(xgRefHandle{ nullptr, nullptr, nullptr });

            return xgRef<T>(_handle.lock(_handle.ptr));
        }

        XG_FORCEINLINE xgUnique<T> promote() const
        {
            if (!_handle.ptr || !_handle.promote)
                return xgUnique<T>(xgUniqueHandle{ nullptr, nullptr });

            return xgUnique<T>(_handle.promote(_handle.ptr));
        }

        XG_FORCEINLINE bool expired() const
        {
            if (!_handle.ptr || !_handle.lock)
                return true;

            xgRefHandle strong = _handle.lock(_handle.ptr);
            return strong.ptr == nullptr;
        }

    private:
        xgWeakHandle _handle;
    };


    //
    // ------------------------------------------------------------
    // EMPTY HANDLE HELPERS
    // ------------------------------------------------------------
#define XG_EMPTY_UNIQUE_HANDLE() xg::xgUniqueHandle{ nullptr, nullptr }
#define XG_EMPTY_REF_HANDLE()    xg::xgRefHandle{ nullptr, nullptr, nullptr }
#define XG_EMPTY_WEAK_HANDLE()   xg::xgWeakHandle{ nullptr, nullptr, nullptr }


//
// ------------------------------------------------------------
// TYPE-SAFE WRAPPER SHORTCUTS
// ------------------------------------------------------------
#define XG_DECLARE_UNIQUE(T) using T##Unique = ::xg::xgUnique<T>;
#define XG_DECLARE_REF(T)    using T##Ref    = ::xg::xgRef<T>;
#define XG_DECLARE_WEAK(T)   using T##Weak   = ::xg::xgWeak<T>;


//
// ------------------------------------------------------------
// UNIQUE HANDLE TYPE DECLARATION
// ------------------------------------------------------------
#define XG_DECLARE_UNIQUE_TYPE(T)                                      \
    extern "C" XG_MODULE_EXPORT void T##Destroy(void* p)               \
    { if (p) delete static_cast<T*>(p); }                              \
                                                                       \
    XG_FORCEINLINE xg::xgUniqueHandle Make##T##UniqueHandle(T* p)      \
    {                                                                  \
        return p ? xg::xgUniqueHandle{ p, &T##Destroy }                \
                 : XG_EMPTY_UNIQUE_HANDLE();                           \
    }                                                                  \
                                                                       \
    using T##Unique = xg::xgUnique<T>;


//
// ------------------------------------------------------------
// REFCOUNTED HANDLE TYPE DECLARATION
// ------------------------------------------------------------
#define XG_DECLARE_REFCOUNTED_TYPE(T)                                  \
    extern "C" XG_MODULE_EXPORT void T##AddRef(void* p)                \
    { if (p) static_cast<T*>(p)->AddRef(); }                           \
                                                                       \
    extern "C" XG_MODULE_EXPORT void T##Release(void* p)               \
    { if (p) static_cast<T*>(p)->Release(); }                          \
                                                                       \
    XG_FORCEINLINE xg::xgRefHandle Make##T##RefHandle(T* p)            \
    {                                                                  \
        return p ? xg::xgRefHandle{ p, &T##AddRef, &T##Release }       \
                 : XG_EMPTY_REF_HANDLE();                              \
    }                                                                  \
                                                                       \
    using T##Ref = xg::xgRef<T>;


//
// ------------------------------------------------------------
// STL-FREE FACTORY WRAPPERS (0–4 parameters)
// ------------------------------------------------------------
#define XG_WRAP_UNIQUE_FACTORY0(T, fn) \
    T##Unique Create##T() { return T##Unique(fn()); }

#define XG_WRAP_UNIQUE_FACTORY1(T, fn, A1) \
    T##Unique Create##T(A1 a1) { return T##Unique(fn(a1)); }

#define XG_WRAP_UNIQUE_FACTORY2(T, fn, A1, A2) \
    T##Unique Create##T(A1 a1, A2 a2) { return T##Unique(fn(a1, a2)); }

#define XG_WRAP_UNIQUE_FACTORY3(T, fn, A1, A2, A3) \
    T##Unique Create##T(A1 a1, A2 a2, A3 a3) { return T##Unique(fn(a1, a2, a3)); }

#define XG_WRAP_UNIQUE_FACTORY4(T, fn, A1, A2, A3, A4) \
    T##Unique Create##T(A1 a1, A2 a2, A3 a3, A4 a4) { return T##Unique(fn(a1, a2, a3, a4)); }


#define XG_WRAP_REF_FACTORY0(T, fn) \
    T##Ref Load##T() { return T##Ref(fn()); }

#define XG_WRAP_REF_FACTORY1(T, fn, A1) \
    T##Ref Load##T(A1 a1) { return T##Ref(fn(a1)); }

#define XG_WRAP_REF_FACTORY2(T, fn, A1, A2) \
    T##Ref Load##T(A1 a1, A2 a2) { return T##Ref(fn(a1, a2)); }

#define XG_WRAP_REF_FACTORY3(T, fn, A1, A2, A3) \
    T##Ref Load##T(A1 a1, A2 a2, A3 a3) { return T##Ref(fn(a1, a2, a3)); }

#define XG_WRAP_REF_FACTORY4(T, fn, A1, A2, A3, A4) \
    T##Ref Load##T(A1 a1, A2 a2, A3 a3, A4 a4) { return T##Ref(fn(a1, a2, a3, a4)); }

}//namespace xg