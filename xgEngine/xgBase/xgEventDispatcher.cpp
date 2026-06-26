#include "pch.h"
#include "xgEventDispatcher.h"
#include <vector>
#include <algorithm>

namespace xg
{
    struct EventDispatcher::Impl
    {
        std::vector<EventListener*> Listeners;
    };

    EventDispatcher::EventDispatcher()
        : Pimpl(new Impl())
    {}

    EventDispatcher::~EventDispatcher()
    {
        delete Pimpl;
        Pimpl = nullptr;
    }

    void EventDispatcher::AddListener(EventListener* listener)
    {
        if (!listener)
            return;

        auto& list = Pimpl->Listeners;
        if (std::find(list.begin(), list.end(), listener) == list.end())
            list.push_back(listener);
    }

    void EventDispatcher::RemoveListener(EventListener* listener)
    {
        if (!listener)
            return;

        auto& list = Pimpl->Listeners;
        auto it = std::remove(list.begin(), list.end(), listener);
        list.erase(it, list.end());
    }

    void EventDispatcher::Dispatch(const xgEvent& e)
    {
        for (EventListener* listener : Pimpl->Listeners)
        {
            if (listener)
                listener->OnEvent(e);
        }
    }
}
