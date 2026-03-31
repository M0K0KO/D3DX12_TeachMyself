#include "Renderer.h"
#include "RenderGraph.h"
#include "ShaderCompiler.h"
#include "CommandContext.h"

void Renderer::Init(GraphicsDevice* device)
{
	debugMode = DebugMode::DepthTexture;

	auto vsBytecode = ShaderCompiler::CompileFromFile(
		L"shaders_VSMain.hlsl",
		"main",
		"vs_5_0"
	);

	auto psBytecode = ShaderCompiler::CompileFromFile(
		L"shaders_PSMain.hlsl",
		"main",
		"ps_5_0"
	);

	std::vector<VertexAttribute> vertexAttributes;
	vertexAttributes.push_back({ Semantic::POSITION, Format::R32G32B32_FLOAT, 0 });
	vertexAttributes.push_back({ Semantic::NORMAL,   Format::R32G32B32_FLOAT, 0 });
	vertexAttributes.push_back({ Semantic::TANGENT, Format::R32G32B32A32_FLOAT, 0 });
	vertexAttributes.push_back({ Semantic::TEXCOORD, Format::R32G32_FLOAT, 0 });


	RootSignatureDesc depthPassRSDesc = {};
	depthPassRSDesc.rootParamDescs.push_back({DescriptorRangeType::CBV, 0, 1, ShaderVisibility::Vertex});
	depthPassRSDesc.rootParamDescs.push_back({DescriptorRangeType::CBV, 1, 1, ShaderVisibility::Vertex});

	PipelineDesc depthPassDesc = {
		depthPassRSDesc,
		vsBytecode, {}, vertexAttributes, 
		{ Format::UNKNOWN }, Format::D32_FLOAT, true, true, ComparisonFunc::Less };

	m_depthPrePassPipeline = device->CreatePipeline(depthPassDesc);



	TextureDesc depthDesc = { 1920, 1080, Format::D32_FLOAT, TextureUsage::DepthStencil };
	m_depthTexture = device->CreateTexture(depthDesc, nullptr);


	RootSignatureDesc forwardPassRSDesc = {};
	forwardPassRSDesc.rootParamDescs.push_back({ DescriptorRangeType::CBV, 0, 1, ShaderVisibility::Vertex });
	forwardPassRSDesc.rootParamDescs.push_back({ DescriptorRangeType::CBV, 1, 1, ShaderVisibility::Vertex });
	forwardPassRSDesc.rootParamDescs.push_back({ DescriptorRangeType::SRV, 0, 1, ShaderVisibility::Pixel });
	forwardPassRSDesc.rootParamDescs.push_back({ DescriptorRangeType::SRV, 1, 1, ShaderVisibility::Pixel });
	forwardPassRSDesc.rootParamDescs.push_back({ DescriptorRangeType::SRV, 2, 1, ShaderVisibility::Pixel });

	PipelineDesc forwardDesc = { 
		forwardPassRSDesc,
		vsBytecode, psBytecode, vertexAttributes, 
		{ Format::R8G8B8A8_UNORM }, Format::D32_FLOAT, true, false, ComparisonFunc::LessEqual };

	m_forwardPipeline = device->CreatePipeline(forwardDesc);


	// Debug
	auto debugVSBytecode = ShaderCompiler::CompileFromFile(
		L"shaders_VSDebug.hlsl",
		"main",
		"vs_5_0"
	);

	auto debugPSBytecode = ShaderCompiler::CompileFromFile(
		L"shaders_PSDebug.hlsl",
		"main",
		"ps_5_0"
	);

	RootSignatureDesc debugPassRSDesc = {};
	debugPassRSDesc.rootParamDescs.push_back({ DescriptorRangeType::SRV, 0, 1, ShaderVisibility::Pixel });

	PipelineDesc debugPassDesc = {
		debugPassRSDesc,
		debugVSBytecode, debugPSBytecode, {},
		{ Format::R8G8B8A8_UNORM }, Format::UNKNOWN, false, false, ComparisonFunc::Equal };

	m_debugPipeline = device->CreatePipeline(debugPassDesc);

	debugMode = DebugMode::DepthTexture;
	// Debug


	BufferDesc perFrameCBDesc = { sizeof(PerFrameData), 0, BufferUsage::Constant, MemoryAccess::GpuOnly };
	m_perFrameCB = device->CreateBuffer(perFrameCBDesc);

	BufferDesc perObjectCBDesc = { sizeof(PerObjectData), 0, BufferUsage::Constant, MemoryAccess::GpuOnly };
	m_perObjectCB = device->CreateBuffer(perObjectCBDesc);
}

