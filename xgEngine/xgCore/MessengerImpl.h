#pragma once

#include "xgMessenger.h"

namespace xg
{
    class ScriptTree;

    class MessengerImpl : public Messenger
    {
    public:
        MessengerImpl(ScriptTree* tree, MessageCodec* codec);

        void Send(const char* targetId,
            uint32_t typeId,
            const void* payload,
            const TypeInfoBase* type) override;

        void Deliver(const char* targetId,
            const ScriptMessage& msg) override;

        void Broadcast(uint32_t typeId,
            const void* payload,
            const TypeInfoBase* type) override;

        MessageCodec* GetCodec() const override { return _codec; }

    private:
        ScriptTree* _tree;
        MessageCodec* _codec;
    };
}
