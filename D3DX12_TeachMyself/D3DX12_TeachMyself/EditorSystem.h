#pragma once
#include "ISystem.h"
#include "CommandContext_DX12.h"
#include <imgui_impl_dx12.h>
#include "DescriptorAllocator.h"
#include "JobSystem.h"
#include "Renderer.h"

struct EditorState
{
	Entity selected = INVALID_ENTITY;
};

class EditorSystem : public ISystem
{
public:
	EditorSystem(GraphicsDevice_DX12* device, Renderer* renderer, HWND hwnd, MokoJob::JobSystem* jobSystem)
		:m_device(device), m_renderer(renderer),  m_hwnd(hwnd), m_jobSystem(jobSystem)
	{}
	void Init(SystemContext& ctx) override;
	void Update(SystemContext& ctx) override;
	void Shutdown(SystemContext& ctx) override;

public:
	void Render(CommandContext& ctx);
	bool TryGetPendingViewportResize(uint32_t& w, uint32_t h);

private:
	void DrawViewportPanel();

	void DrawHierarchy(EntityScene& scene);
	void DrawHierarchyNode(EntityScene& scene, Entity e);

	void DrawInspector(EntityScene& scene);

	template<typename T, typename F>
	void DrawComponent(const char* name, Registry& registry, Entity e, F&& func)
	{
		if (!registry.Has<T>(e)) return;

		ImGui::PushID(name);
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4, 4));
		bool open = ImGui::CollapsingHeader(name, ImGuiTreeNodeFlags_DefaultOpen);
		ImGui::PopStyleVar();

		if (open)
		{
			if (ImGui::BeginTable("CompTable", 2, ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_NoSavedSettings))
			{
				ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed, 100.0f);
				ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);

				func(registry.Get<T>(e));

				ImGui::EndTable();
			}
		}
		ImGui::PopID();
		ImGui::Dummy(ImVec2(0, 5)); 
	}

	void DrawNameComponent(Registry& registry, Entity e);

	void DrawProfilerPanel();
	void DrawJobSystemPanel();

private:
	GraphicsDevice_DX12* m_device = nullptr;
	Renderer* m_renderer = nullptr;
	HWND m_hwnd = nullptr;
	MokoJob::JobSystem* m_jobSystem;
	bool m_initialized = false;

	EditorState m_state;

	ImVec2 m_viewportSize = { 0,0 };
	std::optional<std::pair<uint32_t, uint32_t>> m_pendingViewportResize;
};



static DescriptorAllocator* s_srvAllocator = nullptr;

static void ImGuiAllocCallback(ImGui_ImplDX12_InitInfo*, D3D12_CPU_DESCRIPTOR_HANDLE* outCpu, D3D12_GPU_DESCRIPTOR_HANDLE* outGpu)
{
	auto h = s_srvAllocator->Allocate();
	*outCpu = h.cpu;
	*outGpu = h.gpu;
}

static void ImGuiFreeCallback(ImGui_ImplDX12_InitInfo*,
	D3D12_CPU_DESCRIPTOR_HANDLE cpu,
	D3D12_GPU_DESCRIPTOR_HANDLE)
{
	s_srvAllocator->FreeByCpuHandle(cpu);
}