// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.
#include "pch.h"
#include "LoggerImpl.h"
#include <chrono>
#include <ctime>
#include <format>
#include <iterator>

namespace xg
{
    static std::string GetTimestamp()
    {
        using namespace std::chrono;

        auto now = system_clock::now();
        auto ms = duration_cast<milliseconds>(now.time_since_epoch()) % 1000;

        std::time_t t = system_clock::to_time_t(now);

        std::tm tm;
#if defined(_WIN32)
        localtime_s(&tm, &t);
#else
        localtime_r(&t, &tm);
#endif

        char buffer[64];
        std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &tm);

        char finalBuffer[80];
        snprintf(finalBuffer, sizeof(finalBuffer), "%s.%03lld", buffer, (long long)ms.count());

        return finalBuffer;
    }

    static const char* LevelName(MessageType type)
    {
        switch (type)
        {
        case MessageType::Info:     return "INFO";
        case MessageType::Warning:  return "WARN";
        case MessageType::Success:  return "SUCC";
        case MessageType::Critical: return "CRIT";
        default:                    return "ERR";
        }
    }

    void LoggerImpl::Write(int32_t type, const char* message)
    {
        if (!message)
            return;

        std::vector<LogCallback> callbacks;
        {
            std::lock_guard<std::mutex> lock(_mutex);
            if (type < _minimumLevel)
                return;
            callbacks = _callbacks;
        }

        if (callbacks.empty())
            return;

        const std::string_view text(message, strnlen(message, 2047));
        const MessageType level = static_cast<MessageType>(type);

        std::string finalMessage;
        finalMessage.reserve(text.size() + 40);
        std::format_to(std::back_inserter(finalMessage), "[{}] {}: {}", GetTimestamp(), LevelName(level), text);

        for (LogCallback cb : callbacks)
            cb(level, finalMessage.c_str());
    }

    void LoggerImpl::AddCallback(LogCallback cb)
    {
        if (!cb)
            return;

        std::lock_guard<std::mutex> lock(_mutex);
        if (std::find(_callbacks.begin(), _callbacks.end(), cb) == _callbacks.end())
            _callbacks.push_back(cb);
    }

    void LoggerImpl::RemoveCallback(LogCallback cb)
    {
        if (!cb)
            return;

        std::lock_guard<std::mutex> lock(_mutex);
        _callbacks.erase(std::remove(_callbacks.begin(), _callbacks.end(), cb), _callbacks.end());
    }

    void LoggerImpl::SetMinimumLevel(int32_t type)
    {
        std::lock_guard<std::mutex> lock(_mutex);
        _minimumLevel = type;
    }
}
