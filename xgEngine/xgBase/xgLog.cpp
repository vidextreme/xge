// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.
#include "pch.h"
#include "public/xgLog.h"
namespace xg
{
    static Logger* _logger = nullptr;

    void SetLogger(Logger* logger)
    {
        _logger = logger;
    }

    Logger* GetLogger()
    {
        return _logger;
    }

    void Log(MessageType type, const char* message, ...)
    {
        char msgBuffer[2048];

        va_list args;
        va_start(args, message);
        vsnprintf(msgBuffer, sizeof(msgBuffer), message, args);
        va_end(args);

        if (_logger)
            _logger->Write(static_cast<int32_t>(type), msgBuffer);
        else
            std::fprintf(stderr, "[xg] %s\n", msgBuffer);
    }

}
