#include "Application.h"
#include "AssetLoader.h"
#include "TextureLoader.h"
#include "MokoTime.h"
#include "MokoLog.h"

Application::Application()
	:
	wnd(1920, 1080, L"Moko Engine")
{

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

	m_scene.cam = {};
	m_scene.cam.SetAspectRatio(m_device->GetWidth(), m_device->GetHeight());

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

	for (auto& subMesh : sponzaScene.subMeshes)
	{
		Scene::RenderObject obj = {};
		obj.vertexBuffer = vb;
		obj.indexBuffer = ib;
		obj.indexOffset = subMesh.indexOffset;
		obj.indexCount = subMesh.indexCount;

		auto& mat = sponzaScene.materials[subMesh.materialIndex];

		obj.material.baseColor = (mat.baseColorTexture >= 0)
			? gpuTextures[mat.baseColorTexture] : defaultWhite;
		obj.material.normal = (mat.normalTexture >= 0)
			? gpuTextures[mat.normalTexture] : defaultNormal;
		obj.material.metallicRoughness = (mat.metallicRoughnessTexture >= 0)
			? gpuTextures[mat.metallicRoughnessTexture] : defaultMR;
		obj.material.alphaMode = mat.alphaMode;
		obj.material.alphaCutoff = mat.alphaCutoff;
		obj.material.metallicFactor = mat.metallicFactor;
		obj.material.roughnessFactor = mat.roughnessFactor;

		XMStoreFloat4x4(&obj.world, XMMatrixIdentity());
		m_scene.renderObjects.push_back(obj);
		m_scene.sceneAABBMax = sponzaScene.sceneAABBMax;
		m_scene.sceneAABBMin = sponzaScene.sceneAABBMin;
	}

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
	HandleKeyboardEvents();
	HandleDebugModeInput();
	HandleCameraMovement(MokoTime::GetDeltaTime());
}

void Application::Render()
{
	if (m_needsResize)
	{
		m_device->ResizeSwapChain(m_pendingWidth, m_pendingHeight);
		m_renderer.OnResize(m_pendingWidth, m_pendingHeight);
		m_scene.cam.SetAspectRatio(m_pendingWidth, m_pendingHeight);
		m_needsResize = false;
		return;
	}
	m_renderer.Render(m_device.get(), m_scene);
}

void  Application::HandleKeyboardEvents()
{
	while (const auto e = wnd.kbd.ReadKey())
	{
		if (!e->IsPress())
		{
			continue;
		}

		switch (e->GetCode())
		{
		case VK_ESCAPE:
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
			break;
		}
	}

}
void  Application::HandleDebugModeInput()
{
	if (wnd.kbd.KeyIsPressed('1'))
	{
		m_renderer.ChangeDebugMode(DebugMode::PBR_Enabled);
		wnd.SetTitle(L"DebugMode :: PBR ENABLED");
	}
	else if (wnd.kbd.KeyIsPressed('2'))
	{
		m_renderer.ChangeDebugMode(DebugMode::PBR_Disabled);
		wnd.SetTitle(L"DebugMode :: PBR DISABLED");
	}
	else if (wnd.kbd.KeyIsPressed('3'))
	{
		m_renderer.ChangeDebugMode(DebugMode::DepthTexture);
		wnd.SetTitle(L"DebugMode :: DEPTH");
	}
	else if (wnd.kbd.KeyIsPressed('4'))
	{
		m_renderer.ChangeDebugMode(DebugMode::Albedo);
		wnd.SetTitle(L"DebugMode :: ALBEDO");
	}
	else if (wnd.kbd.KeyIsPressed('5'))
	{
		m_renderer.ChangeDebugMode(DebugMode::Normal);
		wnd.SetTitle(L"DebugMode :: NORMAL");
	}
	else if (wnd.kbd.KeyIsPressed('6'))
	{
		m_renderer.ChangeDebugMode(DebugMode::MR);
		wnd.SetTitle(L"DebugMode :: METALLIC_ROUGHNESS");
	}
	else if (wnd.kbd.KeyIsPressed('7'))
	{
		m_renderer.ChangeDebugMode(DebugMode::BRDF_LUT);
		wnd.SetTitle(L"DebugMode :: BRDF LUT");
	}
	else if (wnd.kbd.KeyIsPressed('8'))
	{
		m_renderer.ChangeDebugMode(DebugMode::IrradianceMap);
		wnd.SetTitle(L"DebugMode :: IRRADIANCE MAP");
	}
	else if (wnd.kbd.KeyIsPressed('9'))
	{
		m_renderer.ChangeDebugMode(DebugMode::PreFilteredEnvrionmentMap);
		wnd.SetTitle(L"DebugMode :: PREFILTERED ENVIRONMENT MAP");
	}
}
void  Application::HandleCameraMovement(float deltaTime)
{
	if (!wnd.CursorEnabled())
	{
		if (wnd.kbd.KeyIsPressed('W'))
		{
			m_scene.cam.Translate({ 0.0f,0.0f,deltaTime });
		}
		if (wnd.kbd.KeyIsPressed('A'))
		{
			m_scene.cam.Translate({ -deltaTime,0.0f,0.0f });
		}
		if (wnd.kbd.KeyIsPressed('S'))
		{
			m_scene.cam.Translate({ 0.0f,0.0f,-deltaTime });
		}
		if (wnd.kbd.KeyIsPressed('D'))
		{
			m_scene.cam.Translate({ deltaTime,0.0f,0.0f });
		}
		if (wnd.kbd.KeyIsPressed('R'))
		{
			m_scene.cam.Translate({ 0.0f,deltaTime,0.0f });
		}
		if (wnd.kbd.KeyIsPressed('F'))
		{
			m_scene.cam.Translate({ 0.0f,-deltaTime,0.0f });
		}
	}

	while (const auto delta = wnd.mouse.ReadRawDelta())
	{
		if (!wnd.CursorEnabled())
		{
			m_scene.cam.Rotate((float)delta->x, (float)delta->y);
		}
	}
}