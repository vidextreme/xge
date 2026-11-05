// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.
#include "pch.h"
#include "public/xgLog.h"
namespace xg
{
    static Logger* s_logger = nullptr;

    void SetLogger(Logger* logger)
    {
        s_logger = logger;
    }

    Logger* GetLogger()
    {
        return s_logger;
    }

    void Log(MessageType type, const char* message, ...)
    {
        char msgBuffer[2048];

        va_list args;
        va_start(args, message);
        vsnprintf(msgBuffer, sizeof(msgBuffer), message, args);
        va_end(args);

        if (s_logger)
            s_logger->Write(static_cast<int32_t>(type), msgBuffer);
        else
            std::fprintf(stderr, "[xg] %s\n", msgBuffer);
    }

}
