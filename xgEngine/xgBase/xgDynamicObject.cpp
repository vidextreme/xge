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

    DynamicObject::DynamicObject(const TypeSchema* schema)
        : _schema(schema),
        _fields(nullptr),
        _fieldCount(schema ? schema->FieldCount : 0)
    {
        if (_schema && _fieldCount > 0)
        {
            _fields = new DynamicField[_fieldCount];

            for (uint32_t i = 0; i < _fieldCount; ++i)
            {
                const FieldSchema& fs = _schema->Fields[i];
                _fields[i].Schema = &fs;
                _fields[i].Value = MakeDefaultValue(fs);
            }
        }
    }

    DynamicObject::~DynamicObject()
    {
        if(_fields)
            delete[] _fields;
    }

    DynamicField* DynamicObject::GetFieldByIndex(uint32_t index)
    {
        return (index < _fieldCount) ? &_fields[index] : nullptr;
    }

    const DynamicField* DynamicObject::GetFieldByIndex(uint32_t index) const
    {
        return (index < _fieldCount) ? &_fields[index] : nullptr;
    }

    DynamicField* DynamicObject::FindField(const char* name)
    {
        for (uint32_t i = 0; i < _fieldCount; ++i)
        {
            const FieldSchema* fs = _fields[i].Schema;
            if (fs && fs->Name && std::strcmp(fs->Name, name) == 0)
                return &_fields[i];
        }
        return nullptr;
    }

    const DynamicField* DynamicObject::FindField(const char* name) const
    {
        for (uint32_t i = 0; i < _fieldCount; ++i)
        {
            const FieldSchema* fs = _fields[i].Schema;
            if (fs && fs->Name && std::strcmp(fs->Name, name) == 0)
                return &_fields[i];
        }
        return nullptr;
    }

    DynamicValue DynamicObject::MakeDefaultValue(const FieldSchema& fs)
    {
        return DynamicValue::Make(fs.ValueKind);
    }
}
