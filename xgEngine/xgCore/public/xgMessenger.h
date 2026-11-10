// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.
#pragma once
#include "xgRoute.h"
#include "xgSubsystem.h"
namespace xg
{
    struct ScriptMessage;
    class ScriptModule;

	class Messenger : public Subsystem
    {        
        XG_DECLARE_BASE_SUBSYSTEM(Messenger)
    public:
        virtual ~Messenger() = default;

        virtual void Send(const ScriptMessage& msg, const Route& route) = 0;
        virtual void SendToAll(const ScriptMessage& msg) = 0;
    };
}
