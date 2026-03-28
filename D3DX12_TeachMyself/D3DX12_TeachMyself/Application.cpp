#include "Application.h"
#include "AssetLoader.h"
#include <chrono>

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

		Update();
		Render();
	}
}

void Application::Init()
{
	CoInitializeEx(nullptr, COINIT_MULTITHREADED);

	m_device = std::make_unique<GraphicsDevice_DX12>();
	m_device->Initialize(wnd.GetHWND(), 1920, 1080);

	AssetLoader loader;
	auto foxMesh = loader.LoadGLTF("../Model/Fox/Fox.glb");
	auto foxTex = loader.LoadTexture(L"../Model/Fox/Texture.png");

	BufferDesc vbDesc = { static_cast<uint32_t>(foxMesh.vertices.size() * sizeof(Mesh::Vertex)), sizeof(Mesh::Vertex), BufferUsage::Vertex, MemoryAccess::CpuWrite };
	auto vb = m_device->CreateBuffer(vbDesc, foxMesh.vertices.data());

	BufferDesc ibDesc = { static_cast<uint32_t>(foxMesh.indices.size() * sizeof(uint32_t)), sizeof(uint32_t), BufferUsage::Index, MemoryAccess::CpuWrite };
	auto ib = m_device->CreateBuffer(ibDesc, foxMesh.indices.data());

	BufferDesc cbDesc = { sizeof(CubeConstantBuffer), 0, BufferUsage::Constant, MemoryAccess::GpuOnly };
	auto cb = m_device->CreateBuffer(cbDesc);

	TextureDesc foxTextureDesc = { foxTex.width, foxTex.height, Format::R8G8B8A8_UNORM, TextureUsage::ShaderResource };
	auto texture = m_device->CreateTexture(foxTextureDesc, foxTex.image.GetPixels());

	Scene::RenderObject fox;
	fox.vertexBuffer = vb;
	fox.indexBuffer = ib;
	fox.texture = texture;
	fox.indexCount = static_cast<uint32_t>(foxMesh.indices.size());
	XMStoreFloat4x4(&fox.world, XMMatrixIdentity());

	m_scene.renderObjects.push_back(fox);

	m_renderer.Init(m_device.get());
}

void Application::Update()
{
	static auto lastTime = std::chrono::high_resolution_clock::now();
	auto now = std::chrono::high_resolution_clock::now();
	float deltaTime = std::chrono::duration<float>(now - lastTime).count();
	lastTime = now;

	static float angle = 0.0f;
	angle += 0.3f * deltaTime;

	XMMATRIX world = XMMatrixRotationY(angle);
	XMMATRIX view = XMMatrixLookAtLH(
		XMVectorSet(0, 50, -200, 1),
		XMVectorSet(0, 20, 0, 1),
		XMVectorSet(0, 1, 0, 0));
	XMMATRIX proj = XMMatrixPerspectiveFovLH(
		XM_PIDIV4,
		static_cast<float>(1920) / static_cast<float>(1080),
		0.1f, 100.0f);

	XMMATRIX wvp = world * view * proj;
	XMStoreFloat4x4(&m_scene.renderObjects[0].world, XMMatrixTranspose(wvp));
}

void Application::Render()
{
	m_renderer.Render(m_device.get(), m_scene);
}