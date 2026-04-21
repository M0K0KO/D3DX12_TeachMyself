#pragma once
#include "ISystem.h"
#include "MokoLogger.h"
#include "ConsoleSink.h"
#include <imgui.h>
#include <memory>
#include <deque>

class ConsoleSystem final : public ISystem
{
public:
	void Init(SystemContext& ctx) override;
	void Update(SystemContext& ctx) override;
	void Shutdown(SystemContext& ctx) override;

	void DrawUI();

private:
	bool PassesLevelFilter(LogLevel level) const;
	ImVec4 ColorForLevel(LogLevel level) const;

private:
	std::shared_ptr<ConsoleSink> m_sink;
	std::deque<LogEntry> m_displayCache;

	bool m_autoScroll = true;
	bool m_showTrace = true;
	bool m_showInfo = true;
	bool m_showWarn = true;
	bool m_showError = true;
	char m_filterBuf[128]{};
};