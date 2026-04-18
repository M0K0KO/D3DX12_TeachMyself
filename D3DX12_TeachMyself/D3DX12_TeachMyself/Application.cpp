#include "Application.h"
#include "AssetLoader.h"
#include "TextureLoader.h"
#include "MokoTime.h"
#include "MokoLog.h"
#include "EntityScene.h"
#include "components.h"
#include "MokoMath.h"
#include "TransformSystem.h"
#include "CameraControllerSystem.h"

Application::Application()
	:
	wnd(1920, 1080, L"Moko Engine")
{

}

Application::~Application()
{
	m_systemManager->ShutdownAll(m_ecsScene);
}

int Application::Run()
{
	Init();

	while (true)
	{
		if (auto exitCode = Window::ProcessMessages())
			return *exitCode;

		MokoTime::Tick();

		float dt = MokoTime::GetDeltaTime();
		float fps = 1.0f / dt;

		LOG_INFO(" FPS: %.1f | Frame: %.2f ms\n", fps, dt * 1000.0f);

		auto t0 = MokoTime::Now();

		Update();

		auto t1 = MokoTime::Now();

		Render();

		LOG_INFO("[CPU Time] Update: %.3f ms | Render: %.3f ms\n", MokoTime::ElapsedMS(t0, t1), MokoTime::ElapsedMS(t1));
	}
}

void Application::Init()
{
	HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);

	m_device = std::make_unique<GraphicsDevice_DX12>();
	m_device->Initialize(wnd.GetHWND(), wnd.GetWidth(), wnd.GetHeight());

	AssetLoader loader;
	auto sponzaScene = loader.LoadGLTF("../Model/Sponza/Sponza.gltf");

	BufferDesc vbDesc = { 
		static_cast<uint32_t>(sponzaScene.vertices.size() * sizeof(Mesh::Vertex)), 
		sizeof(Mesh::Vertex),
		BufferUsage::Vertex, MemoryAccess::CpuWrite};
	auto vb = m_device->CreateBuffer(vbDesc, sponzaScene.vertices.data());

	BufferDesc ibDesc = { 
		static_cast<uint32_t>(sponzaScene.indices.size() * sizeof(uint32_t)),
		sizeof(uint32_t), 
		BufferUsage::Index, MemoryAccess::CpuWrite };
	auto ib = m_device->CreateBuffer(ibDesc, sponzaScene.indices.data());

	std::vector<TextureHandle> gpuTextures;

	m_device->BeginTextureUpload();
	for (auto& tex : sponzaScene.textures)
	{
		gpuTextures.push_back(m_device->LoadTexture(tex.path));
	}
	m_device->FlushTextureUploads();


	uint8_t white[] = { 255, 255, 255, 255 };
	uint8_t normal[] = { 128, 128, 255, 255 }; 
	uint8_t mr[] = { 0, 128, 0, 255 };
	TextureHandle defaultWhite = m_device->CreateTexture({ 1, 1, Format::R8G8B8A8_UNORM, TextureUsage::ShaderResource }, white);
	TextureHandle defaultNormal = m_device->CreateTexture({ 1, 1, Format::R8G8B8A8_UNORM, TextureUsage::ShaderResource }, normal);
	TextureHandle defaultMR = m_device->CreateTexture({ 1, 1, Format::R8G8B8A8_UNORM, TextureUsage::ShaderResource }, mr);

	m_ecsScene.Initialize();

	for (auto& subMesh : sponzaScene.subMeshes)
	{
		Entity e = m_ecsScene.CreateEntity();

		auto& t = m_ecsScene.GetRegistry().Get<TransformComponent>(e);
		XMStoreFloat4x4(&t.worldMatrix, XMMatrixIdentity());

		auto& mr = m_ecsScene.GetRegistry().Add<MeshRendererComponent>(e);
		mr.vertexBuffer = vb;
		mr.indexBuffer = ib;
		mr.indexOffset = subMesh.indexOffset;
		mr.indexCount = subMesh.indexCount;

		auto& mat = sponzaScene.materials[subMesh.materialIndex];
		mr.material.baseColor = (mat.baseColorTexture >= 0) ? gpuTextures[mat.baseColorTexture] : defaultWhite;
		mr.material.normal = (mat.normalTexture >= 0) ? gpuTextures[mat.normalTexture] : defaultNormal;
		mr.material.metallicRoughness = (mat.metallicRoughnessTexture >= 0) ? gpuTextures[mat.metallicRoughnessTexture] : defaultMR;
		mr.material.alphaMode = mat.alphaMode;
		mr.material.alphaCutoff = mat.alphaCutoff;
		mr.material.metallicFactor = mat.metallicFactor;
		mr.material.roughnessFactor = mat.roughnessFactor;
		mr.visible = true;

		mr.aabbMin = subMesh.aabbMin;
		mr.aabbMax = subMesh.aabbMax;
	}
	m_ecsScene.SetSceneAABB(sponzaScene.sceneAABBMin, sponzaScene.sceneAABBMax);

	{
		Entity e = m_ecsScene.CreateEntity();
		auto& dl = m_ecsScene.GetRegistry().Add<DirectionalLightComponent>(e);
		dl.direction = { 0.0f, -1.0f, 0.0f };
		dl.color = { 1,1,1 };
		dl.intensity = 1.0f;
	}

	{
		Entity e = m_ecsScene.CreateEntity();
		auto& t = m_ecsScene.GetRegistry().Get<TransformComponent>(e);
		t.position = { 0.0f, 2.0f, 0.0f };

		auto& pl = m_ecsScene.GetRegistry().Add<PointLightComponent>(e);
		pl.color = { 0.0f, 0.7f, 0.7f };
		pl.radius = 30.0f;
		pl.intensity = 2.0f;
	}

	{
		Entity e = m_ecsScene.CreateEntity();
		auto& cam = m_ecsScene.GetRegistry().Add<CameraComponent>(e);
		cam.aspect = float(m_device->GetWidth()) / float(m_device->GetHeight());
		cam.isMain = true;
		auto& t = m_ecsScene.GetRegistry().Get<TransformComponent>(e);
		t.position = { 0.0f, 1.0f, -5.0f };
	}

	m_systemManager = std::make_unique<SystemManager>();
	m_systemManager->Add<CameraControllerSystem>();
	m_systemManager->Add<TransformSystem>();

	auto* dx12 = static_cast<GraphicsDevice_DX12*>(m_device.get());
	m_editorSystem = m_systemManager->Add<EditorSystem>(dx12, wnd.GetHWND());

	m_systemManager->InitAll(m_ecsScene);

	m_renderer.Init(m_device.get());

	wnd.SetResizeCallback([this](uint32_t w, uint32_t h) {
		if (w > 0 && h > 0 && m_initialized)
		{
			m_pendingWidth = w;
			m_pendingHeight = h;
			m_needsResize = true;
		}
	});

	m_initialized = true;
}

