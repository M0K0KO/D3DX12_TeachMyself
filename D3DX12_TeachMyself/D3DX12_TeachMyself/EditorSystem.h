#pragma once
#include "ISystem.h"
#include "CommandContext_DX12.h"
#include <imgui_impl_dx12.h>
#include "DescriptorAllocator.h"


struct EditorState
{
	Entity selected = INVALID_ENTITY;
};

class EditorSystem : public ISystem
{
public:
	EditorSystem(GraphicsDevice_DX12* device, HWND hwnd)
		:m_device(device), m_hwnd(hwnd)
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
	void DrawProfilerPanel();

private:
	GraphicsDevice_DX12* m_device = nullptr;
	HWND m_hwnd = nullptr;
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