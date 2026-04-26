#include "stdafx.h"
#include "ConsoleSystem.h"
#include "MokoLoggerHelper.h"

void ConsoleSystem::Init(SystemContext& ctx)
{
    m_sink = std::make_shared<ConsoleSink>(2048);
    MokoLogger::Get().AddSink(m_sink);
}

void ConsoleSystem::Update(EntityScene& scene, float dt, SystemContext& ctx)
{}

void ConsoleSystem::Shutdown(SystemContext & ctx)
{
    MokoLogger::Get().RemoveSink(m_sink);
    m_sink.reset();
}

void ConsoleSystem::DrawUI()
{
    if (!ImGui::Begin("Console")) { ImGui::End(); return; }

    m_sink->CopyEntries(m_displayCache);

    if (ImGui::Button("Clear")) m_sink->Clear();
    ImGui::SameLine(); ImGui::Checkbox("Auto-scroll", &m_autoScroll);
    ImGui::SameLine(); ImGui::Checkbox("Trace", &m_showTrace);
    ImGui::SameLine(); ImGui::Checkbox("Info", &m_showInfo);
    ImGui::SameLine(); ImGui::Checkbox("Warn", &m_showWarn);
    ImGui::SameLine(); ImGui::Checkbox("Error", &m_showError);
    ImGui::InputText("Filter", m_filterBuf, sizeof(m_filterBuf));

    ImGui::Separator();
    ImGui::BeginChild("ScrollRegion", {}, false, ImGuiWindowFlags_HorizontalScrollbar);

    ImGuiTableFlags flags =
        ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV;

    ImGui::PushStyleColor(ImGuiCol_TableHeaderBg, ImVec4(0.18f, 0.18f, 0.18f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_TableBorderStrong, ImVec4(0.55f, 0.55f, 0.55f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_TableBorderLight, ImVec4(0.35f, 0.35f, 0.35f, 1.0f));

    if (ImGui::BeginTable("ConsoleTable", 3, flags))
    {
        ImGui::TableSetupColumn("Time", ImGuiTableColumnFlags_WidthFixed, 150.0f);
        ImGui::TableSetupColumn("Level", ImGuiTableColumnFlags_WidthFixed, 65.0f);
        ImGui::TableSetupColumn("Message", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableHeadersRow();

        for (const auto& e : m_displayCache)
        {
            if (!PassesLevelFilter(e.level)) continue;
            if (m_filterBuf[0] && e.message.find(m_filterBuf) == std::string::npos) continue;

            ImVec4 color = ColorForLevel(e.level);

            ImGui::TableNextRow();

            ImGui::TableSetColumnIndex(0);
            ImGui::TextUnformatted(FormatTimestamp(e.timestamp).c_str());

            ImGui::TableSetColumnIndex(1);
            ImGui::PushStyleColor(ImGuiCol_Text, color);
            ImGui::TextUnformatted(ToString(e.level));
            ImGui::PopStyleColor();

            ImGui::TableSetColumnIndex(2);
            ImGui::TextUnformatted(e.message.c_str());
        }

        ImGui::EndTable();
    }
    ImGui::PopStyleColor(3);

    if (m_autoScroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 1.0f)
        ImGui::SetScrollHereY(1.0f);

    ImGui::EndChild();
    ImGui::End();
}


bool ConsoleSystem::PassesLevelFilter(LogLevel level) const
{
    switch (level)
    {
    case LogLevel::Trace: return m_showTrace;
    case LogLevel::Info:  return m_showInfo;
    case LogLevel::Warn:  return m_showWarn;
    case LogLevel::Error: return m_showError;
    default:                          return true;
    }
}

ImVec4 ConsoleSystem::ColorForLevel(LogLevel level) const
{
    switch (level)
    {
    case LogLevel::Trace:
        return ImVec4(0.65f, 0.65f, 0.65f, 1.0f);

    case LogLevel::Info:
        return ImVec4(0.90f, 0.90f, 0.90f, 1.0f);

    case LogLevel::Warn:
        return ImVec4(1.00f, 0.82f, 0.20f, 1.0f);

    case LogLevel::Error:
        return ImVec4(1.00f, 0.35f, 0.35f, 1.0f); 

    default:
        return ImVec4(1.00f, 1.00f, 1.00f, 1.0f);
    }
}