#pragma once
#include "MokoLogger.h"

const char* ToString(LogLevel level)
{
    switch (level)
    {
    case LogLevel::Trace: return "TRACE";
    case LogLevel::Info:  return "INFO";
    case LogLevel::Warn:  return "WARN";
    case LogLevel::Error: return "ERROR";
    default:              return "UNKNOWN";
    }
}

std::string FormatTimestamp(const std::chrono::system_clock::time_point& tp)
{
    const auto timeT = std::chrono::system_clock::to_time_t(tp);

    std::tm localTm{};
    localtime_s(&localTm, &timeT);

    char buffer[64]{};
    std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &localTm);

    const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        tp.time_since_epoch()) % 1000;

    return std::format("{}.{:03}", buffer, ms.count());
}

std::string FormatThreadId(const std::thread::id& id)
{
    std::ostringstream oss;
    oss << id;
    return oss.str();
}

std::string BuildLogLine(const LogEntry& entry)
{
    return std::format(
        "[{}][{}][Thread:{}] {}\n",
        FormatTimestamp(entry.timestamp),
        ToString(entry.level),
        FormatThreadId(entry.thread_id),
        entry.message
    );
}
