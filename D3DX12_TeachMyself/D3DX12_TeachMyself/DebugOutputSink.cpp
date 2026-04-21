#include "MokoLoggerHelper.h"
#include "DebugOutputSink.h"
#include <windows.h>

void DebugOutputSink::Write(const LogEntry& entry)
{
    const std::string line = BuildLogLine(entry);
    OutputDebugStringA(line.c_str());
}
