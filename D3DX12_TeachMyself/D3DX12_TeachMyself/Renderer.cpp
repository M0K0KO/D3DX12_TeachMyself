#include "Renderer.h"
#include "RenderGraph.h"
#include "ShaderCompiler.h"
#include "CommandContext.h"
#include <chrono>

void Renderer::Init(GraphicsDevice* device)
{
	m_width = device->GetWidth();
	m_height = device->GetHeight();

	// Depth Pre Pass
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
	depthPassRSDesc.rootParamDescs.push_back({ RootParamType::RootCBV, RangeType::CBV, 0, 1, ShaderVisibility::Vertex });
	depthPassRSDesc.rootParamDescs.push_back({ RootParamType::RootCBV, RangeType::CBV, 1, 1, ShaderVisibility::Vertex });

	PipelineDesc depthPassPSDesc = {
		depthPassRSDesc,
		vsBytecode, {}, vertexAttributes, 
		{ Format::UNKNOWN }, Format::D32_FLOAT, true, true, ComparisonFunc::Less };

	m_depthPrePassPipeline = device->CreatePipeline(depthPassPSDesc);

	TextureDesc depthTextureDesc = { m_width, m_height, Format::D32_FLOAT, TextureUsage::DepthStencil };
	m_depthTexture = device->CreateTexture(depthTextureDesc, nullptr);
	// Depth Pre Pass


	// GBuffer Pass
	TextureDesc albedoTextureDesc = { m_width, m_height, Format::R8G8B8A8_UNORM, TextureUsage::RenderTarget };
	m_gbufferAlbedo = device->CreateTexture(albedoTextureDesc, nullptr);
	TextureDesc normalTextureDesc = { m_width, m_height, Format::R16G16B16A16_SNORM, TextureUsage::RenderTarget };
	m_gbufferNormal = device->CreateTexture(normalTextureDesc, nullptr);
	TextureDesc mrTextureDesc = { m_width, m_height, Format::R8G8B8A8_UNORM, TextureUsage::RenderTarget };
	m_gbufferMR = device->CreateTexture(mrTextureDesc, nullptr);

	RootSignatureDesc gBufferPassRSDesc = {};
	gBufferPassRSDesc.rootParamDescs.push_back({ RootParamType::RootCBV, RangeType::CBV, 0, 1, ShaderVisibility::Vertex });
	gBufferPassRSDesc.rootParamDescs.push_back({ RootParamType::RootCBV, RangeType::CBV, 1, 1, ShaderVisibility::Vertex });
	gBufferPassRSDesc.rootParamDescs.push_back({ RootParamType::DescriptorTable, RangeType::SRV, 0, 1, ShaderVisibility::Pixel });
	gBufferPassRSDesc.rootParamDescs.push_back({ RootParamType::DescriptorTable, RangeType::SRV, 1, 1, ShaderVisibility::Pixel });
	gBufferPassRSDesc.rootParamDescs.push_back({ RootParamType::DescriptorTable, RangeType::SRV, 2, 1, ShaderVisibility::Pixel });

	auto gBufferVS = ShaderCompiler::CompileFromFile(
		L"shaders_GBuffer_VS.hlsl",
		"main",
		"vs_5_0"
	);

	auto gBufferPS = ShaderCompiler::CompileFromFile(
		L"shaders_GBuffer_PS.hlsl",
		"main",
		"ps_5_0"
	);

	PipelineDesc gBufferPassPSDesc = { 
		gBufferPassRSDesc,
		gBufferVS, gBufferPS, vertexAttributes,
		{ Format::R8G8B8A8_UNORM, Format::R16G16B16A16_SNORM, Format::R8G8B8A8_UNORM }, 
		Format::D32_FLOAT,
		true, false, ComparisonFunc::LessEqual };
	m_gBufferPassPipeline = device->CreatePipeline(gBufferPassPSDesc);
	// GBuffer Pass


	RootSignatureDesc lightingPassRSDesc = {};
	lightingPassRSDesc.rootParamDescs.push_back({ RootParamType::RootCBV, RangeType::CBV, 0, 1, ShaderVisibility::Pixel });
	lightingPassRSDesc.rootParamDescs.push_back({ RootParamType::DescriptorTable, RangeType::SRV, 0, 1, ShaderVisibility::Pixel });
	lightingPassRSDesc.rootParamDescs.push_back({ RootParamType::DescriptorTable, RangeType::SRV, 1, 1, ShaderVisibility::Pixel });
	lightingPassRSDesc.rootParamDescs.push_back({ RootParamType::DescriptorTable, RangeType::SRV, 2, 1, ShaderVisibility::Pixel });
	lightingPassRSDesc.rootParamDescs.push_back({ RootParamType::DescriptorTable, RangeType::SRV, 3, 1, ShaderVisibility::Pixel });

	auto lightingPassVS = ShaderCompiler::CompileFromFile(
		L"shaders_Lighting_VS.hlsl",
		"main",
		"vs_5_0"
	);

	auto lightingPassPS = ShaderCompiler::CompileFromFile(
		L"shaders_Lighting_PS.hlsl",
		"main",
		"ps_5_0"
	);

	PipelineDesc lightingPassPSDesc = {
		lightingPassRSDesc,
		lightingPassVS, lightingPassPS, vertexAttributes,
		{ Format::R8G8B8A8_UNORM },
		Format::UNKNOWN,
		false, false, ComparisonFunc::Equal };
	m_lightingPassPipeline = device->CreatePipeline(lightingPassPSDesc);


	// Debug
	auto debugVSBytecode = ShaderCompiler::CompileFromFile(
		L"debugshaders_VS.hlsl",
		"main",
		"vs_5_0"
	);

	auto debugPSBytecode = ShaderCompiler::CompileFromFile(
		L"debugshaders_PS.hlsl",
		"main",
		"ps_5_0"
	);

	auto depthDebugPSBytecode = ShaderCompiler::CompileFromFile(
		L"debugshaders_Depth_PS.hlsl",
		"main",
		"ps_5_0"
	);

	RootSignatureDesc debugPassRSDesc = {};
	debugPassRSDesc.rootParamDescs.push_back({ RootParamType::DescriptorTable, RangeType::SRV, 0, 1, ShaderVisibility::Pixel });

	PipelineDesc debugPassDesc = {
		debugPassRSDesc,
		debugVSBytecode, debugPSBytecode, {},
		{ Format::R8G8B8A8_UNORM }, 
		Format::UNKNOWN, 
		false, false, ComparisonFunc::Equal };

	m_debugPipeline = device->CreatePipeline(debugPassDesc);

	PipelineDesc depthDebugPassDesc = {
		debugPassRSDesc,
		debugVSBytecode, depthDebugPSBytecode, {},
		{ Format::R8G8B8A8_UNORM },
		Format::UNKNOWN,
		false, false, ComparisonFunc::Equal };

	m_depthDebugPipeline = device->CreatePipeline(depthDebugPassDesc);

	debugMode = DebugMode::None;
	// Debug
}

