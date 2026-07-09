// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.
#pragma once
#include "xgBase.h"
#include "xgScriptMessage.h"


namespace xg
{
	class TypeSchema;
    // Generic JSON encoder for all reflected types
    XG_MODULE_EXPORT bool Encode_JSON_Generic(const void* object,
        const TypeSchema* schema,
        ScriptMessage& out);

    // Generic JSON decoder for all reflected types
    XG_MODULE_EXPORT bool Decode_JSON_Generic(const ScriptMessage& msg,
        const TypeSchema* schema,
        void* outObject);
}
