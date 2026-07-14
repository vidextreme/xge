// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.
#pragma once
#include "xgEventQueue.h"

namespace xg
{
	class EventQueueImpl : public EventQueue
    {
    public:
        EventQueueImpl();
        ~EventQueueImpl();

        // Push an event into the queue
        void Push(const xgEvent& e);

        // Get number of events currently stored
        int Count() const;

        // Get pointer to contiguous array of events
        const xgEvent* Data() const;

        // Clear all events
        void Clear();
    private:
        std::vector<xgEvent> Events;
    };
}