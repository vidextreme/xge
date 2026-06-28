#pragma once
#include "xgStream.h"
#include "xgTypeInfo.h"
#include "xgMessageCodec.h"
namespace xg
{
	
    struct ScriptMessage
    {
        uint32_t Type = 0;
        std::unique_ptr<Stream> Payload;

        // Decode into an existing object
        template<typename T>
        bool DecodeInto(T& outObj, MessageCodec* codec) const
        {
            return codec->Decode(*this, &outObj, GetTypeInfo<T>());
        }

        // Decode and return by value
        template<typename T>
        T Decode(MessageCodec* codec) const
        {
            T obj{};
            codec->Decode(*this, &obj, GetTypeInfo<T>());
            return obj;
        }
    };
}
