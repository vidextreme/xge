#pragma once
#include "xgEvent.h"

namespace xg
{
    class EventQueue
    {
    public:
        EventQueue();
        ~EventQueue();

        // Push an event into the queue
        void Push(const xgEvent& e);

        // Get number of events currently stored
        int Count() const;

        // Get pointer to contiguous array of events
        const xgEvent* Data() const;

        // Clear all events
        void Clear();

    private:
        // Opaque pointer to internal implementation
        struct Impl;
        Impl* Pimpl;
    };
}
