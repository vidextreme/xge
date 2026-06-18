#include "pch.h"
#include "xgEventQueue.h"
#include <vector>

namespace xg
{
    struct EventQueue::Impl
    {
        std::vector<xgEvent> Events;
    };

    EventQueue::EventQueue()
        : Pimpl(new Impl())
    {
    }

    EventQueue::~EventQueue()
    {
        delete Pimpl;
    }

    void EventQueue::Push(const xgEvent& e)
    {
        Pimpl->Events.push_back(e);
    }

    int EventQueue::Count() const
    {
        return static_cast<int>(Pimpl->Events.size());
    }

    const xgEvent* EventQueue::Data() const
    {
        return Pimpl->Events.data();
    }

    void EventQueue::Clear()
    {
        Pimpl->Events.clear();
    }
}
