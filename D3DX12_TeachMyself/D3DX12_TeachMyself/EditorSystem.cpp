#include "EditorSystem.h"
#include "GraphicsDevice_DX12.h"
#include <imgui_impl_win32.h>
#include "MokoLogger.h"
#include "HirerarchyComponent.h"
#include <numbers>
#include "NameComponent.h"
#include "TransformComponent.h"
#include "MeshRendererComponent.h"
#include "DirectionalLightComponent.h"
#include "PointLightComponent.h"
#include "CameraComponent.h"
#include "MokoMath.h"
#include "MokoImGui.h"
#include "MokoAssert.h"
#include "MokoPath.h"
#include "SceneFactory.h"

using namespace MokoImGui;

EditorSystem::EditorSystem(GraphicsDevice_DX12* device, Renderer* renderer, HWND hwnd, MokoJob::JobSystem* jobSystem, ConsoleSystem* consoleSystem)
	: m_device(device), m_renderer(renderer), m_hwnd(hwnd), m_jobSystem(jobSystem), m_consoleSystem(consoleSystem)
{
	m_assetRoot = MokoPath::GetAssetRoot();
	m_currentDirectory = "";
}

void EditorSystem::Init(SystemContext& ctx)
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

	ImGuiStyle& style = ImGui::GetStyle();
	style.WindowRounding = 6.0f;       
	style.FrameRounding = 4.0f;        
	style.GrabRounding = 4.0f;        
	style.PopupRounding = 4.0f;        

	style.ItemSpacing = ImVec2(8, 6);
	style.WindowPadding = ImVec2(10, 10);
	style.FramePadding = ImVec2(5, 5);

	style.WindowBorderSize = 1.0f;
	style.ChildBorderSize = 1.0f;

	ImVec4* colors = ImGui::GetStyle().Colors;
	colors[ImGuiCol_Text] = ImVec4(0.95f, 0.95f, 0.95f, 1.00f);
	colors[ImGuiCol_WindowBg] = ImVec4(0.11f, 0.11f, 0.14f, 1.00f); 
	colors[ImGuiCol_Header] = ImVec4(0.20f, 0.25f, 0.29f, 0.55f);
	colors[ImGuiCol_HeaderHovered] = ImVec4(0.26f, 0.59f, 0.98f, 0.80f);
	colors[ImGuiCol_Button] = ImVec4(0.20f, 0.25f, 0.29f, 1.00f);
	colors[ImGuiCol_ButtonHovered] = ImVec4(0.28f, 0.56f, 1.00f, 1.00f);
	colors[ImGuiCol_FrameBg] = ImVec4(0.16f, 0.16f, 0.21f, 1.00f);
	colors[ImGuiCol_CheckMark] = ImVec4(0.28f, 0.56f, 1.00f, 1.00f);

	MOKO_ASSERT(ImGui_ImplWin32_Init(m_hwnd));

	ImGui_ImplDX12_InitInfo info = {};
	info.Device = m_device->GetDevicePtr();
	info.CommandQueue = m_device->GetCommandQueuePtr();
	info.NumFramesInFlight = FRAMECOUNT;
	info.RTVFormat = m_device->GetSwapChainFormat();
	info.SrvDescriptorHeap = s_srvAllocator->GetHeap();
	info.SrvDescriptorAllocFn = ImGuiAllocCallback;
	info.SrvDescriptorFreeFn = ImGuiFreeCallback;

	MOKO_ASSERT(ImGui_ImplDX12_Init(&info));

	m_initialized = true;
}

void EditorSystem::Update(SystemContext& ctx)
{
	if (!m_initialized) return;

	ImGui_ImplDX12_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();
	ImGuizmo::BeginFrame();

	ImGuiDockNodeFlags dockFlags = ImGuiDockNodeFlags_PassthruCentralNode;
	ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport(), dockFlags);

	DrawHierarchy(*ctx.scene);
	DrawInspector(*ctx.scene);

	DrawProfilerPanel();
	DrawJobSystemPanel();

	DrawViewportPanel(*ctx.scene);

	DrawContentBrowserPanel(*ctx.scene);

	m_consoleSystem->DrawUI();

	ImGui::Render();
}

