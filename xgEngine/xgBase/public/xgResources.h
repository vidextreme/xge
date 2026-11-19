// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.
#pragma once
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include "xgBase.h"
#include "xgSubsystem.h"

namespace xg
{
    // 128-bit identity. Its text form is 32 hex digits in Hi, Lo order, the same text as an asset meta file's guid.
    struct ResourceGUID
    {
        uint64_t Hi;
        uint64_t Lo;
    };

    // Breadcrumb reference: the file first, then the chunk inside it. A zero Chunk addresses the whole file.
    // Text form: "<file-guid>/<chunk-guid>", or "<file-guid>" for the whole file.
    struct ResourceID
    {
        ResourceGUID File;
        ResourceGUID Chunk;
    };

    // Slot index + 1 in the low 32 bits, slot generation in the high 32 bits. 0 is invalid.
    using ResourceHandle = uint64_t;
    using ResourceTypeID = uint32_t;

    enum class ResourceState : int32_t
    {
        Invalid = 0,
        Pending = 1,
        Ready = 2,
        Failed = 3,
    };

    enum class ResourceTypeFlags : uint32_t
    {
        None = 0,
        // Load runs on a job worker; Acquire returns a Pending resource that becomes Ready or Failed in a later frame.
        Async = 1u << 0,
    };

    // Name is copied by RegisterType. The callbacks must stay callable until the type is unregistered.
    // Load reads the resource (on a worker for Async types, so no GPU or main-thread-only calls there). Finalize is
    // optional and runs on the main thread after an Async Load: it takes ownership of the loaded data, returns the
    // final data (e.g. after a GPU upload), and frees the loaded data itself when it fails. Unload frees final data.
    struct ResourceTypeDesc
    {
        const char* Name;
        bool (*Load)(const ResourceID* id, void* user, void** outData);
        void (*Unload)(void* data, void* user);
        void* User;
        bool (*Finalize)(void* data, void* user, void** outData);
        ResourceTypeFlags Flags;
    };

    // Bytes read from a resource source. Free them with ResourceSystem::FreeBlob.
    struct ResourceBlob
    {
        void* Data;
        uint64_t Size;
    };

    inline constexpr int32_t ResourceGUIDTextLength = 32;
    inline constexpr int32_t ResourceIDTextCapacity = ResourceGUIDTextLength * 2 + 2;

    inline bool IsZero(const ResourceGUID& guid)
    {
        return guid.Hi == 0 && guid.Lo == 0;
    }

    inline bool ParseResourceGUID(const char* text, size_t length, ResourceGUID* out)
    {
        if (!text || !out || length != static_cast<size_t>(ResourceGUIDTextLength))
            return false;
        uint64_t parts[2] = {};
        for (size_t i = 0; i < length; ++i)
        {
            const char c = text[i];
            uint64_t digit = 0;
            if (c >= '0' && c <= '9')
                digit = static_cast<uint64_t>(c - '0');
            else if (c >= 'a' && c <= 'f')
                digit = static_cast<uint64_t>(c - 'a' + 10);
            else if (c >= 'A' && c <= 'F')
                digit = static_cast<uint64_t>(c - 'A' + 10);
            else
                return false;
            uint64_t& part = parts[i / 16];
            part = (part << 4) | digit;
        }
        out->Hi = parts[0];
        out->Lo = parts[1];
        return true;
    }

    // Accepts only the canonical forms: a zero file GUID or a written-out zero chunk GUID is rejected.
    inline bool ParseResourceID(const char* text, ResourceID* out)
    {
        if (!text || !out)
            return false;
        const size_t guidLength = static_cast<size_t>(ResourceGUIDTextLength);
        const size_t length = std::strlen(text);
        ResourceID id = {};
        if (!ParseResourceGUID(text, (std::min)(length, guidLength), &id.File) || IsZero(id.File))
            return false;
        if (length != guidLength)
        {
            if (text[guidLength] != '/' || !ParseResourceGUID(text + guidLength + 1, length - guidLength - 1, &id.Chunk) || IsZero(id.Chunk))
                return false;
        }
        *out = id;
        return true;
    }

    // Writes lowercase text and a terminator. Returns the text length, or 0 when the buffer is too small.
    inline int32_t FormatResourceID(const ResourceID* id, char* buffer, int32_t capacity)
    {
        if (!id || !buffer)
            return 0;
        const bool hasChunk = !IsZero(id->Chunk);
        const int32_t length = hasChunk ? ResourceIDTextCapacity - 1 : ResourceGUIDTextLength;
        if (capacity <= length)
            return 0;
        const uint64_t parts[4] = { id->File.Hi, id->File.Lo, id->Chunk.Hi, id->Chunk.Lo };
        char* cursor = buffer;
        for (int32_t part = 0; part < (hasChunk ? 4 : 2); ++part)
        {
            if (part == 2)
                *cursor++ = '/';
            for (int32_t shift = 60; shift >= 0; shift -= 4)
                *cursor++ = "0123456789abcdef"[(parts[part] >> shift) & 0xF];
        }
        *cursor = '\0';
        return length;
    }

    // The engine-wide resource cache, shared by every module. Its calls are main thread only, except ReadFile and
    // FreeBlob, which resource loads use from job workers. Acquire may return a resource that is Pending or Failed,
    // so callers check GetState before using it.
    class ResourceSystem : public Subsystem
    {
        XG_DECLARE_BASE_SUBSYSTEM(ResourceSystem)

    public:
        virtual ~ResourceSystem() = default;

        // Returns 0 when the description is invalid or the name is already registered.
        virtual ResourceTypeID RegisterType(uint32_t moduleId, const ResourceTypeDesc* desc) = 0;
        virtual ResourceTypeID FindType(const char* name) const = 0;

        // Only the owning module can unregister. Live resources of the type are unloaded and their handles go stale.
        virtual bool UnregisterType(uint32_t moduleId, ResourceTypeID type) = 0;

        // Returns the existing resource with one more reference, or loads it. Returns 0 for invalid arguments.
        virtual ResourceHandle Acquire(uint32_t moduleId, ResourceTypeID type, const ResourceID* id) = 0;
        virtual bool AddRef(uint32_t moduleId, ResourceHandle handle) = 0;

        // A resource without references stays alive until the next Collect, and can be acquired again until then.
        virtual bool Release(uint32_t moduleId, ResourceHandle handle) = 0;

        virtual ResourceState GetState(ResourceHandle handle) const = 0;

        // Returns null unless the resource is Ready.
        virtual void* GetData(ResourceHandle handle) const = 0;

        // Drops the module's references and unregisters its types. Call while the module's code is still loaded.
        virtual void ReleaseModule(uint32_t moduleId) = 0;

        // Unloads resources that have no references. The engine calls it after each rendered frame.
        virtual void Collect() = 0;

        // Blocks until a Pending resource is Ready or Failed (running jobs meanwhile). Returns its state.
        virtual ResourceState Wait(ResourceHandle handle) = 0;

        // Adds a folder that ReadFile searches for "<guid>.<extension>", directly or one folder down (e.g. a project's
        // Library/Data). Returns 0 when the folder is missing. Sources are removed with their module.
        virtual uint32_t AddDirectorySource(uint32_t moduleId, const char* root) = 0;
        virtual bool RemoveSource(uint32_t moduleId, uint32_t source) = 0;

        // Reads the file "<file-guid>.<extension>" from the first source that has it. Thread-safe.
        virtual bool ReadFile(const ResourceGUID* file, const char* extension, ResourceBlob* out) = 0;
        virtual void FreeBlob(ResourceBlob* blob) = 0;
    };
}
