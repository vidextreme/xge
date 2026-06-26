#pragma once
#include "xgCallback.h"

namespace xg
{
    class CallbackDispatcher
    {
    public:
        virtual ~CallbackDispatcher() = default;

        virtual void Dispatch(Callback* cb, const xgEvent& e) = 0;
        virtual void InvokeDirect(Callback* cb, const xgEvent& e) = 0;
    };
}
