#include "EditorSystem.h"
#include "GraphicsDevice_DX12.h"
#include <imgui_impl_win32.h>
#include "MokoLog.h"
#include "HirerarchyComponent.h"
#include <numbers>
#include "NameComponent.h"
#include "TransformComponent.h"
#include "MeshRendererComponent.h"
#include "DirectionalLightComponent.h"
#include "PointLightComponent.h"
#include "CameraComponent.h"
#include "MokoMath.h"

void EditorSystem::Init(EntityScene&)
{
	s_srvAllocator = &m_device->GetCbvSrvUavAllocator();

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
	io.ConfigFlags |= ImGuiConfigFlags_DpiEnableScaleFonts;
	io.ConfigFlags |= ImGuiConfigFlags_DpiEnableScaleViewports;

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

	//ImGui::DockSpaceOverViewport();
	ImGuiDockNodeFlags dockFlags = ImGuiDockNodeFlags_PassthruCentralNode;
	ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport(), dockFlags);

	DrawHierarchy(scene);
	DrawInspector(scene);

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

void EditorSystem::DrawHierarchy(EntityScene& scene)
{
	ImGui::Begin("Hierarchy");
	auto& hier = scene.GetRegistry().Get<HierarchyComponent>(scene.GetRoot());
	Entity c = hier.firstChild;
	while (c != INVALID_ENTITY)
	{
		DrawHierarchyNode(scene, c);
		c = scene.GetRegistry().Get<HierarchyComponent>(c).nextSibling;
	}

	if (ImGui::IsMouseClicked(0) && ImGui::IsWindowHovered() && !ImGui::IsAnyItemHovered())
	{
		m_state.selected = INVALID_ENTITY;
	}

	ImGui::End();
}

void EditorSystem::DrawHierarchyNode(EntityScene& scene, Entity e)
{
	auto& registry = scene.GetRegistry();
	auto& hier = registry.Get<HierarchyComponent>(e);

	const char* name = "Entity";
	if (registry.Has<NameComponent>(e))
		name = registry.Get<NameComponent>(e).name.c_str();

	ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;

	bool hasChildren = (hier.firstChild != INVALID_ENTITY);
	if (!hasChildren) flags |= ImGuiTreeNodeFlags_Leaf;
	if (m_state.selected == e) flags |= ImGuiTreeNodeFlags_Selected;

	bool open = ImGui::TreeNodeEx((void*)(uintptr_t)(e.index | (uint64_t)e.generation << 32), flags, "%s", name);

	if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen())
		m_state.selected = e;

	if (open)
	{
		Entity c = hier.firstChild;
		while (c != INVALID_ENTITY)
		{
			DrawHierarchyNode(scene, c);
			c = registry.Get<HierarchyComponent>(c).nextSibling;
		}
		ImGui::TreePop();
	}
}

void EditorSystem::DrawInspector(EntityScene& scene)
{
	ImGui::Begin("Inspector");

	if (m_state.selected == INVALID_ENTITY)
	{
		ImGui::TextDisabled("No Entity Selected");
		ImGui::End();
		return;
	}

	Entity e = m_state.selected;
	auto& registry = scene.GetRegistry();

	DrawNameComponent(registry, e);
	DrawTransformComponent(registry, e);
	DrawMeshRendererComponent(registry, e);
	DrawDirectionalLightComponent(registry, e);
	DrawPointLightComponent(registry, e);
	DrawCameraComponent(registry, e);

	ImGui::End();
}

void EditorSystem::DrawNameComponent(Registry& registry, Entity e)
{
	if (!registry.Has<NameComponent>(e)) return;

	auto& n = registry.Get<NameComponent>(e);
	char buf[128];
	strncpy_s(buf, n.name.c_str(), sizeof(buf));

	if (ImGui::InputText("Name", buf, sizeof(buf)))
	{
		n.name = buf;
	}
}

