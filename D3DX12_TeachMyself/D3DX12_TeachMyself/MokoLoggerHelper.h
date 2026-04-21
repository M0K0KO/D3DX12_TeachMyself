#pragma once
#include "MokoLogger.h"

const char* ToString(LogLevel level);
std::string FormatTimestamp(const std::chrono::system_clock::time_point& tp);
std::string FormatThreadId(const std::thread::id& id);
std::string BuildLogLine(const LogEntry& entry);
