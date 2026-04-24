#include "stdafx.h"
#include "MokoLogger.h"

#include <ctime>
#include <sstream>

#include "DebugOutputSink.h"

MokoLogger& MokoLogger::Get()
{
    static MokoLogger instance;
    return instance;
}

MokoLogger::MokoLogger()
{
    AddSink(std::make_shared<DebugOutputSink>());
}

void MokoLogger::AddSink(std::shared_ptr<ILogSink> sink)
{
    if (!sink)
        return;

    std::scoped_lock lock(m_mutex);
    m_sinks.push_back(std::move(sink));
}

void MokoLogger::RemoveSink(const std::shared_ptr<ILogSink>& sink)
{
    std::scoped_lock lock(m_mutex);

    std::erase_if(m_sinks, [&](const auto& s) { return s == sink; });
}

void MokoLogger::Log(LogLevel level, std::string message)
{
    LogEntry entry{
            .level = level,
            .timestamp = std::chrono::system_clock::now(),
            .thread_id = std::this_thread::get_id(),
            .message = std::move(message)
    };

    std::vector<std::shared_ptr<ILogSink>> sinksCopy;
    {
        std::scoped_lock lock(m_mutex);
        sinksCopy = m_sinks;
    }

    for (const auto& sink : sinksCopy)
    {
        if (sink)
            sink->Write(entry);
    }
}