void Application::Update()
{
	m_inputState.Capture(wnd);

	HandleKeyboardEvents();
	HandleDebugModeInput();

	SystemContext ctx = { .window = &wnd, .input = &m_inputState };
	m_systemManager->UpdateAll(m_ecsScene, MokoTime::GetDeltaTime(), ctx);

}

void Application::Render()
{
	auto& ctx = m_device->BeginFrame();

	if (m_needsResize)
	{
		m_device->ResizeSwapChain(m_pendingWidth, m_pendingHeight);
		m_renderer.OnResize(m_pendingWidth, m_pendingHeight);
		auto view = m_ecsScene.GetRegistry().GetView<CameraComponent>();
		for (auto [e, cam] : view)
		{
			if (cam.isMain) cam.aspect = float(m_pendingWidth) / float(m_pendingHeight);
		}
		m_needsResize = false;
	}

	RenderScene renderScene = ExtractRenderScene();
	m_renderer.Render(m_device.get(), ctx, renderScene);
	m_editorSystem->Render(ctx);

	m_device->EndFrame();

}

RenderScene Application::ExtractRenderScene()
{
	RenderScene rs;
	rs.renderObjects.reserve(512);
	rs.sceneAABBMax = m_ecsScene.GetSceneAABBMax();
	rs.sceneAABBMin = m_ecsScene.GetSceneAABBMin();

	auto meshView = m_ecsScene.GetRegistry().GetView<TransformComponent, MeshRendererComponent>();
	for (auto [e, t, mr] : meshView)
	{
		if (!mr.visible) continue;
		RenderObject obj;
		obj.vertexBuffer = mr.vertexBuffer;
		obj.indexBuffer = mr.indexBuffer;
		obj.indexOffset = mr.indexOffset;
		obj.indexCount = mr.indexCount;
		obj.material = mr.material;
		obj.world = t.worldMatrix;
		obj.aabbMin = mr.aabbMin;
		obj.aabbMax = mr.aabbMax;
		rs.renderObjects.push_back(obj);
	}

	auto dirLightView = m_ecsScene.GetRegistry().GetView<DirectionalLightComponent>();
	for (auto [e, dl] : dirLightView)
	{
		rs.frameData.DirectionalLightDir = dl.direction;
		rs.frameData.DirectionalLightColor = dl.color;
		rs.frameData.DirectionalLightIntensity = dl.intensity;
		rs.frameData.Ambient = dl.ambient;
		break;
	}

	auto pointLightView = m_ecsScene.GetRegistry().GetView<TransformComponent, PointLightComponent>();
	for (auto [e, t, pl] : pointLightView)
	{
		PointLightData data;
		data.Position = { t.worldMatrix._41, t.worldMatrix._42, t.worldMatrix._43 };
		data.Color = pl.color;
		data.Radius = pl.radius;
		data.Intensity = pl.intensity;
		rs.frameData.PointLights.push_back(data);
	}
	rs.frameData.PointLightCount = (int)rs.frameData.PointLights.size();

	auto camView = m_ecsScene.GetRegistry().GetView<TransformComponent, CameraComponent>();
	for (auto [e, t, cam] : camView)
	{
		if (!cam.isMain) continue;
		XMFLOAT3 pos = { t.worldMatrix._41, t.worldMatrix._42, t.worldMatrix._43 };
		XMMATRIX view = CameraMath::GetViewMatrix(pos, cam.pitch, cam.yaw);
		XMMATRIX proj = CameraMath::GetProjectionMatrix(cam.fovY, cam.aspect, cam.nearZ, cam.farZ);

		XMStoreFloat4x4(&rs.frameData.ViewMatrix, view);
		XMStoreFloat4x4(&rs.frameData.ProjMatrix, proj);
		rs.frameData.CameraPos = pos;
		break;
	}

	return rs;
}

