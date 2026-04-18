#include "EditorSystem.h"
#include "GraphicsDevice_DX12.h"
#include <imgui_impl_win32.h>

void EditorSystem::Init(EntityScene&)
{
	s_srvAllocator = &m_device->GetCbvSrvUavAllocator();

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

	ImGui::StyleColorsDark();

	assert(ImGui_ImplWin32_Init(m_hwnd));

	ImGui_ImplDX12_InitInfo info = {};
	info.Device = m_device->GetDevicePtr();
	info.CommandQueue = m_device->GetCommandQueuePtr();
	info.NumFramesInFlight = FRAMECOUNT;
	info.RTVFormat = m_device->GetSwapChainFormat();
	info.SrvDescriptorHeap = s_srvAllocator->GetHeap();
	info.SrvDescriptorAllocFn = ImGuiAllocCallback;
	info.SrvDescriptorFreeFn = ImGuiFreeCallback;

	assert(ImGui_ImplDX12_Init(&info));

	m_initialized = true;
}

void EditorSystem::Update(EntityScene& scene, float dt, const SystemContext& ctx)
{
	if (!m_initialized) return;

	ImGui_ImplDX12_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();

	DrawProfilerPanel();

	ImGui::Render();
}

void EditorSystem::Shutdown(EntityScene & scene)
{
	if (!m_initialized) return;

	m_device->WaitForGpu();  

	ImGui_ImplDX12_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();

	s_srvAllocator = nullptr;
	m_initialized = false;
}

void EditorSystem::Render(CommandContext& ctx)
{
	assert(m_initialized && "Not Initialized Yet");

	auto& dx12Ctx = static_cast<CommandContext_DX12&>(ctx);
	auto* cmdList = reinterpret_cast<ID3D12GraphicsCommandList*>(dx12Ctx.GetNativeHandle());

	auto rtv = m_device->GetCurrentBackBufferRTV();
	cmdList->OMSetRenderTargets(1, &rtv, FALSE, nullptr);
	
	ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), cmdList);
}

void EditorSystem::DrawProfilerPanel()
{
	if (ImGui::Begin("GPU Profiler"))
	{
		const auto results = m_device->GetGPUProfiler()->GetLastFrameResults();
		float total = 0.0f;
		for (const auto& r : results)
		{
			ImGui::Text("%-20s %6.3f ms", r.name.c_str(), r.ms);
			total += r.ms;
		}
		ImGui::Separator();
		ImGui::Text("Total: %6.3f ms", total);
	}
	ImGui::End();
}