void EditorSystem::DrawTransformComponent(Registry & registry, Entity e)
{
	if (!registry.Has<TransformComponent>(e)) return;

	if (ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen))
	{
		auto& t = registry.Get<TransformComponent>(e);

		bool changed = false;
		changed |= ImGui::DragFloat3("Position", &t.position.x, 0.01f);

		XMFLOAT3 euler = QuatToEuler(t.rotation);
		euler.x = XMConvertToDegrees(euler.x);
		euler.y = XMConvertToDegrees(euler.y);
		euler.z = XMConvertToDegrees(euler.z);

		if (ImGui::DragFloat3("Rotation", &euler.x, 0.5f))
		{
			euler.x = XMConvertToRadians(euler.x);
			euler.y = XMConvertToRadians(euler.y);
			euler.z = XMConvertToRadians(euler.z);

			t.rotation = EulerToQuat(euler);
			changed = true;
		}

		changed |= ImGui::DragFloat3("Scale", &t.scale.x, 0.01f, 0.01f, 100.0f);

		if (changed) t.dirty = true;  
	}
}

void EditorSystem::DrawMeshRendererComponent(Registry & registry, Entity e)
{
	if (!registry.Has<MeshRendererComponent>(e)) return;

	if (ImGui::CollapsingHeader("Mesh Renderer", ImGuiTreeNodeFlags_DefaultOpen))
	{
		auto& mr = registry.Get<MeshRendererComponent>(e);

		ImGui::Checkbox("Visible", &mr.visible);

		ImGui::Separator();
		ImGui::Text("Material");
		ImGui::SliderFloat("Metallic", &mr.material.metallicFactor, 0.0f, 1.0f);
		ImGui::SliderFloat("Roughness", &mr.material.roughnessFactor, 0.0f, 1.0f);
		ImGui::SliderFloat("Alpha Cutoff", &mr.material.alphaCutoff, 0.0f, 1.0f);

		ImGui::Text("Index Count: %u", mr.indexCount);
	}
}

void EditorSystem::DrawDirectionalLightComponent(Registry & registry, Entity e)
{
	if (!registry.Has<DirectionalLightComponent>(e)) return;

	if (ImGui::CollapsingHeader("Directional Light", ImGuiTreeNodeFlags_DefaultOpen))
	{
		auto& dl = registry.Get<DirectionalLightComponent>(e);
		ImGui::DragFloat3("Direction", &dl.direction.x, 0.01f, -1.0f, 1.0f);
		ImGui::ColorEdit3("Color", &dl.color.x);
		ImGui::DragFloat("Intensity", &dl.intensity, 0.1f, 0.0f, 100.0f);
		ImGui::DragFloat("Ambient", &dl.ambient, 0.001f, 0.001f, 1.0f);
	}
}

void EditorSystem::DrawPointLightComponent(Registry & registry, Entity e)
{
	if (!registry.Has<PointLightComponent>(e)) return;

	if (ImGui::CollapsingHeader("Point Light", ImGuiTreeNodeFlags_DefaultOpen))
	{
		auto& pl = registry.Get<PointLightComponent>(e);
		ImGui::ColorEdit3("Color", &pl.color.x);
		ImGui::DragFloat("Intensity", &pl.intensity, 0.1f, 0.0f, 100.0f);
		ImGui::DragFloat("Radius", &pl.radius, 0.05f, 0.001f, 50.0f);
		ImGui::Checkbox("Cast Shadow", &pl.castShadow);
	}
}

void EditorSystem::DrawCameraComponent(Registry & registry, Entity e)
{
	if (!registry.Has<CameraComponent>(e)) return;

	if (ImGui::CollapsingHeader("Camera", ImGuiTreeNodeFlags_DefaultOpen))
	{
		auto& cam = registry.Get<CameraComponent>(e);
		ImGui::SliderFloat("FOV Y", &cam.fovY, 0.1f, DirectX::XM_PI);
		ImGui::InputFloat("Aspect", &cam.aspect);

		ImGui::InputFloat("Near Z", &cam.nearZ);
		ImGui::InputFloat("Far Z", &cam.farZ);

		ImGui::SliderFloat("Pitch", &cam.pitch, -DirectX::XM_PIDIV2, DirectX::XM_PIDIV2);
		ImGui::SliderFloat("Yaw", &cam.yaw, -DirectX::XM_PI, DirectX::XM_PI);

		ImGui::Checkbox("Main Camera", &cam.isMain);
	}
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
