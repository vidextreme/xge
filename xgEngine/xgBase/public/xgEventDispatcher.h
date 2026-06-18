#pragma once
#include "xgEvent.h"

namespace xg
{
    class IEventCallback
    {
    public:
        virtual ~IEventCallback() = default;
        virtual void Invoke(const xgEvent& e) = 0;
    };

    class EventDispatcher
    {
    public:
        EventDispatcher();
        ~EventDispatcher();

        void AddListener(IEventCallback* cb);
        void RemoveListener(IEventCallback* cb);

        void Dispatch(const xgEvent& e);

    private:
        struct Impl;
        Impl* Pimpl;
    };
}
