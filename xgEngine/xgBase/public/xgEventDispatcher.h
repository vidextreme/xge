// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.
#pragma once
#include "xgEvent.h"
#include "xgSystem.h"
namespace xg
{
    class EventListener
    {
    public:
        virtual ~EventListener() = default;
        virtual void OnEvent(const xgEvent& e) = 0;
    };

    class EventDispatcher : public System
    {
        XG_DECLARE_BASE_SYSTEM(EventDispatcher)
    public:
        virtual ~EventDispatcher() = default;

        virtual void AddListener(EventListener* listener) = 0;
        virtual void RemoveListener(EventListener* listener) = 0;

        virtual void Dispatch(const xgEvent& e) = 0;
    };
}