#include "stdafx.h"
#include "Application.h"
#include "AssetLoader.h"
#include "TextureLoader.h"
#include "MokoTime.h"
#include "MokoLogger.h"
#include "EntityScene.h"
#include "MokoMath.h"
#include "TransformSystem.h"
#include "CameraControllerSystem.h"
#include "DirectionalLightComponent.h"
#include "TransformComponent.h"
#include "PointLightComponent.h"
#include "CameraComponent.h"
#include "MeshRendererComponent.h"
#include "JobSystem.h"
#include "JobGroup.h"
#include "ConsoleSystem.h"
#include "SceneFactory.h"
#include "BuiltinAssets.h"
#include "MokoPath.h"
#include "GraphicsDevice_DX12.h"
#include "AssetManager.h"

Application::Application()
	:
	wnd(1920, 1080, L"Moko Engine")
{

}

Application::~Application()
{
	m_device->WaitForGpu();
	SystemContext ctx = { .window = &wnd, .input = &m_inputState, .assetManager = m_assetManager.get()};
	m_systemManager->ShutdownAll(ctx);
}

int Application::Run()
{
	Init();

	while (true)
	{
		if (auto exitCode = Window::ProcessMessages())
			return *exitCode;

		MokoTime::Tick();

		Update();
		Render();
	}
}

void Application::Init()
{
	HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);

	m_device = std::make_unique<GraphicsDevice_DX12>();
	m_device->Initialize(wnd.GetHWND(), wnd.GetWidth(), wnd.GetHeight());

	m_assetManager = std::make_unique<AssetManager>(m_device.get());
	m_assetManager->Initialize(m_device.get());

	m_systemManager = std::make_unique<SystemManager>();

	m_jobSystem = m_systemManager->Add<MokoJob::JobSystem>(0);

	m_systemManager->Add<CameraControllerSystem>();
	m_systemManager->Add<TransformSystem>();
	
	auto* consoleSystem = m_systemManager->Add<ConsoleSystem>();

	auto* dx12 = static_cast<GraphicsDevice_DX12*>(m_device.get());
	m_editorSystem = m_systemManager->Add<EditorSystem>(dx12, &m_renderer, wnd.GetHWND(), m_jobSystem, consoleSystem);

	m_systemContext.window = &wnd;
	m_systemContext.input = &m_inputState;
	m_systemContext.assetManager = m_assetManager.get();
	m_systemManager->InitAll(m_systemContext);


	SceneFactory::CreateDirLight(m_ecsScene);
	SceneFactory::CreatePointLight(m_ecsScene);
	{
		Entity e = m_ecsScene.CreateSceneEntity("Main Camera");
		auto& cam = m_ecsScene.GetRegistry().Add<CameraComponent>(e);
		cam.aspect = float(m_device->GetWidth()) / float(m_device->GetHeight());
		cam.isMain = true;
		auto& t = m_ecsScene.GetRegistry().Get<TransformComponent>(e);
		t.position = { 0.0f, 1.0f, -5.0f };
	}

	m_renderer.Init(m_device.get());

	wnd.SetResizeCallback([this](uint32_t w, uint32_t h) {
		if (w > 0 && h > 0 && m_initialized)
		{
			m_pendingWidth = w;
			m_pendingHeight = h;
			m_pendingSwapchainResize = true;
		}
	});

	m_initialized = true;

	MOKOLOG_INFO("Application Initialized");
}

