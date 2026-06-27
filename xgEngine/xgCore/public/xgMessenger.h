#pragma once

#include "xgBase.h"
#include "xgMessage.h"
#include "xgReflection.h"

namespace xg
{
    class ScriptTree;
    class ScriptModule;
    class MessageCodec;

    //
    // Messenger
    //
    // Public messaging subsystem used by ScriptModules.
    // ScriptEngine exposes this via GetMessenger().
    //
    class Messenger
    {
    public:
        Messenger(ScriptTree* tree, MessageCodec* codec);

        // Send a typed message to a specific module
        void Send(const char* targetId,
            uint32_t typeId,
            const void* payload,
            const TypeInfoBase* type);

        // Deliver a fully encoded message (internal)
        void Deliver(const char* targetId,
            const ScriptMessage& msg);

        // Broadcast to all modules
        void Broadcast(uint32_t typeId,
            const void* payload,
            const TypeInfoBase* type);

    private:
        ScriptTree* _tree;
        MessageCodec* _codec;
    };
}
