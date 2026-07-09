// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.
#pragma once
#include "xgEvent.h"

namespace xg
{
    //
    // Base interface for script callbacks
    //
    class Callback
    {
    public:
        virtual ~Callback() = default;
        virtual void Invoke(const xgEvent& e) = 0;
    };
}