void Renderer::Render(GraphicsDevice* device, const Scene& scene)
{
	CommandContext& ctx = device->BeginFrame();

	RenderGraph graph(device);

	RGResourceDesc backBufferDesc = { m_width, m_height, Format::R8G8B8A8_UNORM, TextureUsage::RenderTarget};
	auto backBuffer = graph.ImportTexture(device->GetCurrentBackBuffer(), backBufferDesc, RGResourceState::Present);

	RGResourceDesc gbufferAlbedoDesc = { m_width, m_height, Format::R8G8B8A8_UNORM, TextureUsage::RenderTarget };
	auto gbufferAlbedo = graph.ImportTexture(m_gbufferAlbedo, gbufferAlbedoDesc, RGResourceState::RenderTarget);

	RGResourceDesc gbufferNormalDesc = { m_width, m_height, Format::R16G16B16A16_SNORM, TextureUsage::RenderTarget };
	auto gbufferNormal = graph.ImportTexture(m_gbufferNormal, gbufferNormalDesc, RGResourceState::RenderTarget);

	RGResourceDesc gbufferMRDesc = { m_width, m_height, Format::R8G8B8A8_UNORM, TextureUsage::RenderTarget };
	auto gbufferMR = graph.ImportTexture(m_gbufferMR, gbufferMRDesc, RGResourceState::RenderTarget);

	RGResourceDesc depthTextrueDesc = { m_width, m_height, Format::R32_TYPELESS, TextureUsage::DepthStencil };
	auto depthTexture = graph.ImportTexture(m_depthTexture, depthTextrueDesc, RGResourceState::DepthWrite);

	PerFrameData perFrame;
	XMMATRIX vp = scene.cam.GetViewMatrix() * scene.cam.GetProjectionMatrix();
	DirectX::XMStoreFloat4x4(&perFrame.ViewProj, XMMatrixTranspose(vp));
	perFrame.CameraPos = scene.cam.GetPos();

	CBHandle perFrameCBHandle;

	graph.AddPass(
		"DepthPrePass",
		[&](RGBuilder& builder) {
			builder.Write(depthTexture, RGResourceState::DepthWrite);
		},
		[&](CommandContext& passCtx) {
			passCtx.ClearDepthStencil(m_depthTexture, 1.0f);
			passCtx.SetRenderTarget(0, {}, m_depthTexture);
			passCtx.SetPipeline(m_depthPrePassPipeline);

			perFrameCBHandle = passCtx.UpdateConstantBuffer(0, &perFrame, sizeof(perFrame));
			passCtx.BindConstantBuffer(0, perFrameCBHandle);

			for (const auto& obj : scene.renderObjects)
			{
				auto perObjectCBHandle = passCtx.UpdateConstantBuffer(1, &obj.world, sizeof(obj.world));
				passCtx.BindConstantBuffer(1, perObjectCBHandle);
				passCtx.SetVertexBuffer(obj.vertexBuffer);
				passCtx.SetIndexBuffer(obj.indexBuffer);
				passCtx.DrawIndexed(obj.indexCount, obj.indexOffset, 0);
			}
		}
	);


	graph.AddPass(
		"GBufferPass",
		[&](RGBuilder& builder) {
			builder.Read(depthTexture, RGResourceState::DepthRead);
			builder.Write(gbufferAlbedo, RGResourceState::RenderTarget);
			builder.Write(gbufferNormal, RGResourceState::RenderTarget);
			builder.Write(gbufferMR, RGResourceState::RenderTarget);
		},
		[&](CommandContext& passCtx) {
			TextureHandle renderTargets[] = { m_gbufferAlbedo, m_gbufferNormal, m_gbufferMR };
			passCtx.ClearRenderTargets(3, renderTargets, clearColor);
			passCtx.SetRenderTarget(3, renderTargets, m_depthTexture);
			passCtx.SetPipeline(m_gBufferPassPipeline);

			passCtx.BindConstantBuffer(0, perFrameCBHandle);

			for (const auto& obj : scene.renderObjects)
			{
				auto perObjectCBHandle = passCtx.UpdateConstantBuffer(1, &obj.world, sizeof(obj.world));
				passCtx.BindConstantBuffer(1, perObjectCBHandle);
				passCtx.SetVertexBuffer(obj.vertexBuffer);
				passCtx.SetIndexBuffer(obj.indexBuffer);
				passCtx.BindTexture(obj.material.baseColor, 2);
				passCtx.BindTexture(obj.material.normal, 3);
				passCtx.BindTexture(obj.material.metallicRoughness, 4);
				passCtx.DrawIndexed(obj.indexCount, obj.indexOffset, 0);
			}
		}
	);

	if (debugMode == DebugMode::None)
	{
		graph.AddPass(
			"LightingPass",
			[&](RGBuilder& builder) {
				builder.Read(depthTexture, RGResourceState::ShaderResource);
				builder.Read(gbufferAlbedo, RGResourceState::ShaderResource);
				builder.Read(gbufferNormal, RGResourceState::ShaderResource);
				builder.Read(gbufferMR, RGResourceState::ShaderResource);
				builder.Write(backBuffer, RGResourceState::RenderTarget);
			},
			[&](CommandContext& passCtx) {
				passCtx.ClearRenderTarget(device->GetCurrentBackBuffer(), clearColor);
				passCtx.SetRenderTarget(1, device->GetCurrentBackBufferPtr(), {});
				passCtx.SetPipeline(m_lightingPassPipeline);

				static auto lastTime = std::chrono::high_resolution_clock::now();
				auto now = std::chrono::high_resolution_clock::now();
				float deltaTime = std::chrono::duration<float>(now - lastTime).count();
				lastTime = now;

				LightingData lightingData;
				XMMATRIX inverseVP = XMMatrixInverse(nullptr, vp);
				XMStoreFloat4x4(&lightingData.inverseVP, XMMatrixTranspose(inverseVP));
				lightingData.cameraPos = scene.cam.GetPos();


				static float time = 0.0f;
				time += deltaTime * 0.5f;

				float x = sinf(time * 2) * 0.8f;

				lightingData.direction = { x, -0.82f, 0.4f };

				float len = sqrtf(
					lightingData.direction.x * lightingData.direction.x +
					lightingData.direction.y * lightingData.direction.y +
					lightingData.direction.z * lightingData.direction.z);

				lightingData.direction.x /= len;
				lightingData.direction.y /= len;
				lightingData.direction.z /= len;



				lightingData.color = { 1.0f, 1.0f, 1.0f };
				lightingData.ambient = { 0.07f, 0.07f, 0.07f };
				lightingData.intensity = 1.1f;

				auto ligthDataCBHandle = passCtx.UpdateConstantBuffer(0, &lightingData, sizeof(lightingData));
				passCtx.BindConstantBuffer(0, ligthDataCBHandle);

				passCtx.BindTexture(m_depthTexture, 1);
				passCtx.BindTexture(m_gbufferAlbedo, 2);
				passCtx.BindTexture(m_gbufferNormal, 3);
				passCtx.BindTexture(m_gbufferMR, 4);

				passCtx.Draw(3, 0);
			}
		);
	}
	else
	{
		graph.AddPass(
			"DebugPass",
			[&](RGBuilder& builder) {
				builder.Read(depthTexture, RGResourceState::ShaderResource);
				builder.Read(gbufferAlbedo, RGResourceState::ShaderResource);
				builder.Read(gbufferNormal, RGResourceState::ShaderResource);
				builder.Read(gbufferMR, RGResourceState::ShaderResource);

				builder.Write(backBuffer, RGResourceState::RenderTarget);
			},
			[&](CommandContext& passCtx) {
				passCtx.ClearRenderTarget(device->GetCurrentBackBuffer(), clearColor);
				passCtx.SetRenderTarget(1, device->GetCurrentBackBufferPtr(), {});

				if (debugMode == DebugMode::DepthTexture)
				{
					passCtx.SetPipeline(m_depthDebugPipeline);
					passCtx.BindTexture(m_depthTexture, 0);
				}
				else if (debugMode == DebugMode::Albedo)
				{
					passCtx.SetPipeline(m_debugPipeline);
					passCtx.BindTexture(m_gbufferAlbedo, 0);
				}
				else if (debugMode == DebugMode::Normal)
				{
					passCtx.SetPipeline(m_debugPipeline);
					passCtx.BindTexture(m_gbufferNormal, 0);
				}
				else if (debugMode == DebugMode::MR)
				{
					passCtx.SetPipeline(m_debugPipeline);
					passCtx.BindTexture(m_gbufferMR, 0);
				}

				passCtx.Draw(3, 0);
			}
		);
	}
	

	graph.Compile();
	graph.Execute(ctx);
	graph.Clear();

	device->EndFrame();
}