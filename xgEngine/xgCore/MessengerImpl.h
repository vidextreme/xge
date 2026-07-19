// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.
#pragma once
#include "xgMessenger.h"

namespace xg
{
    class ScriptTree;

    class MessengerImpl : public Messenger
    {        
        XG_DECLARE_SYSTEM(MessengerImpl, Messenger)
    public:
        explicit MessengerImpl(ScriptTree* tree);

        void Send(const ScriptMessage& msg, const Route& route) override;
        void SendToAll(const ScriptMessage& msg) override;

    private:
        ScriptTree* _tree;

        void RouteDirect(const ScriptMessage& msg, const Route& route);
        void RouteChildren(const ScriptMessage& msg, const Route& route);
        void RouteParents(const ScriptMessage& msg, const Route& route);
        void RouteSiblings(const ScriptMessage& msg, const Route& route);
        void RouteFiltered(const ScriptMessage& msg, const Route& route);
    };
}
