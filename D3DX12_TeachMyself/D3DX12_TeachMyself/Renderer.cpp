#include "Renderer.h"
#include "RenderGraph.h"
#include "CommandContext.h"

void Renderer::Init(GraphicsDevice* device)
{
	// TO DO : make an abstraction layer for shader compilation
	Microsoft::WRL::ComPtr<ID3DBlob> vertexShader;
	Microsoft::WRL::ComPtr<ID3DBlob> pixelShader;
	Microsoft::WRL::ComPtr<ID3DBlob> error;

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
	// TO DO : make an abstraction layer for shader compilation


	ShaderBytecode vs = { vertexShader->GetBufferPointer(), vertexShader->GetBufferSize() };
	ShaderBytecode ps = { pixelShader->GetBufferPointer(), pixelShader->GetBufferSize() };

	std::vector<VertexAttribute> vertexAttributes;
	vertexAttributes.push_back({ Semantic::POSITION, Format::R32G32B32_FLOAT, 0 });
	vertexAttributes.push_back({ Semantic::NORMAL,   Format::R32G32B32_FLOAT, 0 });
	vertexAttributes.push_back({ Semantic::TEXCOORD, Format::R32G32_FLOAT, 0 });

	PipelineDesc pipelineDesc = { vs, ps, vertexAttributes, Format::R8G8B8A8_UNORM, false, false };
	m_forwardPipeline = device->CreatePipeline(pipelineDesc);


	BufferDesc cbDesc = { sizeof(XMFLOAT4X4), 0, BufferUsage::Constant, MemoryAccess::GpuOnly };
	m_constantBuffer = device->CreateBuffer(cbDesc);
}

void Renderer::Render(GraphicsDevice* device, const Scene& scene)
{
	CommandContext& ctx = device->BeginFrame();

	RenderGraph graph(device);

	RGResourceDesc backBufferDesc = { 1920, 1080, Format::R8G8B8A8_UNORM, TextureUsage::RenderTarget };
	auto backBuffer = graph.ImportTexture(TextureHandle{}, backBufferDesc);

	graph.AddPass(
		"ForwardPass",
		[&](RGBuilder& builder) 
		{
			builder.Write(backBuffer);
		}, 
		[&](CommandContext& passCtx) 
		{ 
			passCtx.SetPipeline(m_forwardPipeline);

			for (const auto& obj : scene.renderObjects)
			{
				device->UpdateBuffer(m_constantBuffer, &obj.world, sizeof(obj.world));

				passCtx.SetVertexBuffer(obj.vertexBuffer);
				passCtx.SetIndexBuffer(obj.indexBuffer);
				passCtx.BindConstantBuffer(m_constantBuffer, 0);
				passCtx.BindTexture(obj.texture, 1);
				passCtx.DrawIndexed(obj.indexCount, 0, 0);
			}
		}
	);

	graph.Compile();
	graph.Execute(ctx);
	graph.Clear();

	device->EndFrame();
}