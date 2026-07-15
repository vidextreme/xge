#pragma once
#include "xgDynamicObject.h"

namespace xg
{
	class DynamicObjectImpl : public DynamicObject
    {
    public:
        explicit DynamicObjectImpl(const TypeSchema* schema);
        ~DynamicObjectImpl();

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
    };
}