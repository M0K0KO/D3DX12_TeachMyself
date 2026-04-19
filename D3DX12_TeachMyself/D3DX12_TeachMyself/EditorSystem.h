#pragma once
#include "ISystem.h"
#include "CommandContext_DX12.h"
#include <imgui_impl_dx12.h>
#include "DescriptorAllocator.h"
#include "JobSystem.h"

struct EditorState
{
	Entity selected = INVALID_ENTITY;
};

class EditorSystem : public ISystem
{
public:
	EditorSystem(GraphicsDevice_DX12* device, HWND hwnd, MokoJob::JobSystem* jobSystem)
		:m_device(device), m_hwnd(hwnd), m_jobSystem(jobSystem)
	{}
	void Init(EntityScene&) override;
	void Update(EntityScene& scene, float dt, const SystemContext& ctx) override;
	void Shutdown(EntityScene& scene) override;

public:
	void Render(CommandContext& ctx);

private:
	void DrawHierarchy(EntityScene& scene);
	void DrawHierarchyNode(EntityScene& scene, Entity e);

	void DrawInspector(EntityScene& scene);
	void DrawNameComponent(Registry& registry, Entity e);
	void DrawTransformComponent(Registry& registry, Entity e);
	void DrawMeshRendererComponent(Registry& registry, Entity e);
	void DrawDirectionalLightComponent(Registry& registry, Entity e);
	void DrawPointLightComponent(Registry& registry, Entity e);
	void DrawCameraComponent(Registry& registry, Entity e);

	void DrawProfilerPanel();
	void DrawJobSystemPanel();

private:
	GraphicsDevice_DX12* m_device = nullptr;
	HWND m_hwnd = nullptr;
	MokoJob::JobSystem* m_jobSystem;
	bool m_initialized = false;

	EditorState m_state;
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