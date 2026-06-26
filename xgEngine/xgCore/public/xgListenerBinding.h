#pragma once
#include "xgEventDispatcher.h"

namespace xg
{
    //
    // ListenerBinding<T>
    //   Binds a member function to the EventListener interface
    //
    template<typename T>
    class ListenerBinding : public EventListener
    {
    public:
        using Method = void (T::*)(const xgEvent&);

        ListenerBinding(T* instance, Method method)
            : _instance(instance)
            , _method(method)
        {}

        void OnEvent(const xgEvent& e) override
        {
            (_instance->*_method)(e);
        }

    private:
        T* _instance;
        Method _method;
    };
}
