// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.
#pragma once
#include "xgEventDispatcher.h"

namespace xg
{
	class EventDispatcherImpl : public EventDispatcher
    {
    public:
        EventDispatcherImpl();
        ~EventDispatcherImpl();

        void AddListener(EventListener* listener) override;
        void RemoveListener(EventListener* listener) override;

        void Dispatch(const xgEvent& e) override;

    private:
        std::vector<EventListener*> Listeners;
    };
}
