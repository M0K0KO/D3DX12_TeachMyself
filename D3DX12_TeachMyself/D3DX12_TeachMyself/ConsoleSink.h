#pragma once
#include "MokoLogger.h"
#include <deque>
#include <mutex>

class ConsoleSink final : public ILogSink
{
public:
	explicit ConsoleSink(size_t maxEntries = 2048);

	void Write(const LogEntry& entry) override;

	void CopyEntries(std::deque<LogEntry>& out);
	void Clear();

private:
	std::mutex m_mutex;
	std::deque<LogEntry> m_entries;
	size_t m_maxEntries;
};