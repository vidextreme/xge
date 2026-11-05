// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.
#pragma once
#include <cstdint>
#include "xgBase.h"

namespace xg
{

    XG_ENUM(inherit=byte)
    enum class MessageType
    {
        Info,
        Success,
        Warning,
        Critical,
        Error
    };

    using LogCallback = void (*)(MessageType type, const char* message);

    class Logger
    {
    public:
        virtual void Write(int32_t type, const char* message) = 0;
        virtual void AddCallback(LogCallback cb) = 0;
        virtual void RemoveCallback(LogCallback cb) = 0;
        virtual void SetMinimumLevel(int32_t type) = 0;

    protected:
        ~Logger() = default;
    };

    XG_API void SetLogger(Logger* logger);
    XG_API Logger* GetLogger();

    XG_API void Log(MessageType _type, const char* _message, ...);
}