void Application::Update()
{
	if (m_pendingSwapchainResize)
	{
		m_device->WaitForGpu();
		m_device->ResizeSwapChain(m_pendingWidth, m_pendingHeight);
		m_pendingSwapchainResize = false;
	}

	m_inputState.Capture(wnd);

	HandleKeyboardEvents();

	m_systemContext.window = &wnd;
	m_systemContext.input = &m_inputState;
	m_systemContext.assetManager = m_assetManager.get();
	m_systemManager->UpdateAll(m_ecsScene, MokoTime::GetDeltaTime(), m_systemContext);

	uint32_t vw = 0, vh = 0;
	if (m_editorSystem->TryGetPendingViewportResize(vw, vh))
	{
		vw = std::max(vw, 1u);
		vh = std::max(vh, 1u);

		m_device->WaitForGpu();
		m_renderer.OnViewportResize(vw, vh);

		Entity mainCam = m_ecsScene.GetMainCamera();
		auto& cam = m_ecsScene.GetRegistry().Get<CameraComponent>(mainCam);
		cam.aspect = (float)vw / (float)vh;
	}
}

void Application::Render()
{
	auto& ctx = m_device->BeginFrame();

	/*
	auto t0 = std::chrono::high_resolution_clock::now();
	RenderScene renderScene = ExtractRenderScene();
	auto t1 = std::chrono::high_resolution_clock::now();
	double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
	MOKOLOG_INFO("ExtractRenderScene: {:.3f} ms", ms);
	*/
	RenderScene renderScene = ExtractRenderScene();

	m_renderer.Render(m_device.get(), ctx, renderScene);
	m_editorSystem->Render(ctx);

	m_device->EndFrame();

}

RenderScene Application::ExtractRenderScene()
{
	RenderScene rs;
	rs.sceneAABBMax = m_ecsScene.GetSceneAABBMax();
	rs.sceneAABBMin = m_ecsScene.GetSceneAABBMin();

	auto meshView = m_ecsScene.GetRegistry().GetView<TransformComponent, MeshRendererComponent>();
	rs.renderObjects.reserve(meshView.SizeHint());

	for (auto [e, t, mr] : meshView)
	{
		if (!mr.visible) continue;
		if (!mr.mesh.IsValid()) continue;

		const MeshAsset* mesh = m_assetManager->Meshes().Get(mr.mesh);
		if (!mesh) continue;

		XMFLOAT4X4 worldT;
		XMStoreFloat4x4(&worldT, XMMatrixTranspose(XMLoadFloat4x4(&t.worldMatrix)));

		for (size_t i = 0; i < mr.submeshIndices.size(); i++)
		{
			uint32_t subIdx = mr.submeshIndices[i];
			if (subIdx >= mesh->submeshes.size()) continue;
			const Submesh& sub = mesh->submeshes[subIdx];

			const Material* mat = nullptr;
			if (i < mr.materials.size())
			{
				mat = m_assetManager->Materials().Get(mr.materials[i]);
			}
			if (!mat)
			{
				mat = m_assetManager->Materials().Get(BuiltinAssets::GetDefaultMaterial());
				if (!mat) continue;
			}

			RenderObject obj;
			obj.world = worldT;
			obj.vertexBuffer = mesh->vb;
			obj.indexBuffer = mesh->ib;
			obj.indexOffset = sub.indexOffset;
			obj.indexCount = sub.indexCount;
			obj.material = m_assetManager->Materials().ToGPU(*mat);
			obj.aabbMin = sub.aabb.min;
			obj.aabbMax = sub.aabb.max;
			rs.renderObjects.push_back(obj);
		}
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
		XMMATRIX view = Camera::GetViewMatrix(pos, cam.pitch, cam.yaw);
		XMMATRIX proj = Camera::GetProjectionMatrix(cam.fovY, cam.aspect, cam.nearZ, cam.farZ);

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

	if (m_inputState.IsKeyDown(VK_CONTROL) && m_inputState.WasKeyPressed('S'))
	{
		SceneSerializer::Save(m_ecsScene, MokoPath::GetAssetRoot() / "testScene.json");
	}
	else if (m_inputState.IsKeyDown(VK_CONTROL) && m_inputState.WasKeyPressed('L'))
	{
		SceneSerializer::Load(m_ecsScene, MokoPath::GetAssetRoot() / "testScene.json");
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

