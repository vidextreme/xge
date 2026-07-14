// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.
#include "pch.h"
#include "EventDispatcherImpl.h"


namespace xg
{
    EventDispatcherImpl::EventDispatcherImpl()
    {}

    EventDispatcherImpl::~EventDispatcherImpl()
    {
    }

    void EventDispatcherImpl::AddListener(EventListener* listener)
    {
        if (!listener)
            return;

        auto& list = Listeners;
        if (std::find(list.begin(), list.end(), listener) == list.end())
            list.push_back(listener);
    }

    void EventDispatcherImpl::RemoveListener(EventListener* listener)
    {
        if (!listener)
            return;

        auto& list = Listeners;
        auto it = std::remove(list.begin(), list.end(), listener);
        list.erase(it, list.end());
    }

    void EventDispatcherImpl::Dispatch(const xgEvent& e)
    {
        for (EventListener* listener : Listeners)
        {
            if (listener)
                listener->OnEvent(e);
        }
    }
}
