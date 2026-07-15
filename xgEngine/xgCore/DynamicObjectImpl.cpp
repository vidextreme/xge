#include "pch.h"
#include "DynamicObjectImpl.h"

namespace xg
{    

    DynamicObjectImpl::DynamicObjectImpl(const TypeSchema* schema)
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
                _fields[i].Value = DynamicObject::MakeDefaultValue(fs);
            }
        }
    }

    DynamicObjectImpl::~DynamicObjectImpl()
    {
        if(_fields)
            delete[] _fields;
    }

    DynamicField* DynamicObjectImpl::GetFieldByIndex(uint32_t index)
    {
        return (index < _fieldCount) ? &_fields[index] : nullptr;
    }

    const DynamicField* DynamicObjectImpl::GetFieldByIndex(uint32_t index) const
    {
        return (index < _fieldCount) ? &_fields[index] : nullptr;
    }

    DynamicField* DynamicObjectImpl::FindField(const char* name)
    {
        for (uint32_t i = 0; i < _fieldCount; ++i)
        {
            const FieldSchema* fs = _fields[i].Schema;
            if (fs && fs->Name && std::strcmp(fs->Name, name) == 0)
                return &_fields[i];
        }
        return nullptr;
    }

    const DynamicField* DynamicObjectImpl::FindField(const char* name) const
    {
        for (uint32_t i = 0; i < _fieldCount; ++i)
        {
            const FieldSchema* fs = _fields[i].Schema;
            if (fs && fs->Name && std::strcmp(fs->Name, name) == 0)
                return &_fields[i];
        }
        return nullptr;
    }


}
