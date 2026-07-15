#include "pch.h"
#include "xgDynamicObject.h"

namespace xg
{
    DynamicValue::DynamicValue()
        : Type(ValueType::None), Int32Value(0)
    {}

    DynamicValue DynamicValue::Make(ValueType t)
    {
        DynamicValue v;
        v.Type = t;

        switch (t)
        {
        case ValueType::Bool:
            v.BoolValue = false;
            break;
        case ValueType::Int32:
            v.Int32Value = 0;
            break;
        case ValueType::UInt32:
            v.UInt32Value = 0;
            break;
        case ValueType::Float:
            v.FloatValue = 0.0f;
            break;
        case ValueType::String:
            v.StringValue = nullptr;
            break;
        default:
            break;
        }

        return v;
    }

    DynamicValue DynamicObject::MakeDefaultValue(const FieldSchema& fs)
    {
        return DynamicValue::Make(fs.ValueKind);
    }
}