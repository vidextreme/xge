#pragma once
#include "xgMessageCodec.h"
#include "xgJson.h"
#include "xgJsonBackend.h"
#include "xgReflectionSerialization.h"
#include "xgStream.h"   // your MemoryStream implementation

namespace xg
{
    class JsonMessageCodec : public MessageCodec
    {
    public:
        ScriptMessage Encode(uint32_t typeId,
            const void* obj,
            const TypeInfoBase* type) override;

        bool Decode(const ScriptMessage& msg,
            void* outObj,
            const TypeInfoBase* type) override;
    };
}
