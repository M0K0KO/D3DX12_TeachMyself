#pragma once
#include <imgui.h>

namespace MokoImGui
{
	template<typename F>
	static void DrawProperty(const char* label, F&& func, bool indent = false)
	{
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::AlignTextToFramePadding();

        if (indent) ImGui::Indent(25.0f);
        ImGui::TextUnformatted(label);
        if (indent) ImGui::Unindent(25.0f);

        ImGui::TableSetColumnIndex(1);
        ImGui::PushItemWidth(-1); 
        func();
        ImGui::PopItemWidth();
	}

    static void DrawSection(const char* label)
    {
        ImGui::EndTable(); 

        ImGui::Spacing();
        ImGui::SeparatorText(label);

        ImGui::BeginTable("CompTable", 2, ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_NoSavedSettings);
        ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed, 140.0f);
        ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
    }
}