void Application::HandleKeyboardEvents()
{
	if (m_inputState.WasKeyPressed(VK_ESCAPE))
	{
		if (wnd.CursorEnabled())
		{
			wnd.DisableCursor();
			wnd.mouse.EnableRaw();
		}
		else
		{
			wnd.EnableCursor();
			wnd.mouse.DisableRaw();
		}
	}
}
void  Application::HandleDebugModeInput()
{
	if (m_inputState.WasKeyPressed('1'))
	{
		m_renderer.ChangeDebugMode(DebugMode::PBR_Enabled);
		wnd.SetTitle(L"DebugMode :: PBR ENABLED");
	}
	else if (m_inputState.WasKeyPressed('2'))
	{
		m_renderer.ChangeDebugMode(DebugMode::PBR_Disabled);
		wnd.SetTitle(L"DebugMode :: PBR DISABLED");
	}
	else if (m_inputState.WasKeyPressed('3'))
	{
		m_renderer.ChangeDebugMode(DebugMode::DepthTexture);
		wnd.SetTitle(L"DebugMode :: DEPTH");
	}
	else if (m_inputState.WasKeyPressed('4'))
	{
		m_renderer.ChangeDebugMode(DebugMode::Albedo);
		wnd.SetTitle(L"DebugMode :: ALBEDO");
	}
	else if (m_inputState.WasKeyPressed('5'))
	{
		m_renderer.ChangeDebugMode(DebugMode::Normal);
		wnd.SetTitle(L"DebugMode :: NORMAL");
	}
	else if (m_inputState.WasKeyPressed('6'))
	{
		m_renderer.ChangeDebugMode(DebugMode::MR);
		wnd.SetTitle(L"DebugMode :: METALLIC_ROUGHNESS");
	}
	else if (m_inputState.WasKeyPressed('7'))
	{
		m_renderer.ChangeDebugMode(DebugMode::SSAO_ENABLED);
		wnd.SetTitle(L"DebugMode :: SSAO ENABLED");
	}
	else if (m_inputState.WasKeyPressed('8'))
	{
		m_renderer.ChangeDebugMode(DebugMode::SSAO_DISABLED);
		wnd.SetTitle(L"DebugMode :: SSAO DISABLED");
	}
}

