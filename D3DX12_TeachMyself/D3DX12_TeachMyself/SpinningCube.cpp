#include "stdafx.h"
#include "SpinningCube.h"
#include "GraphicsDevice_DX12.h"
#include "RenderGraph.h"
#include <chrono>
#include "AssetLoader.h"

SpinningCube::SpinningCube(UINT width, UINT height, std::wstring name) :
	DXSample(width, height, name)
{
}

void SpinningCube::OnInit()
{
	CoInitializeEx(nullptr, COINIT_MULTITHREADED);

	m_device = new GraphicsDevice_DX12();
	m_device->Initialize(Win32Application::GetHwnd(), m_width, m_height);

	// load a model!
	AssetLoader loader = {};
	const std::string modelPath = "C:\\PORTFOLIO\\Graphics\\D3DX12_TeachMyself\\D3DX12_TeachMyself\\Model\\Fox\\Fox.glb";
	foxMesh = loader.LoadGLTF(modelPath);

	const std::wstring texturePath = L"C:\\PORTFOLIO\\Graphics\\D3DX12_TeachMyself\\D3DX12_TeachMyself\\Model\\Fox\\Texture.png";
	Mesh::TextureData tex = loader.LoadTexture(texturePath);
	auto* pixels = tex.image.GetPixels();

	BufferDesc vbDesc = { static_cast<uint32_t>(foxMesh.vertices.size() * sizeof(Mesh::Vertex)), sizeof(Mesh::Vertex), BufferUsage::Vertex, MemoryAccess::CpuWrite };
	m_vertexBuffer = m_device->CreateBuffer(vbDesc, foxMesh.vertices.data());

	BufferDesc ibDesc = { static_cast<uint32_t>(foxMesh.indices.size() * sizeof(uint32_t)), sizeof(uint32_t), BufferUsage::Index, MemoryAccess::CpuWrite };
	m_indexBuffer = m_device->CreateBuffer(ibDesc, foxMesh.indices.data());

	BufferDesc cbDesc = { sizeof(CubeConstantBuffer), 0, BufferUsage::Constant, MemoryAccess::GpuOnly };
	m_constantBuffer = m_device->CreateBuffer(cbDesc);

	TextureDesc foxTextureDesc = { tex.width, tex.height, Format::R8G8B8A8_UNORM, TextureUsage::ShaderResource };
	m_texture = m_device->CreateTexture(foxTextureDesc, pixels);


	ComPtr<ID3DBlob> vertexShader;
	ComPtr<ID3DBlob> pixelShader;
	ComPtr<ID3DBlob> error;

	// TO DO : make an abstraction layer for shader compilation
	D3DCompileFromFile(
		L"shaders_VSMain.hlsl", nullptr, nullptr,
		"main", "vs_5_0",
		D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION,
		0, &vertexShader, &error);
	D3DCompileFromFile(
		L"shaders_PSMain.hlsl", nullptr, nullptr,
		"main", "ps_5_0",
		D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION,
		0, &pixelShader, &error);
	ShaderBytecode vs = { vertexShader->GetBufferPointer(), vertexShader->GetBufferSize() };
	ShaderBytecode ps = { pixelShader->GetBufferPointer(), pixelShader->GetBufferSize() };
	
	std::vector<VertexAttribute> vertexAttributes;
	vertexAttributes.push_back({ Semantic::POSITION, Format::R32G32B32_FLOAT, 0 });
	vertexAttributes.push_back({ Semantic::NORMAL,   Format::R32G32B32_FLOAT, 0 });
	vertexAttributes.push_back({ Semantic::TEXCOORD, Format::R32G32_FLOAT, 0 });

	PipelineDesc pipelineDesc = { vs, ps, vertexAttributes, Format::R8G8B8A8_UNORM, false, false };
	m_pipeline = m_device->CreatePipeline(pipelineDesc);

}

void SpinningCube::OnUpdate()
{

}

void SpinningCube::OnRender()
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
		m_aspectRatio,
		0.1f, 100.0f);

	XMMATRIX wvp = world * view * proj;
	XMStoreFloat4x4(&m_constantBufferData.worldViewProj, XMMatrixTranspose(wvp));

	CommandContext& ctx = m_device->BeginFrame();
	m_device->UpdateBuffer(m_constantBuffer, &m_constantBufferData, sizeof(m_constantBufferData));

	RenderGraph graph(m_device);

	RGResourceDesc backBufferDesc = { m_width, m_height, Format::R8G8B8A8_UNORM, TextureUsage::RenderTarget };
	auto backBuffer = graph.ImportTexture(TextureHandle{}, backBufferDesc);

	graph.AddPass("SpinningCube", [&](RGBuilder& builder) {
		builder.Write(backBuffer);
	}, [this](CommandContext& passCtx) {
		passCtx.SetPipeline(m_pipeline);
		passCtx.SetVertexBuffer(m_vertexBuffer);
		passCtx.SetIndexBuffer(m_indexBuffer);
		passCtx.BindConstantBuffer(m_constantBuffer, 0);
		passCtx.BindTexture(m_texture, 1);
		passCtx.DrawIndexed(static_cast<uint32_t>(foxMesh.indices.size()), 0, 0);
	});

	graph.Compile();
	graph.Execute(ctx);
	graph.Clear();
	
	m_device->EndFrame();
}

void SpinningCube::OnDestroy()
{
	m_device->Shutdown();

	delete(m_device);
}

