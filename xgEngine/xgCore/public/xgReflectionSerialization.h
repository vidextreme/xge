#pragma once
#include <cstring>
#include <cstdint>
#include "xgTypeInfo.h"

namespace xg
{
    //
    // ============================================================
    //  Generic Serializer Interface (STL-free)
    // ============================================================
    //
    struct ISerializer
    {
        virtual ~ISerializer() = default;

        virtual void BeginObject(const char* name) {}
        virtual void EndObject() {}

        virtual void WriteBool(const char* key, bool value) = 0;
        virtual void WriteInt(const char* key, int value) = 0;
        virtual void WriteFloat(const char* key, float value) = 0;
        virtual void WriteString(const char* key, const char* value) = 0;
    };

    //
    // ============================================================
    //  Generic Deserializer Interface (STL-free)
    // ============================================================
    //
    struct IDeserializer
    {
        virtual ~IDeserializer() = default;

        virtual bool ReadBool(const char* key) = 0;
        virtual int ReadInt(const char* key) = 0;
        virtual float ReadFloat(const char* key) = 0;
        virtual const char* ReadString(const char* key) = 0;
    };

    //
    // ============================================================
    //  Helper: decode pointer-to-member
    // ============================================================
    //
    template<typename T, typename MemberT>
    inline MemberT& ResolveMember(T& obj, std::uintptr_t offset)
    {
        return *reinterpret_cast<MemberT*>(reinterpret_cast<char*>(&obj) + offset);
    }

    template<typename T, typename MemberT>
    inline const MemberT& ResolveMember(const T& obj, std::uintptr_t offset)
    {
        return *reinterpret_cast<const MemberT*>(reinterpret_cast<const char*>(&obj) + offset);
    }


    //
    // ============================================================
    //  Reflection → Serializer
    // ============================================================
    //
    template<typename T>
    void Serialize(ISerializer& out, const T& obj)
    {
        using TI = TypeInfo<T>;

        const RawFieldInfo* fields = TI::Fields();
        const int count = TI::FieldCount();

        out.BeginObject(GetTypeInfo<T>()->Name);

        for (int i = 0; i < count; ++i)
        {
            const RawFieldInfo& f = fields[i];
            const char* key = f.Key;
            const char* type = f.TypeName;

            if (strcmp(type, "bool") == 0)
                out.WriteBool(key, ResolveMember<T, bool>(obj, f.MemberPtr));

            else if (strcmp(type, "int") == 0)
                out.WriteInt(key, ResolveMember<T, int>(obj, f.MemberPtr));

            else if (strcmp(type, "float") == 0)
                out.WriteFloat(key, ResolveMember<T, float>(obj, f.MemberPtr));

            else if (strcmp(type, "const char*") == 0)
                out.WriteString(key, ResolveMember<T, const char*>(obj, f.MemberPtr));
            else if (strcmp(type, "char*") == 0)
                out.WriteString(key, ResolveMember<T, const char*>(obj, f.MemberPtr));
        }

        out.EndObject();
    }


    //
    // ============================================================
    //  Deserializer → Reflection
    // ============================================================
    //
    template<typename T>
    void Deserialize(IDeserializer& in, T& obj)
    {
        using TI = TypeInfo<T>;

        const RawFieldInfo* fields = TI::Fields();
        const int count = TI::FieldCount();

        for (int i = 0; i < count; ++i)
        {
            const RawFieldInfo& f = fields[i];
            const char* key = f.Key;
            const char* type = f.TypeName;

            if (strcmp(type, "bool") == 0)
                ResolveMember<T, bool>(obj, f.MemberPtr) = in.ReadBool(key);

            else if (strcmp(type, "int") == 0)
                ResolveMember<T, int>(obj, f.MemberPtr) = in.ReadInt(key);

            else if (strcmp(type, "float") == 0)
                ResolveMember<T, float>(obj, f.MemberPtr) = in.ReadFloat(key);

            else if (strcmp(type, "const char*") == 0)
                ResolveMember<T, const char*>(obj, f.MemberPtr) = in.ReadString(key);

            else if (strcmp(type, "char*") == 0)
                ResolveMember<T, const char*>(obj, f.MemberPtr) = in.ReadString(key);

        }
    }

}
