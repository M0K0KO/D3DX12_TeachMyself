#include "stdafx.h"
#include "SpinningCube.h"
#include "GraphicsDevice_DX12.h"
#include <chrono>

SpinningCube::SpinningCube(UINT width, UINT height, std::wstring name) :
	DXSample(width, height, name)
{
}

void SpinningCube::OnInit()
{
	m_device = new GraphicsDevice_DX12();
	m_device->Initialize(Win32Application::GetHwnd(), m_width, m_height);

	Vertex cubeVertices[] =
	{
		// Front (z = 1)
		{ {-1.0f,  1.0f,  1.0f}, {0.0f, 0.0f} },
		{ { 1.0f,  1.0f,  1.0f}, {1.0f, 0.0f} },
		{ { 1.0f, -1.0f,  1.0f}, {1.0f, 1.0f} },
		{ {-1.0f, -1.0f,  1.0f}, {0.0f, 1.0f} },

		// Back (z = -1)
		{ { 1.0f,  1.0f, -1.0f}, {0.0f, 0.0f} },
		{ {-1.0f,  1.0f, -1.0f}, {1.0f, 0.0f} },
		{ {-1.0f, -1.0f, -1.0f}, {1.0f, 1.0f} },
		{ { 1.0f, -1.0f, -1.0f}, {0.0f, 1.0f} },

		// Top (y = 1)
		{ {-1.0f,  1.0f, -1.0f}, {0.0f, 0.0f} },
		{ { 1.0f,  1.0f, -1.0f}, {1.0f, 0.0f} },
		{ { 1.0f,  1.0f,  1.0f}, {1.0f, 1.0f} },
		{ {-1.0f,  1.0f,  1.0f}, {0.0f, 1.0f} },

		// Bottom (y = -1)
		{ {-1.0f, -1.0f,  1.0f}, {0.0f, 0.0f} },
		{ { 1.0f, -1.0f,  1.0f}, {1.0f, 0.0f} },
		{ { 1.0f, -1.0f, -1.0f}, {1.0f, 1.0f} },
		{ {-1.0f, -1.0f, -1.0f}, {0.0f, 1.0f} },

		// Right (x = 1)
		{ { 1.0f,  1.0f,  1.0f}, {0.0f, 0.0f} },
		{ { 1.0f,  1.0f, -1.0f}, {1.0f, 0.0f} },
		{ { 1.0f, -1.0f, -1.0f}, {1.0f, 1.0f} },
		{ { 1.0f, -1.0f,  1.0f}, {0.0f, 1.0f} },

		// Left (x = -1)
		{ {-1.0f,  1.0f, -1.0f}, {0.0f, 0.0f} },
		{ {-1.0f,  1.0f,  1.0f}, {1.0f, 0.0f} },
		{ {-1.0f, -1.0f,  1.0f}, {1.0f, 1.0f} },
		{ {-1.0f, -1.0f, -1.0f}, {0.0f, 1.0f} },
	};

	unsigned int indices[] =
	{
		0,  2,  1,   0,  3,  2,   // Front
		4,  6,  5,   4,  7,  6,   // Back
		8,  10, 9,   8,  11, 10,  // Top
		12, 14, 13,  12, 15, 14,  // Bottom
		16, 18, 17,  16, 19, 18,  // Right
		20, 22, 21,  20, 23, 22,  // Left
	};

	BufferDesc vbDesc = { sizeof(cubeVertices), sizeof(Vertex), BufferUsage::Vertex, MemoryAccess::CpuWrite };
	m_vertexBuffer = m_device->CreateBuffer(vbDesc, cubeVertices);

	BufferDesc ibDesc = { sizeof(indices), sizeof(unsigned int), BufferUsage::Index, MemoryAccess::CpuWrite };
	m_indexBuffer = m_device->CreateBuffer(ibDesc, indices);

	BufferDesc cbDesc = { sizeof(CubeConstantBuffer), 0, BufferUsage::Constant, MemoryAccess::GpuOnly };
	m_constantBuffer = m_device->CreateBuffer(cbDesc);

	TextureDesc cubeTextureDesc = { 256, 256, Format::R8G8B8A8_UNORM, TextureUsage::ShaderResource };
	m_texture = m_device->CreateTexture(cubeTextureDesc, GenerateTextureData().data());


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
		XMVectorSet(0, 2, -5, 1),
		XMVectorSet(0, 0, 0, 1),
		XMVectorSet(0, 1, 0, 0));
	XMMATRIX proj = XMMatrixPerspectiveFovLH(
		XM_PIDIV4,
		m_aspectRatio,
		0.1f, 100.0f);

	XMMATRIX wvp = world * view * proj;

	XMStoreFloat4x4(&m_constantBufferData.worldViewProj, XMMatrixTranspose(wvp));

	CommandContext& ctx = m_device->BeginFrame();
	m_device->UpdateBuffer(m_constantBuffer, &m_constantBufferData, sizeof(m_constantBufferData));

	ctx.SetPipeline(m_pipeline);
	ctx.SetVertexBuffer(m_vertexBuffer);
	ctx.SetIndexBuffer(m_indexBuffer);
	ctx.BindConstantBuffer(m_constantBuffer, 0);
	ctx.BindTexture(m_texture, 1);
	ctx.DrawIndexed(36, 0, 0);
	
	m_device->EndFrame();
}

void SpinningCube::OnDestroy()
{
	m_device->Shutdown();

	delete(m_device);
}

std::vector<UINT8> SpinningCube::GenerateTextureData()
{
	const UINT rowPitch = TextureWidth * TexturePixelSize;
	const UINT cellPitch = rowPitch >> 3;
	const UINT cellHeight = TextureWidth >> 3;
	const UINT textureSize = rowPitch * TextureHeight;

	std::vector<UINT8> data(textureSize);
	UINT8* pData = &data[0];

	for (UINT n = 0; n < textureSize; n += TexturePixelSize)
	{
		UINT x = n % rowPitch;
		UINT y = n / rowPitch;
		UINT i = x / cellPitch;
		UINT j = y / cellHeight;

		if (i % 2 == j % 2)
		{
			pData[n] = 0x00;        // R
			pData[n + 1] = 0x00;    // G
			pData[n + 2] = 0x00;    // B
			pData[n + 3] = 0xff;    // A
		}
		else
		{
			pData[n] = 0xff;        // R
			pData[n + 1] = 0xff;    // G
			pData[n + 2] = 0xff;    // B
			pData[n + 3] = 0xff;    // A
		}
	}

	return data;
}

