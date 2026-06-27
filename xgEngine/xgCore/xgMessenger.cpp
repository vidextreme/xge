#include "pch.h"
#include "xgMessenger.h"
#include "ScriptTree.h"
#include "xgScriptModule.h"
#include "xgMessageCodec.h"
#include <cstring> // for strcmp

namespace xg
{
    Messenger::Messenger(ScriptTree* tree, MessageCodec* codec)
        : _tree(tree)
        , _codec(codec)
    {}

    void Messenger::Send(const char* targetId,
        uint32_t typeId,
        const void* payload,
        const TypeInfoBase* type)
    {
        ScriptMessage msg = _codec->Encode(typeId, payload, type);
        Deliver(targetId, msg);
    }

    void Messenger::Deliver(const char* targetId,
        const ScriptMessage& msg)
    {
        ScriptModule* m = _tree->FindModuleById(targetId);
        if (m)
            m->OnMessage(msg);
    }

    void Messenger::Broadcast(uint32_t typeId,
        const void* payload,
        const TypeInfoBase* type)
    {
        ScriptMessage msg = _codec->Encode(typeId, payload, type);

        _tree->ForEachModule([&](ScriptModule* m)
            {
                m->OnMessage(msg);
            });
    }
}
