#include "pch.h"
#include "JsonMessageCodec.h"

namespace xg
{
    ScriptMessage JsonMessageCodec::Encode(uint32_t typeId,
        const void* obj,
        const TypeInfoBase* type)
    {
        // Create JSON object
        Json json;

        // Create serializer backend that writes into Json
        JsonSerializer serializer(json);

        // Use reflection to serialize the object into Json
        type->SerializeFn(serializer, obj);
        // ^ You already have Serialize<T>(serializer, obj)
        //   This line assumes TypeInfoBase stores a function pointer.
        //   If not, replace with: Serialize<T>(serializer, *(const T*)obj)

        // Create memory stream for payload
        auto mem = CreateMemoryStream();

        // Save JSON into the stream
        json.Save(*mem);

        // Build ScriptMessage
        ScriptMessage msg;
        msg.Type = typeId;
        msg.Payload = std::move(mem);
        return msg;
    }

    bool JsonMessageCodec::Decode(const ScriptMessage& msg,
        void* outObj,
        const TypeInfoBase* type)
    {
        if (!msg.Payload)
            return false;

        Json json;

        // Load JSON from stream
        if (!json.Load(*msg.Payload))
            return false;

        // Create deserializer backend
        JsonDeserializer deserializer(json);

        // Use reflection to populate the object
        type->DeserializeFn(deserializer, outObj);
        // ^ Same note as above: if TypeInfoBase doesn't store function pointers,
        //   call Deserialize<T>(deserializer, *(T*)outObj)

        return true;
    }
}

