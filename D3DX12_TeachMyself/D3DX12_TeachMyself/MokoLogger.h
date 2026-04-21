#pragma once
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <format>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

enum class LogLevel
{
    Trace,
    Info,
    Warn,
    Error
};

struct LogEntry
{
    LogLevel level;
    std::chrono::system_clock::time_point timestamp;
    std::thread::id thread_id;
    std::string message;
};

class ILogSink
{
public:
    virtual ~ILogSink() = default;
    virtual void Write(const LogEntry& entry) = 0;
};


class MokoLogger
{
public:
    static MokoLogger& Get();
    void AddSink(std::shared_ptr<ILogSink> sink);
    void RemoveSink(const std::shared_ptr<ILogSink>& sink);
    void Log(LogLevel level, std::string message);
    template<typename... Args>
    void LogFormat(LogLevel level, std::format_string<Args...> fmt, Args&&... args)
    {
        Log(level, std::format(fmt, std::forward<Args>(args)...));
    }

private:
    MokoLogger();
    MokoLogger(const MokoLogger&) = delete;
    MokoLogger& operator=(const MokoLogger&) = delete;
private:
    std::mutex m_mutex;
    std::vector<std::shared_ptr<ILogSink>> m_sinks;
};

#define MOKOLOG_TRACE(...) ::MokoLogger::Get().LogFormat(::LogLevel::Trace, __VA_ARGS__)
#define MOKOLOG_INFO(...)  ::MokoLogger::Get().LogFormat(::LogLevel::Info,  __VA_ARGS__)
#define MOKOLOG_WARN(...)  ::MokoLogger::Get().LogFormat(::LogLevel::Warn,  __VA_ARGS__)
#define MOKOLOG_ERROR(...) ::MokoLogger::Get().LogFormat(::LogLevel::Error, __VA_ARGS__)