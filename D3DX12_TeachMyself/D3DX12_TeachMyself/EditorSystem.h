#pragma once
#include "ISystem.h"
#include "CommandContext_DX12.h"
#include <imgui_impl_dx12.h>
#include "DescriptorAllocator.h"
#include "JobSystem.h"
#include "ConsoleSystem.h"
#include "Renderer.h"
#include <ImGuizmo.h>
#include "AssetManager.h"
#include <filesystem>

constexpr const char* PAYLOAD_ENTITY = "MOKO_ENTITY";

struct EditorState
{
	Entity selected = INVALID_ENTITY;
};

class EditorSystem : public ISystem
{
public:
	EditorSystem(GraphicsDevice_DX12* device, Renderer* renderer, HWND hwnd, MokoJob::JobSystem* jobSystem, ConsoleSystem* consoleSystem);
	void Init(SystemContext& ctx) override;
	void Update(EntityScene& scene, float dt, SystemContext& ctx) override;
	void Shutdown(SystemContext& ctx) override;

public:
	void Render(CommandContext& ctx);
	bool TryGetPendingViewportResize(uint32_t& w, uint32_t& h);
	void SetCPUTiming(float updateMs, float renderMs);

private:
	void DrawViewportPanel(EntityScene& scene);

	void DrawHierarchy(EntityScene& scene);
	void DrawHierarchyNode(EntityScene& scene, Entity e);
	bool DrawHierarchyNodeFiltered(EntityScene& scene, Entity e, const std::string& filterLower);
	void DrawEntityContextMenu(EntityScene& scene, Entity e);
	void DrawCreateMenu(EntityScene& scene, Entity parent);

	void DrawInspector(EntityScene& scene);
	void DrawAddComponentMenu(Registry& registry, Entity e);
	void DrawRemoveComponentMenu(Registry& registry, Entity e);
	void DrawViewportStatusBar();

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

	void DrawContentBrowserPanel(EntityScene& scene);
	void DrawBreadCrumb();
	void DrawDirectoryContents(EntityScene& scene);
	void RebuildContentBrowserCache();
	bool ShouldRefreshContentBrowserCache();

	void FlushPendingDestroy(EntityScene& scene);

	void ManipulateSelectedEntity(EntityScene& scene);
	void HandleFileOpen(EntityScene& scene, std::filesystem::path path);

private:

	GraphicsDevice_DX12* m_device = nullptr;
	Renderer* m_renderer = nullptr;
	HWND m_hwnd = nullptr;
	MokoJob::JobSystem* m_jobSystem;
	AssetManager* m_assetManager;
	ConsoleSystem* m_consoleSystem;
	bool m_initialized = false;
	std::filesystem::path m_assetRoot;
	std::filesystem::path m_currentDirectory;

	struct ContentBrowserEntry
	{
		std::string name;
		std::filesystem::path fullPath;
		bool isDirectory = false;
	};

	struct ContentBrowserCache
	{
		std::filesystem::path dir;
		std::vector<ContentBrowserEntry> entries;
	};

	EditorState m_state;

	ImVec2 m_viewportSize = { 0,0 };
	std::optional<std::pair<uint32_t, uint32_t>> m_pendingViewportResize;

	ImGuizmo::OPERATION m_gizmoOp = ImGuizmo::TRANSLATE;
	ImGuizmo::MODE      m_gizmoMode = ImGuizmo::WORLD;

	std::vector<Entity> m_pendingDestroy;
	char m_hierarchySearch[128] = {};
	char m_contentSearch[128] = {};
	bool m_contentOnlyGLTF = false;
	bool m_forceContentBrowserRefresh = true;
	ContentBrowserCache m_contentBrowserCache;
	bool m_viewportStatusBarFolded = false;

	float m_cpuUpdateMsRaw = 0.0f;
	float m_cpuRenderMsRaw = 0.0f;
	float m_cpuUpdateMsSmoothed = 0.0f;
	float m_cpuRenderMsSmoothed = 0.0f;
	bool m_cpuTimingInitialized = false;
	float m_viewportFrameMsSmoothed = 0.0f;
	bool m_viewportTimingInitialized = false;
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