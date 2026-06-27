#pragma once
#include <cstdint>
#include <cstddef>

// Forward declarations to avoid circular include
namespace xg
{
    struct ISerializer;
    struct IDeserializer;
    struct RawFieldInfo;

    struct TypeInfoBase
    {
        const char* Name;
        size_t Size;

        const RawFieldInfo* (*GetFields)();
        int (*GetFieldCount)();

        void (*SerializeFn)(ISerializer&, const void*);
        void (*DeserializeFn)(IDeserializer&, void*);
    };

    template<typename T>
    struct TypeInfo : TypeInfoBase
    {
        static TypeInfo<T> Instance;

        static const RawFieldInfo* Fields();
        static int FieldCount();

    private:
        TypeInfo()
        {
            Name = T::StaticTypeName();
            Size = sizeof(T);

            GetFields = []() { return Fields(); };
            GetFieldCount = []() { return FieldCount(); };

            SerializeFn = [](ISerializer& s, const void* obj)
                {
                    const T& typed = *reinterpret_cast<const T*>(obj);
                    Serialize<T>(s, typed);
                };

            DeserializeFn = [](IDeserializer& d, void* obj)
                {
                    T& typed = *reinterpret_cast<T*>(obj);
                    Deserialize<T>(d, typed);
                };
        }
    };

    template<typename T>
    TypeInfo<T> TypeInfo<T>::Instance;

    template<typename T>
    const RawFieldInfo* TypeInfo<T>::Fields()
    {
        return nullptr;
    }

    template<typename T>
    int TypeInfo<T>::FieldCount()
    {
        return 0;
    }
}
