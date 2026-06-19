#pragma once
#include "xgBase.h"

namespace xg
{

    XG_ENUM(inherit=byte)
    enum class MessageType
    {
        Info,
        Warning,
        Critical,
        Error
    };

    using LogCallback = void (*)(MessageType type, const char* message);

    XG_API void AddLogCallback(LogCallback callback);
    XG_API void RemoveLogCallback(LogCallback callback);
    XG_API void SetMinimumLogLevel(MessageType type);
    XG_API void Log(MessageType _type, const char* _message, ...);
}