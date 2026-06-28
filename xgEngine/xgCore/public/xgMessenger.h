#pragma once

#include <cstdint>

namespace xg
{
    struct ScriptMessage;
    class MessageCodec;
    class TypeInfoBase;

    class Messenger
    {
    public:
        virtual ~Messenger() = default;

        virtual void Send(const char* targetId,
            uint32_t typeId,
            const void* payload,
            const TypeInfoBase* type) = 0;

        virtual void Deliver(const char* targetId,
            const ScriptMessage& msg) = 0;

        virtual void Broadcast(uint32_t typeId,
            const void* payload,
            const TypeInfoBase* type) = 0;

        virtual MessageCodec* GetCodec() const = 0;
    };
}