void EditorSystem::Shutdown(SystemContext& ctx)
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
	MOKO_ASSERT(m_initialized && "Not Initialized Yet");

	auto& dx12Ctx = static_cast<CommandContext_DX12&>(ctx);
	auto* cmdList = reinterpret_cast<ID3D12GraphicsCommandList*>(dx12Ctx.GetNativeHandle());

	auto rtv = m_device->GetCurrentBackBufferRTV();
	cmdList->OMSetRenderTargets(1, &rtv, FALSE, nullptr);

	ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), cmdList);
}

bool EditorSystem::TryGetPendingViewportResize(uint32_t& w, uint32_t& h)
{
	if (!m_pendingViewportResize) return false;
	std::tie(w, h) = *m_pendingViewportResize;
	m_pendingViewportResize.reset();
	return true;
}

void EditorSystem::DrawViewportPanel(EntityScene& scene)
{
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
	ImGui::Begin("Viewport");
	ImGui::PopStyleVar();

	{
		ImGui::BeginChild("ViewportToolbar", ImVec2(0, 28), false,
			ImGuiWindowFlags_NoScrollbar);

		ImGuiIO& io = ImGui::GetIO();
		if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) && !io.WantTextInput)
		{
			if (ImGui::IsKeyPressed(ImGuiKey_W)) m_gizmoOp = ImGuizmo::TRANSLATE;
			if (ImGui::IsKeyPressed(ImGuiKey_E)) m_gizmoOp = ImGuizmo::ROTATE;
			if (ImGui::IsKeyPressed(ImGuiKey_R)) m_gizmoOp = ImGuizmo::SCALE;
		}

		int mode = (int)m_gizmoMode;
		ImGui::RadioButton("World", &mode, ImGuizmo::WORLD); ImGui::SameLine();
		ImGui::RadioButton("Local", &mode, ImGuizmo::LOCAL);
		m_gizmoMode = (ImGuizmo::MODE)mode;

		ImGui::EndChild();
	}

	ImVec2 pos = ImGui::GetCursorScreenPos();
	ImVec2 size = ImGui::GetContentRegionAvail();

	if ((int)size.x != (int)m_viewportSize.x ||
		(int)size.y != (int)m_viewportSize.y)
	{
		if (size.x > 0 && size.y > 0)
		{
			m_pendingViewportResize = { (uint32_t)size.x, (uint32_t)size.y };
			m_viewportSize = size;
		}
	}

	auto gpuHandle = m_device->GetSRVHandle(m_renderer->GetSceneColor()).gpu;
	ImGui::Image((ImTextureID)gpuHandle.ptr, size);

	ImGuizmo::SetOrthographic(false);
	ImGuizmo::SetDrawlist();
	ImGuizmo::SetRect(pos.x, pos.y, size.x, size.y);

	ManipulateSelectedEntity(scene);

	ImGui::End();
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
	ImGui::Separator();
	ImGui::Dummy(ImVec2(0, 5));

	// Transform
	DrawComponent<TransformComponent>("Transform", registry, e, [&](auto& t) {
		bool changed = false;
		DrawProperty("Position", [&]() { changed |= ImGui::DragFloat3("##P", &t.position.x, 0.01f); });

		DrawProperty("Rotation", [&]() {
			XMFLOAT3 euler = QuatToEuler(t.rotation);
			euler.x = XMConvertToDegrees(euler.x); euler.y = XMConvertToDegrees(euler.y); euler.z = XMConvertToDegrees(euler.z);
			if (ImGui::DragFloat3("##R", &euler.x, 0.5f))
			{
				t.rotation = EulerToQuat(XMFLOAT3(XMConvertToRadians(euler.x), XMConvertToRadians(euler.y), XMConvertToRadians(euler.z)));
				changed = true;
			}
			});

		DrawProperty("Scale", [&]() { changed |= ImGui::DragFloat3("##S", &t.scale.x, 0.01f, 0.01f, 100.0f); });
		if (changed) t.dirty = true;
		});

	// Mesh Renderer
	DrawComponent<MeshRendererComponent>("Mesh Renderer", registry, e, [&](auto& mr) {
		DrawProperty("Visible", [&]() { ImGui::Checkbox("##V", &mr.visible); });

		DrawSection("Material Settings");
		DrawProperty("Metallic", [&]() { ImGui::SliderFloat("##M", &mr.material.metallicFactor, 0.0f, 1.0f); }, true);
		DrawProperty("Roughness", [&]() { ImGui::SliderFloat("##R", &mr.material.roughnessFactor, 0.0f, 1.0f); }, true);

		DrawSection("Statistics");
		DrawProperty("Indices", [&]() { ImGui::Text("%u", mr.indexCount); }, true);
		});

	// Directional Light
	DrawComponent<DirectionalLightComponent>("Directional Light", registry, e, [&](auto& dl) {
		DrawProperty("Color", [&]() { ImGui::ColorEdit3("##C", &dl.color.x); });
		DrawProperty("Direction", [&]() {ImGui::DragFloat3("##D", &dl.direction.x, 0.001f, -1.0f, 1.0f); });
		DrawProperty("Intensity", [&]() { ImGui::DragFloat("##I", &dl.intensity, 0.1f, 0.0f, 100.0f); });
		});

	// Point Light
	DrawComponent<PointLightComponent>("Point Light", registry, e, [&](auto& pl) {
		DrawProperty("Color", [&]() { ImGui::ColorEdit3("##C", &pl.color.x); });
		DrawProperty("Radius", [&]() { ImGui::DragFloat("##R", &pl.radius, 0.05f, 0.001f, 50.0f); });
		DrawProperty("Intensity", [&]() { ImGui::DragFloat("##I", &pl.intensity, 0.05f, 0.001f, 50.0f); });
		DrawProperty("Shadow", [&]() { ImGui::Checkbox("##S", &pl.castShadow); });
		});

	// Camera
	DrawComponent<CameraComponent>("Camera", registry, e, [&](auto& cam) {
		DrawProperty("FOV Y", [&]() { ImGui::SliderFloat("##F", &cam.fovY, 0.1f, DirectX::XM_PI); });
		DrawProperty("Near Z", [&]() { ImGui::InputFloat("##N", &cam.nearZ); });
		DrawProperty("Far Z", [&]() { ImGui::InputFloat("##FZ", &cam.farZ); });
		DrawProperty("Main", [&]() { ImGui::Checkbox("##M", &cam.isMain); });
		});

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

