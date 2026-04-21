#pragma once
#include "MokoLogger.h"

class DebugOutputSink final : public ILogSink
{
public:
    void Write(const LogEntry& entry) override;
};