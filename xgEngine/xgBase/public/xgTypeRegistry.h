#pragma once
#include "xgHandles.h"
#include <cstdint>
#include <memory>
namespace xg
{
    struct FieldSchema
    {
        const char* Name;
        const char* TypeString;
        uint32_t Offset;
        uint32_t Size;
        uint32_t Alignment;
        const char* Attributes[8]; // fixed-size, STL-free
    };

    struct TypeSchema
    {
        const char* Name;
        uint32_t Version;
        bool NativeLayout;

        const FieldSchema* Fields;
        uint32_t FieldCount;

        uint32_t Size;
        uint32_t Alignment;
    };

    class TypeRegistry
    {
    public:
        virtual ~TypeRegistry() = default;

        // Register a type schema (called by generated code)
        virtual bool Register(const TypeSchema* schema) = 0;

        // Lookup by type name
        virtual const TypeSchema* Get(const char* typeName) const = 0;

        // Optional: lookup by ID
        virtual const TypeSchema* Get(uint32_t typeId) const = 0;

        // Enumerate all types
        virtual uint32_t GetTypeCount() const = 0;
        virtual const TypeSchema* GetTypeByIndex(uint32_t index) const = 0;
    };

    XG_API xgUnique<TypeRegistry> CreateTypeRegistry();
}

#define XG_REGISTER_TYPE_DEF(TypeName) \
    void RegisterType_##TypeName(xg::TypeRegistry* registry)

#define XG_REGISTER_TYPE(TypeName) \
    XG_REGISTER_TYPE_DEF(TypeName) { \
        registry->Register(&TypeName); \
    }

#define XG_REGISTER_FILE_TYPES(FileName) \
    void RegisterTypes_##FileName(xg::TypeRegistry* registry)