void EditorSystem::DrawJobSystemPanel()
{
	if (!ImGui::Begin("Job System")) { ImGui::End(); return; }

	auto snap = m_jobSystem->GetSnapshot();

	ImGui::Text("Workers : %d", snap.workerCount);
	ImGui::Text("Queue   : %llu", (unsigned long long)snap.queueSize);
	ImGui::Text("Submit  : %llu", (unsigned long long)snap.submitted);
	ImGui::Text("Done    : %llu", (unsigned long long)snap.completed);
	uint64_t pending = snap.submitted - snap.completed;
	ImGui::Text("Pending : %llu", (unsigned long long)pending);

	ImGui::Separator();
	ImGui::Text("Worker Utilization:");

	for (int i = 0; i < snap.workerCount; ++i)
	{
		char label[32];
		snprintf(label, sizeof(label), "W%d (%llu)", i,
			(unsigned long long)snap.workerExecuted[i]);

		float ratio = (float)snap.workerBusyRatio[i];
		ImVec4 col = ImVec4(ratio, 1.0f - ratio, 0.2f, 1.0f);
		ImGui::PushStyleColor(ImGuiCol_PlotHistogram, col);
		ImGui::ProgressBar(ratio, ImVec2(200, 0), "");
		ImGui::PopStyleColor();

		ImGui::SameLine();
		ImGui::Text("%s  %.1f%%", label, ratio * 100.0f);
	}

	ImGui::End();
}

void EditorSystem::DrawContentBrowserPanel(EntityScene& scene)
{
	if (ImGui::Begin("Content Browser"))
	{
		DrawBreadCrumb();
		ImGui::Separator();
		DrawDirectoryContents(scene);
	}
	ImGui::End();  // 항상 호출
}

void EditorSystem::DrawBreadCrumb()
{
	if (ImGui::Button("Assets"))
	{
		m_currentDirectory.clear();
	}

	std::filesystem::path accum;
	for (const auto& segment : m_currentDirectory)
	{
		accum /= segment;
		ImGui::SameLine();
		ImGui::TextUnformatted(">");
		ImGui::SameLine();
		if (ImGui::Button(segment.string().c_str()))
		{
			m_currentDirectory = accum;
			break;
		}
	}
}

