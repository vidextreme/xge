#include "pch.h"
#include "public/xgLog.h"
namespace xg
{
    static std::vector<LogCallback> g_LogCallbacks;
    static std::mutex g_LogMutex;
    MessageType g_EngineLogLevel = MessageType::Info;

    static std::string GetTimestamp()
    {
        using namespace std::chrono;

        // Current time
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

    void AddLogCallback(LogCallback callback)
    {
        if (!callback)
            return;

        std::lock_guard<std::mutex> lock(g_LogMutex);

        // Prevent duplicates
        auto it = std::find(g_LogCallbacks.begin(), g_LogCallbacks.end(), callback);
        if (it == g_LogCallbacks.end())
            g_LogCallbacks.push_back(callback);
    }

    void RemoveLogCallback(LogCallback callback)
    {
        if (!callback)
            return;

        std::lock_guard<std::mutex> lock(g_LogMutex);

        g_LogCallbacks.erase(
            std::remove(g_LogCallbacks.begin(), g_LogCallbacks.end(), callback),
            g_LogCallbacks.end()
        );
    }


    void SetMinimumLogLevel(MessageType type)
    {
        g_EngineLogLevel = type;
    }

    void Log(MessageType type, const char* message, ...)
    {
        if (type < g_EngineLogLevel)
            return;

        char msgBuffer[2048];

        va_list args;
        va_start(args, message);
        vsnprintf(msgBuffer, sizeof(msgBuffer), message, args);
        va_end(args);

        // Build final message with timestamp
        std::string finalMessage;
        finalMessage.reserve(2200);
        std::format_to(std::back_inserter(finalMessage), "[{}] {}: {}",
            GetTimestamp(),
            (type == MessageType::Info) ? "INFO" :
            (type == MessageType::Warning) ? "WARN" :
            (type == MessageType::Success) ? "SUCC" :
            (type == MessageType::Critical) ? "CRIT" : "ERR",
            msgBuffer);

        std::lock_guard<std::mutex> lock(g_LogMutex);

        for (auto& cb : g_LogCallbacks)
            cb(type, finalMessage.c_str());
    }

}