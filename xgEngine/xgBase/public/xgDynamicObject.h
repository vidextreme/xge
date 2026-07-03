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
        explicit DynamicObject(const TypeSchema* schema);
        ~DynamicObject();

        const TypeSchema* GetSchema() const { return _schema; }
        uint32_t GetFieldCount() const { return _fieldCount; }

        DynamicField* GetFieldByIndex(uint32_t index);
        const DynamicField* GetFieldByIndex(uint32_t index) const;

        DynamicField* FindField(const char* name);
        const DynamicField* FindField(const char* name) const;

    private:
        const TypeSchema* _schema = nullptr;
        DynamicField* _fields = nullptr;
        uint32_t          _fieldCount = 0;

        static DynamicValue MakeDefaultValue(const FieldSchema& fs);
    };
}
