// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.
#pragma once
#include "xgEvent.h"
#include "xgSystem.h"
namespace xg
{
    class EventQueue : public System
    {
        XG_DECLARE_BASE_SYSTEM(EventQueue)
    public:
        virtual ~EventQueue() = default;

        // Push an event into the queue
        virtual void Push(const xgEvent& e) = 0;

        // Get number of events currently stored
        virtual int Count() const = 0;

        // Get pointer to contiguous array of events
        virtual const xgEvent* Data() const = 0;

        // Clear all events
        virtual void Clear() = 0;
    };
}