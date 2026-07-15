#pragma once
#include "xgBase.h"
#include "xgScriptMessage.h"


namespace xg
{
    class TypeSchema;
    XG_MODULE_EXPORT bool Encode_Binary_Generic(const void* object,
        const TypeSchema* schema,
        ScriptMessage& out);

    XG_MODULE_EXPORT bool Decode_Binary_Generic(const ScriptMessage& msg,
        const TypeSchema* schema,
        void* outObject);
}
