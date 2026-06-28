#pragma once
#include <cstdint>
#include "xgStream.h"
#include "xgReflection.h"
#include "xgTypeInfo.h"
namespace xg
{
	class ScriptMessage;
    //
    // MessageCodec
    //   Abstract backend for encoding/decoding ScriptMessage payloads.
    //
    class MessageCodec
    {
    public:
        virtual ~MessageCodec() = default;

        // Encode a reflected object into a ScriptMessage
        virtual ScriptMessage Encode(uint32_t typeId,
            const void* obj,
            const TypeInfoBase* type) = 0;

        // Decode a ScriptMessage payload into a reflected object
        virtual bool Decode(const ScriptMessage& msg,
            void* outObj,
            const TypeInfoBase* type) = 0;
    };
}
