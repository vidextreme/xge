#pragma once
#include "xgReflection.h"

namespace xg
{
    class TypeRegistry
    {
    public:
        static void Register(const TypeInfoBase* info);
        static const TypeInfoBase* Find(const char* name);
    };

}
