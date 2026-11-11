#pragma once
#include "xgBase.h"
#include "xgScriptMessage.h"


namespace xg
{
    struct TypeSchema;
    XG_MODULE_EXPORT bool XG_Encode_Binary_Generic(const void* object,
        const TypeSchema* schema,
        ScriptMessage& out);

    XG_MODULE_EXPORT bool XG_Decode_Binary_Generic(const ScriptMessage& msg,
        const TypeSchema* schema,
        void* outObject);
}