void Renderer::Render(GraphicsDevice* device, const Scene& scene)
{
	CommandContext& ctx = device->BeginFrame();

	RenderGraph graph(device);

	RGResourceDesc backBufferDesc = { 1920, 1080, Format::R8G8B8A8_UNORM, TextureUsage::RenderTarget };
	auto backBuffer = graph.ImportTexture(device->GetCurrentBackBuffer(), backBufferDesc, RGResourceState::Present);

	RGResourceDesc depthTextrueDesc = { 1920, 1080, Format::R32_TYPELESS, TextureUsage::DepthStencil };
	auto depthTexture = graph.ImportTexture(m_depthTexture, depthTextrueDesc, RGResourceState::DepthWrite);


	graph.AddPass(
		"DepthPrePass",
		[&](RGBuilder& builder)
		{
			builder.Write(depthTexture, RGResourceState::DepthWrite);
		},
		[&](CommandContext& passCtx) {
			passCtx.ClearDepthStencil(m_depthTexture, 1.0f);
			passCtx.SetRenderTarget({}, m_depthTexture);
			passCtx.SetPipeline(m_depthPrePassPipeline);

			PerFrameData perFrame;
			XMMATRIX vp = scene.cam.GetViewMatrix() * scene.cam.GetProjectionMatrix();
			XMStoreFloat4x4(&perFrame.ViewProj, XMMatrixTranspose(vp));
			perFrame.CameraPos = scene.cam.GetPos();

			device->UpdateBuffer(m_perFrameCB, &perFrame, sizeof(perFrame));
			passCtx.BindConstantBuffer(m_perFrameCB, 0);

			for (const auto& obj : scene.renderObjects)
			{
				device->UpdateBuffer(m_perObjectCB, &obj.world, sizeof(obj.world));
				passCtx.BindConstantBuffer(m_perObjectCB, 1);
				passCtx.SetVertexBuffer(obj.vertexBuffer);
				passCtx.SetIndexBuffer(obj.indexBuffer);
				passCtx.DrawIndexed(obj.indexCount, obj.indexOffset, 0);
			}
		}
	);


	graph.AddPass(
		"ForwardPass",
		[&](RGBuilder& builder) 
		{
			builder.Read(depthTexture, RGResourceState::DepthRead);
			builder.Write(backBuffer, RGResourceState::RenderTarget);
		}, 
		[&](CommandContext& passCtx) 
		{ 
			passCtx.ClearRenderTarget(device->GetCurrentBackBuffer(), clearColor);
			passCtx.SetRenderTarget(device->GetCurrentBackBuffer(), m_depthTexture);
			passCtx.SetPipeline(m_forwardPipeline);

			PerFrameData perFrame;
			XMMATRIX vp = scene.cam.GetViewMatrix() * scene.cam.GetProjectionMatrix();
			XMStoreFloat4x4(&perFrame.ViewProj, XMMatrixTranspose(vp));
			perFrame.CameraPos = scene.cam.GetPos();

			device->UpdateBuffer(m_perFrameCB, &perFrame, sizeof(perFrame));
			passCtx.BindConstantBuffer(m_perFrameCB, 0);

			for (const auto& obj : scene.renderObjects)
			{
				device->UpdateBuffer(m_perObjectCB, &obj.world, sizeof(obj.world));
				passCtx.BindConstantBuffer(m_perObjectCB, 1);
				passCtx.SetVertexBuffer(obj.vertexBuffer);
				passCtx.SetIndexBuffer(obj.indexBuffer);
				passCtx.BindTexture(obj.material.baseColor, 2);
				passCtx.BindTexture(obj.material.normal, 3);
				passCtx.BindTexture(obj.material.metallicRoughness, 4);
				passCtx.DrawIndexed(obj.indexCount, obj.indexOffset, 0);
			}
		}
	);

	/*
	if (debugMode == DebugMode::DepthTexture)
	{
		graph.AddPass(
			"DebugPass",
			[&](RGBuilder& builder) 
			{
				builder.Read(depthTexture, RGResourceState::ShaderResource);
				builder.Write(backBuffer, RGResourceState::RenderTarget);
			},
			[&](CommandContext& passCtx) 
			{
				passCtx.ClearRenderTarget(device->GetCurrentBackBuffer(), clearColor);
				passCtx.SetRenderTarget(device->GetCurrentBackBuffer(), {});
				passCtx.SetPipeline(m_debugPipeline);
				passCtx.BindTexture(m_depthTexture, 0);
				passCtx.Draw(3, 0);
			}
		);
	}
	*/

	graph.Compile();
	graph.Execute(ctx);
	graph.Clear();

	device->EndFrame();
}