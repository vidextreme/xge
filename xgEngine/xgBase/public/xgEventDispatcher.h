// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.
#pragma once
#include "xgEvent.h"

namespace xg
{
    class EventListener
    {
    public:
        virtual ~EventListener() = default;
        virtual void OnEvent(const xgEvent& e) = 0;
    };

    class EventDispatcher
    {
    public:
        EventDispatcher();
        ~EventDispatcher();

        void AddListener(EventListener* listener);
        void RemoveListener(EventListener* listener);

        void Dispatch(const xgEvent& e);

    private:
        struct Impl;
        Impl* Pimpl;
    };
}
