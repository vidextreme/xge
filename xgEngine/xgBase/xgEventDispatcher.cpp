#include "pch.h"
#include "xgEventDispatcher.h"
#include <vector>
#include <algorithm>

namespace xg
{
    struct EventDispatcher::Impl
    {
        std::vector<IEventCallback*> Listeners;
    };

    EventDispatcher::EventDispatcher()
        : Pimpl(new Impl())
    {
    }

    EventDispatcher::~EventDispatcher()
    {
        delete Pimpl;
    }

    void EventDispatcher::AddListener(IEventCallback* cb)
    {
        if (!cb)
            return;

        Pimpl->Listeners.push_back(cb);
    }

    void EventDispatcher::RemoveListener(IEventCallback* cb)
    {
        auto& vec = Pimpl->Listeners;
        vec.erase(std::remove(vec.begin(), vec.end(), cb), vec.end());
    }

    void EventDispatcher::Dispatch(const xgEvent& e)
    {
        for (IEventCallback* cb : Pimpl->Listeners)
        {
            if (cb)
                cb->Invoke(e);
        }
    }
}

