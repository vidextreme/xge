#pragma once
#include "xgTypeRegistry.h"
#include <cstdint>
#include <cstring>

namespace xg
{
    struct DynamicValue
    {
        ValueType Type = ValueType::None;

        union
        {
            bool        BoolValue;
            int32_t     Int32Value;
            uint32_t    UInt32Value;
            float       FloatValue;
            const char* StringValue;
        };

        DynamicValue();

        static DynamicValue Make(ValueType t);
    };

    struct DynamicField
    {
        const FieldSchema* Schema;
        DynamicValue        Value;
    };

    class DynamicObject
    {
    public:
        virtual ~DynamicObject() = default;

        virtual const TypeSchema* GetSchema() const = 0;
        virtual uint32_t GetFieldCount() const = 0;

        virtual DynamicField* GetFieldByIndex(uint32_t index) = 0;
        virtual const DynamicField* GetFieldByIndex(uint32_t index) const = 0;

        virtual DynamicField* FindField(const char* name) = 0;
        virtual const DynamicField* FindField(const char* name) const = 0;

    protected:

        static DynamicValue MakeDefaultValue(const FieldSchema& fs);
    };

    XG_DECLARE_UNIQUE(DynamicObject)
}
