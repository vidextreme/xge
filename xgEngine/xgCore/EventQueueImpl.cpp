// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.
#include "pch.h"
#include "EventQueueImpl.h"
#include <vector>

namespace xg
{
    EventQueueImpl::EventQueueImpl()
    {
    }

    EventQueueImpl::~EventQueueImpl()
    {
    }

    void EventQueueImpl::Push(const xgEvent& e)
    {
        Events.push_back(e);
    }

    int EventQueueImpl::Count() const
    {
        return static_cast<int>(Events.size());
    }

    const xgEvent* EventQueueImpl::Data() const
    {
        return Events.data();
    }

    void EventQueueImpl::Clear()
    {
        Events.clear();
    }
}
