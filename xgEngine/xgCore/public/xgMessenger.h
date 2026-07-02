#pragma once
#include "xgRoute.h"

namespace xg
{
    struct ScriptMessage;
    class ScriptModule;

    class Messenger
    {
    public:
        virtual ~Messenger() = default;

        virtual void Send(const ScriptMessage& msg, const Route& route) = 0;
        virtual void SendToAll(const ScriptMessage& msg) = 0;
    };
}
