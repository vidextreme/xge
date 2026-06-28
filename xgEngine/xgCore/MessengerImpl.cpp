#include "pch.h"
#include "MessengerImpl.h"
#include "ScriptTree.h"
#include "xgScriptModule.h"
#include "xgMessageCodec.h"

namespace xg
{
    MessengerImpl::MessengerImpl(ScriptTree* tree, MessageCodec* codec)
        : _tree(tree)
        , _codec(codec)
    {}

    void MessengerImpl::Send(const char* targetId,
        uint32_t typeId,
        const void* payload,
        const TypeInfoBase* type)
    {
        ScriptMessage msg = _codec->Encode(typeId, payload, type);
        Deliver(targetId, msg);
    }

    void MessengerImpl::Deliver(const char* targetId,
        const ScriptMessage& msg)
    {
        ScriptModule* module = _tree->FindModuleById(targetId);
        if (module)
            module->OnMessage(msg);
    }

    void MessengerImpl::Broadcast(uint32_t typeId,
        const void* payload,
        const TypeInfoBase* type)
    {
        ScriptMessage msg = _codec->Encode(typeId, payload, type);

        _tree->ForEachModule([&](ScriptModule* module)
            {
                module->OnMessage(msg);
            });
    }
}