void EditorSystem::DrawDirectoryContents(EntityScene & scene)
{
	std::filesystem::path fullPath = m_assetRoot / m_currentDirectory;

	std::error_code ec;
	if (!std::filesystem::exists(fullPath, ec))
	{
		ImGui::TextDisabled("(invalid path)");
		return;
	}

	std::vector<std::filesystem::directory_entry> dirs, files;
	for (const auto& entry : std::filesystem::directory_iterator(fullPath, ec))
	{
		if (entry.is_directory(ec)) dirs.push_back(entry);
		else files.push_back(entry);
	}
	

	for (const auto& e : dirs)
	{
		std::string label = "[D] " + e.path().filename().string();
		ImGui::Selectable(label.c_str());
		if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0))
		{
			m_currentDirectory /= e.path().filename();
			return;
		}
	}

	for (const auto& e : files)
	{
		std::string label = e.path().filename().string();
		ImGui::Selectable(label.c_str());
		if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0))
		{
			HandleFileOpen(scene, e.path());
		}
	}
}

void EditorSystem::ManipulateSelectedEntity(EntityScene& scene)
{
	if (m_state.selected == INVALID_ENTITY) return;

	auto& reg = scene.GetRegistry();
	TransformComponent* tf = &reg.Get<TransformComponent>(m_state.selected);
	if (!tf) return;

	Entity camEntity = scene.GetMainCamera();
	if (camEntity == INVALID_ENTITY) return;
	TransformComponent* camTf = &reg.Get<TransformComponent>(camEntity);
	CameraComponent* camCp = &reg.Get<CameraComponent>(camEntity);
	if (!camTf || !camCp) return;

	XMMATRIX view = Camera::GetViewMatrix(camTf->position, camCp->pitch, camCp->yaw);
	XMMATRIX proj = Camera::GetProjectionMatrix(camCp->fovY, camCp->aspect, camCp->nearZ, camCp->farZ);

	XMFLOAT4X4 viewF, projF, worldF;
	XMStoreFloat4x4(&viewF, view);
	XMStoreFloat4x4(&projF, proj);

	XMStoreFloat4x4(&worldF, XMLoadFloat4x4(&tf->worldMatrix));

	ImGuizmo::Manipulate(
		(const float*)&viewF, (const float*)&projF,
		m_gizmoOp, m_gizmoMode,
		(float*)&worldF);

	if (!ImGuizmo::IsUsing()) return;

	XMMATRIX newWorld = XMLoadFloat4x4(&worldF);
	XMMATRIX newLocal = newWorld;

	HierarchyComponent* hier = &reg.Get<HierarchyComponent>(m_state.selected);
	if (hier && hier->parent != INVALID_ENTITY)
	{
		TransformComponent* parentTf = &reg.Get<TransformComponent>(hier->parent);
		if (parentTf)
		{
			XMVECTOR det;
			XMMATRIX invParent = XMMatrixInverse(&det, XMLoadFloat4x4(&parentTf->worldMatrix));
			newLocal = newWorld * invParent;
		}
	}

	XMFLOAT4X4 localF;
	XMStoreFloat4x4(&localF, newLocal);

	float t[3], r[3], s[3];
	ImGuizmo::DecomposeMatrixToComponents((const float*)&localF, t, r, s);

	Transform::SetPosition(reg, m_state.selected, { t[0], t[1], t[2] });
	Transform::SetRotation(reg, m_state.selected, { DegToRad(r[0]), DegToRad(r[1]), DegToRad(r[2]) });
	Transform::SetScale(reg, m_state.selected, { s[0], s[1], s[2] });

	XMStoreFloat4x4(&tf->localMatrix, newLocal);
	XMStoreFloat4x4(&tf->worldMatrix, newWorld);
}

void EditorSystem::HandleFileOpen(EntityScene& scene, std::filesystem::path path)
{
	if (MokoPath::IsLoadableGLTF(path))
	{
		SceneFactory::LoadGLTFToScene(scene, *m_device, path);
	}
	else
	{
		auto ext = path.extension().string();
		MOKOLOG_ERROR("Unsupported file type : {}", ext);
	}
}
