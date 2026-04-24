#include "stdafx.h"
#include "ConsoleSink.h"

ConsoleSink::ConsoleSink(size_t maxEntries)
	: m_maxEntries(maxEntries)
{}

void ConsoleSink::Write(const LogEntry& entry)
{
	std::scoped_lock lock(m_mutex);
	m_entries.push_back(entry);
	while (m_entries.size() > m_maxEntries)
		m_entries.pop_front();
}

void ConsoleSink::CopyEntries(std::deque<LogEntry>&out)
{
	std::scoped_lock lock(m_mutex);
	out = m_entries; 
}

void ConsoleSink::Clear()
{
	std::scoped_lock lock(m_mutex);
	m_entries.clear();
}
