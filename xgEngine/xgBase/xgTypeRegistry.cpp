// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.
#include "pch.h"
#include "xgTypeRegistry.h"

namespace xg
{
    class TypeRegistryImpl : public TypeRegistry
    {
    public:
        bool Register(const TypeSchema* schema) override
        {
            if (!schema || !schema->Name)
                return false;

            auto it = _typesByName.find(schema->Name);
            if (it != _typesByName.end())
                return false; // duplicate

            _types.push_back(schema);
            _typesByName[schema->Name] = schema;

            return true;
        }

        const TypeSchema* Get(const char* typeName) const override
        {
            auto it = _typesByName.find(typeName);
            return (it != _typesByName.end()) ? it->second : nullptr;
        }

        const TypeSchema* Get(uint32_t typeId) const override
        {
            return (typeId < _types.size()) ? _types[typeId] : nullptr;
        }

        uint32_t GetTypeCount() const override
        {
            return static_cast<uint32_t>(_types.size());
        }

        const TypeSchema* GetTypeByIndex(uint32_t index) const override
        {
            return (index < _types.size()) ? _types[index] : nullptr;
        }

    private:
        std::vector<const TypeSchema*> _types;
        std::unordered_map<std::string, const TypeSchema*> _typesByName;
    };

    XG_DECLARE_UNIQUE_TYPE(TypeRegistryImpl)
    TypeRegistryUnique CreateTypeRegistry()
    {
        return TypeRegistryUnique(MakeTypeRegistryImplUniqueHandle(new TypeRegistryImpl()));
    }
}
