#pragma once
#include "xgEventDispatcher.h"

namespace xg
{
    template<typename T>
    class MemberCallback : public IEventCallback
    {
    public:
        using Method = void(T::*)(const xgEvent&);

        MemberCallback(T* instance, Method method)
            : _instance(instance), _method(method)
        {
        }

        void Invoke(const xgEvent& e) override
        {
            (_instance->*_method)(e);
        }

    private:
        T* _instance;
        Method _method;
    };
}
