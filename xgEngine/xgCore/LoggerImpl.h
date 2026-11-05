// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.
#pragma once
#include "xgLog.h"

namespace xg
{
    class LoggerImpl final : public Logger
    {
    public:
        void Write(int32_t type, const char* message) override;
        void AddCallback(LogCallback cb) override;
        void RemoveCallback(LogCallback cb) override;
        void SetMinimumLevel(int32_t type) override;

    private:
        std::vector<LogCallback> _callbacks;
        std::mutex _mutex;
        int32_t _minimumLevel = static_cast<int32_t>(MessageType::Info);
    };
}
