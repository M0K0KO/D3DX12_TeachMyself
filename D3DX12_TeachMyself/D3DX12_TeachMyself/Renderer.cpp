#include "Renderer.h"
#include "RenderGraph.h"
#include "ShaderCompiler.h"
#include "CommandContext.h"
#include "GPUTimestampProfiler.h"
#include <chrono>
#include "AssetLoader.h"

void Renderer::Init(GraphicsDevice* device)
{
	m_width = device->GetWidth();
	m_height = device->GetHeight();

	ShaderCompiler::Reserve(50);

	// Depth Pre Pass
	m_depthVS = ShaderCompiler::CompileFromFile(
		L"shaders_Depth_VS.hlsl",
		"main",
		"vs_5_0"
	);

	std::vector<VertexAttribute> vertexAttributes;
	vertexAttributes.push_back({ Semantic::POSITION, Format::R32G32B32_FLOAT, 0 });
	vertexAttributes.push_back({ Semantic::NORMAL,   Format::R32G32B32_FLOAT, 0 });
	vertexAttributes.push_back({ Semantic::TANGENT, Format::R32G32B32A32_FLOAT, 0 });
	vertexAttributes.push_back({ Semantic::TEXCOORD, Format::R32G32_FLOAT, 0 });


	RootSignatureDesc depthPassRSDesc = {};
	depthPassRSDesc.rootParamDescs.push_back({ RootParamType::RootCBV, RangeType::CBV, 0, 1, ShaderVisibility::Vertex });
	depthPassRSDesc.rootParamDescs.push_back({ RootParamType::RootCBV, RangeType::CBV, 1, 1, ShaderVisibility::Vertex });

	m_depthPrePassPipelinDesc = {
		depthPassRSDesc,
		ShaderCompiler::GetBytecode(m_depthVS), {}, vertexAttributes,
		{ Format::UNKNOWN }, Format::D32_FLOAT, 
		true, true, ComparisonFunc::Less, 
		CullMode::Back };


	m_depthPrePassPipeline = device->CreatePipeline(m_depthPrePassPipelinDesc);

	TextureDesc depthTextureDesc = { m_width, m_height, Format::D32_FLOAT, TextureUsage::DepthStencil };
	m_depthTexture = device->CreateTexture(depthTextureDesc, nullptr);
	// Depth Pre Pass


	// GBuffer Pass
	TextureDesc albedoTextureDesc = { m_width, m_height, Format::R8G8B8A8_UNORM, TextureUsage::RenderTarget };
	m_gbufferAlbedo = device->CreateTexture(albedoTextureDesc, nullptr);
	TextureDesc normalTextureDesc = { m_width, m_height, Format::R16G16B16A16_FLOAT, TextureUsage::RenderTarget };
	m_gbufferNormal = device->CreateTexture(normalTextureDesc, nullptr);
	TextureDesc mrTextureDesc = { m_width, m_height, Format::R8G8B8A8_UNORM, TextureUsage::RenderTarget };
	m_gbufferMR = device->CreateTexture(mrTextureDesc, nullptr);

	RootSignatureDesc gBufferPassRSDesc = {};
	gBufferPassRSDesc.rootParamDescs.push_back({ RootParamType::RootCBV, RangeType::CBV, 0, 1, ShaderVisibility::Vertex });
	gBufferPassRSDesc.rootParamDescs.push_back({ RootParamType::RootCBV, RangeType::CBV, 1, 1, ShaderVisibility::Vertex });
	gBufferPassRSDesc.rootParamDescs.push_back({ RootParamType::DescriptorTable, RangeType::SRV, 0, 1, ShaderVisibility::Pixel });
	gBufferPassRSDesc.rootParamDescs.push_back({ RootParamType::DescriptorTable, RangeType::SRV, 1, 1, ShaderVisibility::Pixel });
	gBufferPassRSDesc.rootParamDescs.push_back({ RootParamType::DescriptorTable, RangeType::SRV, 2, 1, ShaderVisibility::Pixel });
	gBufferPassRSDesc.rootParamDescs.push_back({ RootParamType::RootConstants, RangeType::CBV, 2, 3, ShaderVisibility::Pixel });
	gBufferPassRSDesc.staticSamplers.push_back({ SamplerFilter::Anisotropic, SamplerAddressMode::Wrap, 0, ShaderVisibility::Pixel });



	m_gBufferVS = ShaderCompiler::CompileFromFile(
		L"shaders_GBuffer_VS.hlsl",
		"main",
		"vs_5_0"
	);

	m_gBufferOpaquePS = ShaderCompiler::CompileFromFile(
		L"shaders_GBufferOpaque_PS.hlsl",
		"main",
		"ps_5_0"
	);


	m_gBufferOpaquePassPipelineDesc = { 
		gBufferPassRSDesc,
		ShaderCompiler::GetBytecode(m_gBufferVS), ShaderCompiler::GetBytecode(m_gBufferOpaquePS), vertexAttributes,
		{ Format::R8G8B8A8_UNORM, Format::R16G16B16A16_FLOAT, Format::R8G8B8A8_UNORM },
		Format::D32_FLOAT,
		true, false, ComparisonFunc::Equal,
		CullMode::Back
	};

	m_gBufferOpaquePassPipeline = device->CreatePipeline(m_gBufferOpaquePassPipelineDesc);


	RootSignatureDesc gBufferAlphaPassRSDesc = {};
	gBufferAlphaPassRSDesc.rootParamDescs.push_back({ RootParamType::RootCBV, RangeType::CBV, 0, 1, ShaderVisibility::Vertex });
	gBufferAlphaPassRSDesc.rootParamDescs.push_back({ RootParamType::RootCBV, RangeType::CBV, 1, 1, ShaderVisibility::Vertex });
	gBufferAlphaPassRSDesc.rootParamDescs.push_back({ RootParamType::DescriptorTable, RangeType::SRV, 0, 1, ShaderVisibility::Pixel });
	gBufferAlphaPassRSDesc.rootParamDescs.push_back({ RootParamType::DescriptorTable, RangeType::SRV, 1, 1, ShaderVisibility::Pixel });
	gBufferAlphaPassRSDesc.rootParamDescs.push_back({ RootParamType::DescriptorTable, RangeType::SRV, 2, 1, ShaderVisibility::Pixel });
	gBufferAlphaPassRSDesc.rootParamDescs.push_back({ RootParamType::RootConstants, RangeType::CBV, 2, 3, ShaderVisibility::Pixel });
	gBufferAlphaPassRSDesc.staticSamplers.push_back({ SamplerFilter::Anisotropic, SamplerAddressMode::Wrap, 0, ShaderVisibility::Pixel });


	m_gBufferAlphaPS = ShaderCompiler::CompileFromFile(
		L"shaders_GBufferAlpha_PS.hlsl",
		"main",
		"ps_5_0"
	);

	m_gBufferAlphaPassPipelineDesc = {
		gBufferAlphaPassRSDesc,
		ShaderCompiler::GetBytecode(m_gBufferVS), ShaderCompiler::GetBytecode(m_gBufferAlphaPS), vertexAttributes,
		{ Format::R8G8B8A8_UNORM, Format::R16G16B16A16_FLOAT, Format::R8G8B8A8_UNORM },
		Format::D32_FLOAT,
		true, true, ComparisonFunc::LessEqual,
		CullMode::None
	};
	m_gBufferAlphaPassPipeline = device->CreatePipeline(m_gBufferAlphaPassPipelineDesc);
	// GBuffer Pass

	// Lighting Pass
	RootSignatureDesc lightingPassRSDesc = {};
	lightingPassRSDesc.rootParamDescs.push_back({ RootParamType::RootCBV, RangeType::CBV, 0, 1, ShaderVisibility::Pixel });
	lightingPassRSDesc.rootParamDescs.push_back({ RootParamType::DescriptorTable, RangeType::SRV, 0, 1, ShaderVisibility::Pixel });
	lightingPassRSDesc.rootParamDescs.push_back({ RootParamType::DescriptorTable, RangeType::SRV, 1, 1, ShaderVisibility::Pixel });
	lightingPassRSDesc.rootParamDescs.push_back({ RootParamType::DescriptorTable, RangeType::SRV, 2, 1, ShaderVisibility::Pixel });
	lightingPassRSDesc.rootParamDescs.push_back({ RootParamType::DescriptorTable, RangeType::SRV, 3, 1, ShaderVisibility::Pixel });
	lightingPassRSDesc.staticSamplers.push_back({ SamplerFilter::Point, SamplerAddressMode::Clamp, 0, ShaderVisibility::Pixel });


	m_fullscreenVS = ShaderCompiler::CompileFromFile(
		L"shaders_FullScreen_VS.hlsl",
		"main",
		"vs_5_0"
	);

	m_lightingPS = ShaderCompiler::CompileFromFile(
		L"shaders_Lighting_PS.hlsl",
		"main",
		"ps_5_0"
	);

	m_lightingPassPipelineDesc = {
		lightingPassRSDesc,
		ShaderCompiler::GetBytecode(m_fullscreenVS), ShaderCompiler::GetBytecode(m_lightingPS), vertexAttributes,
		{ Format::R8G8B8A8_UNORM },
		Format::UNKNOWN,
		false, false, ComparisonFunc::Equal,
		CullMode::None };
	m_lightingPassPipeline = device->CreatePipeline(m_lightingPassPipelineDesc);
	// Lighting Pass

	// PBR Pass
	RootSignatureDesc PBRlightingPassRSDesc = {};
	PBRlightingPassRSDesc.rootParamDescs.push_back({ RootParamType::RootCBV, RangeType::CBV, 0, 1, ShaderVisibility::Pixel });
	PBRlightingPassRSDesc.rootParamDescs.push_back({ RootParamType::DescriptorTable, RangeType::SRV, 0, 1, ShaderVisibility::Pixel });
	PBRlightingPassRSDesc.rootParamDescs.push_back({ RootParamType::DescriptorTable, RangeType::SRV, 1, 1, ShaderVisibility::Pixel });
	PBRlightingPassRSDesc.rootParamDescs.push_back({ RootParamType::DescriptorTable, RangeType::SRV, 2, 1, ShaderVisibility::Pixel });
	PBRlightingPassRSDesc.rootParamDescs.push_back({ RootParamType::DescriptorTable, RangeType::SRV, 3, 1, ShaderVisibility::Pixel });
	PBRlightingPassRSDesc.staticSamplers.push_back({ SamplerFilter::Point, SamplerAddressMode::Clamp, 0, ShaderVisibility::Pixel });


	m_PBRlightingPS = ShaderCompiler::CompileFromFile(
		L"shaders_PBRLighting_PS.hlsl",
		"main",
		"ps_5_0"
	);

	m_PBRlightingPassPipelineDesc = {
		PBRlightingPassRSDesc,
		ShaderCompiler::GetBytecode(m_fullscreenVS), ShaderCompiler::GetBytecode(m_PBRlightingPS), vertexAttributes,
		{ Format::R8G8B8A8_UNORM },
		Format::UNKNOWN,
		false, false, ComparisonFunc::Equal,
		CullMode::None };
	m_PBRlightingPassPipeline = device->CreatePipeline(m_PBRlightingPassPipelineDesc);
	// PBR Pass


	// Skybox Pass
	RootSignatureDesc equirectConvertRSDesc = {};
	equirectConvertRSDesc.rootParamDescs.push_back({ RootParamType::DescriptorTable, RangeType::SRV, 0, 1, ShaderVisibility::All });
	equirectConvertRSDesc.rootParamDescs.push_back({ RootParamType::DescriptorTable, RangeType::UAV, 0, 1, ShaderVisibility::All });
	equirectConvertRSDesc.rootParamDescs.push_back({ RootParamType::RootConstants, RangeType::CBV, 0, 1, ShaderVisibility::All });
	equirectConvertRSDesc.staticSamplers.push_back({ SamplerFilter::Bilinear, SamplerAddressMode::Wrap, 0, ShaderVisibility::All });

	m_equirectConvertCS = ShaderCompiler::CompileFromFile(
		L"shaders_Skybox_CS.hlsl",
		"CSMain",
		"cs_5_1"
	);

	ComputePipelineDesc equirectConvertPSODesc = {};
	equirectConvertPSODesc.rootSignatureDesc = equirectConvertRSDesc;
	equirectConvertPSODesc.cs = ShaderCompiler::GetBytecode(m_equirectConvertCS);
	m_equirectCSPipeline = device->CreateComputePipeline(equirectConvertPSODesc);

	AssetLoader loader;
	int w, h, ch;
	float* hdrData = loader.LoadHDR("../citrus_orchard_puresky_4k.hdr", w, h, ch, 4);

	TextureDesc equirectDesc = {};
	equirectDesc.width = w;
	equirectDesc.height = h;
	equirectDesc.format = Format::R32G32B32A32_FLOAT;
	equirectDesc.usage = TextureUsage::ShaderResource;
	m_equirectTexture = device->CreateTexture(equirectDesc, hdrData);

	loader.FreeImage(hdrData);

	const uint32_t kCubemapSize = 4096;

	CubemapTextureDesc cubemapDesc = {};
	cubemapDesc.width = kCubemapSize;
	cubemapDesc.height = kCubemapSize;
	cubemapDesc.format = Format::R16G16B16A16_FLOAT;
	m_cubemapTexture = device->CreateCubemapTexture(cubemapDesc);


	device->ExecuteImmediate(
		[&](CommandContext& ctx) {
			ctx.SetComputePipeline(m_equirectCSPipeline);
			ctx.SetComputeDescriptorTable(0, device->GetSRVHeapSlot(m_equirectTexture));
			ctx.SetComputeDescriptorTable(1, device->GetUAVHeapSlot(m_cubemapTexture));
			ctx.SetComputeRootConstants(2, &kCubemapSize, 1);
			ctx.Dispatch(kCubemapSize / 8, kCubemapSize / 8, 6);
			ctx.TransitionBarrier(m_cubemapTexture, RGResourceState::UnorderedAccess, RGResourceState::ShaderResource);
		}
	);

	RootSignatureDesc skyboxPassRSDesc = {};
	skyboxPassRSDesc.rootParamDescs.push_back({ RootParamType::RootCBV, RangeType::CBV, 0, 1, ShaderVisibility::Pixel });
	skyboxPassRSDesc.rootParamDescs.push_back({ RootParamType::DescriptorTable, RangeType::SRV, 0, 1, ShaderVisibility::Pixel });
	skyboxPassRSDesc.staticSamplers.push_back({ SamplerFilter::Bilinear, SamplerAddressMode::Clamp, 0, ShaderVisibility::Pixel });


	m_skyboxPS = ShaderCompiler::CompileFromFile(
		L"shaders_Skybox_PS.hlsl", "main", "ps_5_0"
	);

	PipelineDesc skyboxPSODesc = {};
	skyboxPSODesc.rootSignatureDesc = skyboxPassRSDesc;
	skyboxPSODesc.vs = ShaderCompiler::GetBytecode(m_fullscreenVS);
	skyboxPSODesc.ps = ShaderCompiler::GetBytecode(m_skyboxPS);
	skyboxPSODesc.rtvFormats = { Format::R8G8B8A8_UNORM }; // 나중에 HDR RT로 교체
	skyboxPSODesc.dsvFormat = Format::D32_FLOAT;
	skyboxPSODesc.depthEnable = true;
	skyboxPSODesc.depthWrite = false;
	skyboxPSODesc.depthFunc = ComparisonFunc::LessEqual;
	skyboxPSODesc.cullMode = CullMode::None;
	m_skyboxPipeline = device->CreatePipeline(skyboxPSODesc);
	// Skybox Pass


	// Debug
	m_debugPS = ShaderCompiler::CompileFromFile(
		L"debugshaders_PS.hlsl",
		"main",
		"ps_5_0"
	);

	m_depthDebugPS = ShaderCompiler::CompileFromFile(
		L"debugshaders_Depth_PS.hlsl",
		"main",
		"ps_5_0"
	);

	RootSignatureDesc debugPassRSDesc = {};
	debugPassRSDesc.rootParamDescs.push_back({ RootParamType::DescriptorTable, RangeType::SRV, 0, 1, ShaderVisibility::Pixel });
	debugPassRSDesc.staticSamplers.push_back({ SamplerFilter::Anisotropic, SamplerAddressMode::Wrap, 0, ShaderVisibility::Pixel });


	m_debugPipelineDesc = {
		debugPassRSDesc,
		ShaderCompiler::GetBytecode(m_fullscreenVS), ShaderCompiler::GetBytecode(m_debugPS), {},
		{ Format::R8G8B8A8_UNORM }, 
		Format::UNKNOWN, 
		false, false, ComparisonFunc::Equal,
		CullMode::None 
	};

	m_debugPipeline = device->CreatePipeline(m_debugPipelineDesc);

	m_depthDebugPipelineDesc = {
		debugPassRSDesc,
		ShaderCompiler::GetBytecode(m_fullscreenVS), ShaderCompiler::GetBytecode(m_depthDebugPS), {},
		{ Format::R8G8B8A8_UNORM },
		Format::UNKNOWN,
		false, false, ComparisonFunc::Equal,
		CullMode::None 
	};

	m_depthDebugPipeline = device->CreatePipeline(m_depthDebugPipelineDesc);

	debugMode = DebugMode::PBR_Enabled;
	// Debug
}

void Renderer::Render(GraphicsDevice* device, const Scene& scene)
{
	ReloadPSO(device);

	if (m_needsResize)
		Resize(device);

	CommandContext& ctx = device->BeginFrame();

	RenderGraph graph(device);

	RGResourceDesc backBufferDesc = { m_width, m_height, Format::R8G8B8A8_UNORM, TextureUsage::RenderTarget};
	auto backBuffer = graph.ImportTexture(device->GetCurrentBackBuffer(), backBufferDesc, RGResourceState::Present);

	RGResourceDesc gbufferAlbedoDesc = { m_width, m_height, Format::R8G8B8A8_UNORM, TextureUsage::RenderTarget };
	auto gbufferAlbedo = graph.ImportTexture(m_gbufferAlbedo, gbufferAlbedoDesc, RGResourceState::RenderTarget);

	RGResourceDesc gbufferNormalDesc = { m_width, m_height, Format::R16G16B16A16_FLOAT, TextureUsage::RenderTarget };
	auto gbufferNormal = graph.ImportTexture(m_gbufferNormal, gbufferNormalDesc, RGResourceState::RenderTarget);

	RGResourceDesc gbufferMRDesc = { m_width, m_height, Format::R8G8B8A8_UNORM, TextureUsage::RenderTarget };
	auto gbufferMR = graph.ImportTexture(m_gbufferMR, gbufferMRDesc, RGResourceState::RenderTarget);

	RGResourceDesc depthTextrueDesc = { m_width, m_height, Format::R32_TYPELESS, TextureUsage::DepthStencil };
	auto depthTexture = graph.ImportTexture(m_depthTexture, depthTextrueDesc, RGResourceState::DepthWrite);

	RGResourceDesc skyboxTextureDesc = { m_width, m_height, Format::R16G16B16A16_FLOAT, TextureUsage::ShaderResource };
	auto skyboxTexture = graph.ImportTexture(m_cubemapTexture, skyboxTextureDesc, RGResourceState::ShaderResource);

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
			passCtx.BeginTimestamp(PassID::DepthPrePass);
			{
				passCtx.ClearDepthStencil(m_depthTexture, 1.0f);
				passCtx.SetRenderTarget(0, {}, m_depthTexture);
				passCtx.SetPipeline(m_depthPrePassPipeline);

				perFrameCBHandle = passCtx.UpdateConstantBuffer(0, &perFrame, sizeof(perFrame));
				passCtx.BindConstantBuffer(0, perFrameCBHandle);

				for (const auto& obj : scene.renderObjects)
				{
					if (obj.material.alphaMode == AlphaMode::Opaque)
					{
						auto perObjectCBHandle = passCtx.UpdateConstantBuffer(1, &obj.world, sizeof(obj.world));
						passCtx.BindConstantBuffer(1, perObjectCBHandle);
						passCtx.SetVertexBuffer(obj.vertexBuffer);
						passCtx.SetIndexBuffer(obj.indexBuffer);
						passCtx.DrawIndexed(obj.indexCount, obj.indexOffset, 0);
					}
				}
			}
			passCtx.EndTimestamp(PassID::DepthPrePass);
		}
	);


	graph.AddPass(
		"GBufferOpaquePass",
		[&](RGBuilder& builder) {
			builder.Read(depthTexture, RGResourceState::DepthRead);
			builder.Write(gbufferAlbedo, RGResourceState::RenderTarget);
			builder.Write(gbufferNormal, RGResourceState::RenderTarget);
			builder.Write(gbufferMR, RGResourceState::RenderTarget);
		},
		[&](CommandContext& passCtx) {
			passCtx.BeginTimestamp(PassID::GBufferPass);
			{
				TextureHandle renderTargets[] = { m_gbufferAlbedo, m_gbufferNormal, m_gbufferMR };
				passCtx.ClearRenderTargets(3, renderTargets, clearColor);
				passCtx.SetRenderTarget(3, renderTargets, m_depthTexture);

				passCtx.SetPipeline(m_gBufferOpaquePassPipeline);
				passCtx.BindConstantBuffer(0, perFrameCBHandle);
				for (const auto& obj : scene.renderObjects)
				{
					if (obj.material.alphaMode == AlphaMode::Opaque)
					{
						MaterialConstants matConst;
						matConst.alphaCutoff = obj.material.alphaCutoff;
						matConst.metallicFactor = obj.material.metallicFactor;
						matConst.roughnessFactor = obj.material.roughnessFactor;

						passCtx.SetRootConstants(5, &matConst, 3);

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
			}
			passCtx.EndTimestamp(PassID::GBufferPass);
		}
	);

	graph.AddPass(
		"GBufferAlphaPass",
		[&](RGBuilder& builder) {
			builder.Write(depthTexture, RGResourceState::DepthWrite);
			builder.Write(gbufferAlbedo, RGResourceState::RenderTarget);
			builder.Write(gbufferNormal, RGResourceState::RenderTarget);
			builder.Write(gbufferMR, RGResourceState::RenderTarget);
		},
		[&](CommandContext& passCtx) {
			passCtx.BeginTimestamp(PassID::GBufferAlphaPass);
			{
				TextureHandle renderTargets[] = { m_gbufferAlbedo, m_gbufferNormal, m_gbufferMR };
				passCtx.SetRenderTarget(3, renderTargets, m_depthTexture);
				passCtx.SetPipeline(m_gBufferAlphaPassPipeline);
				passCtx.BindConstantBuffer(0, perFrameCBHandle);
				for (const auto& obj : scene.renderObjects)
				{
					if (obj.material.alphaMode == AlphaMode::Mask)
					{
						MaterialConstants matConst;
						matConst.alphaCutoff = obj.material.alphaCutoff;
						matConst.metallicFactor = obj.material.metallicFactor;
						matConst.roughnessFactor = obj.material.roughnessFactor;

						passCtx.SetRootConstants(5, &matConst, 3);

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
			}
			passCtx.EndTimestamp(PassID::GBufferAlphaPass);
		}
	);

	if (debugMode == DebugMode::PBR_Enabled)
	{
		graph.AddPass(
			"PBRLightingPass",
			[&](RGBuilder& builder) {
				builder.Read(depthTexture, RGResourceState::ShaderResource);
				builder.Read(gbufferAlbedo, RGResourceState::ShaderResource);
				builder.Read(gbufferNormal, RGResourceState::ShaderResource);
				builder.Read(gbufferMR, RGResourceState::ShaderResource);
				builder.Write(backBuffer, RGResourceState::RenderTarget);
			},
			[&](CommandContext& passCtx) {
				passCtx.BeginTimestamp(PassID::PBRLightingPass);
				{
					passCtx.ClearRenderTarget(device->GetCurrentBackBuffer(), clearColor);
					passCtx.SetRenderTarget(1, device->GetCurrentBackBufferPtr(), {});
					passCtx.SetPipeline(m_PBRlightingPassPipeline);

					PBRLightingData PBRlightingData;
					XMMATRIX inverseVP = XMMatrixInverse(nullptr, vp);
					XMStoreFloat4x4(&PBRlightingData.gInvViewProj, inverseVP);
					PBRlightingData.gCameraPos = scene.cam.GetPos();
					PBRlightingData.gLightDir = { -0.5f, -1.0f, -0.5f };
					float len = sqrtf(
						PBRlightingData.gLightDir.x * PBRlightingData.gLightDir.x +
						PBRlightingData.gLightDir.y * PBRlightingData.gLightDir.y +
						PBRlightingData.gLightDir.z * PBRlightingData.gLightDir.z);
					PBRlightingData.gLightDir.x /= len;
					PBRlightingData.gLightDir.y /= len;
					PBRlightingData.gLightDir.z /= len;
					PBRlightingData.gLightColor = { 4.0f, 4.0f, 4.0f };
					auto PBRligthDataCBHandle = passCtx.UpdateConstantBuffer(0, &PBRlightingData, sizeof(PBRlightingData));
					passCtx.BindConstantBuffer(0, PBRligthDataCBHandle);

					passCtx.BindTexture(m_depthTexture, 1);
					passCtx.BindTexture(m_gbufferAlbedo, 2);
					passCtx.BindTexture(m_gbufferNormal, 3);
					passCtx.BindTexture(m_gbufferMR, 4);

					passCtx.Draw(3, 0);
				}
				passCtx.EndTimestamp(PassID::PBRLightingPass);
			}
		);
	}
	else if (debugMode == DebugMode::PBR_Disabled)
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
				{
					passCtx.ClearRenderTarget(device->GetCurrentBackBuffer(), clearColor);
					passCtx.SetRenderTarget(1, device->GetCurrentBackBufferPtr(), {});
					passCtx.SetPipeline(m_lightingPassPipeline);

					LightingData lightingData;
					XMMATRIX inverseVP = XMMatrixInverse(nullptr, vp);
					XMStoreFloat4x4(&lightingData.inverseVP, XMMatrixTranspose(inverseVP));
					lightingData.cameraPos = scene.cam.GetPos();
					lightingData.direction = { -0.5f, -1.0f, -0.5f };

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

	graph.AddPass(
		"SkyboxPass",
		[&](RGBuilder& builder) {
			builder.Read(depthTexture, RGResourceState::DepthRead);
			builder.Read(skyboxTexture, RGResourceState::ShaderResource);
			builder.Write(backBuffer, RGResourceState::RenderTarget);
		},
		[&](CommandContext& passCtx) {
			passCtx.BeginTimestamp(PassID::SkyboxPass);

			passCtx.SetRenderTarget(1, device->GetCurrentBackBufferPtr(), m_depthTexture);
			passCtx.SetPipeline(m_skyboxPipeline);
			
			XMFLOAT4X4 skyboxCB;
			XMMATRIX inverseVP = XMMatrixInverse(nullptr, vp);
			XMStoreFloat4x4(&skyboxCB, XMMatrixTranspose(inverseVP));
			auto cb = passCtx.UpdateConstantBuffer(0, &skyboxCB, sizeof(skyboxCB));
			passCtx.BindConstantBuffer(0, cb);

			passCtx.BindTexture(m_cubemapTexture, 1);

			passCtx.Draw(3, 0);

			passCtx.EndTimestamp(PassID::SkyboxPass);
		}
	);
	

	graph.Compile();
	graph.Execute(ctx);
	graph.Clear();

	device->EndFrame();

	PrintGPUTimes(device);
}

void Renderer::Resize(GraphicsDevice* device)
{
	TextureDesc gbufferDesc = { m_resizeWidth, m_resizeHeight,
							 Format::R8G8B8A8_UNORM, TextureUsage::RenderTarget };
	m_gbufferAlbedo = device->CreateTexture(gbufferDesc, nullptr);

	gbufferDesc.format = Format::R16G16B16A16_FLOAT;
	m_gbufferNormal = device->CreateTexture(gbufferDesc, nullptr);

	gbufferDesc.format = Format::R8G8B8A8_UNORM;
	m_gbufferMR = device->CreateTexture(gbufferDesc, nullptr);

	TextureDesc depthDesc = { m_resizeWidth, m_resizeHeight,
							   Format::D32_FLOAT, TextureUsage::DepthStencil };
	m_depthTexture = device->CreateTexture(depthDesc, nullptr);

	m_width = m_resizeWidth;
	m_height = m_resizeHeight;

	m_needsResize = false;
}

void Renderer::OnResize(uint32_t width, uint32_t height)
{
	m_resizeWidth = width;
	m_resizeHeight = height;
	m_needsResize = true;
}

void Renderer::ReloadPSO(GraphicsDevice* device)
{
	ShaderCompiler::CheckForChanges();
	bool dirtyDepthVS = ShaderCompiler::IsDirty(m_depthVS);

	bool dirtyGBufferVS = ShaderCompiler::IsDirty(m_gBufferVS);
	bool dirtyGBufferOpaque = ShaderCompiler::IsDirty(m_gBufferOpaquePS);
	bool dirtyGBufferAlpha = ShaderCompiler::IsDirty(m_gBufferAlphaPS);

	bool dirtyFullscreenVS = ShaderCompiler::IsDirty(m_fullscreenVS);
	bool dirtyLightingPS = ShaderCompiler::IsDirty(m_lightingPS);
	bool dirtyDebugPS = ShaderCompiler::IsDirty(m_debugPS);
	bool dirtyDepthDebugPS = ShaderCompiler::IsDirty(m_depthDebugPS);
	bool dirtyPBRLightingPS = ShaderCompiler::IsDirty(m_PBRlightingPS);

	// Depth PrePass
	if (dirtyDepthVS)
	{
		m_depthPrePassPipelinDesc.vs = ShaderCompiler::GetBytecode(m_depthVS);
		m_depthPrePassPipeline = device->CreatePipeline(m_depthPrePassPipelinDesc);
	}

	// GBuffer
	if (dirtyGBufferVS || dirtyGBufferOpaque || dirtyGBufferAlpha)
	{
		if (dirtyGBufferVS)
		{
			auto vs = ShaderCompiler::GetBytecode(m_gBufferVS);
			m_gBufferOpaquePassPipelineDesc.vs = vs;
			m_gBufferAlphaPassPipelineDesc.vs = vs;
		}

		if (dirtyGBufferOpaque)
			m_gBufferOpaquePassPipelineDesc.ps = ShaderCompiler::GetBytecode(m_gBufferOpaquePS);

		if (dirtyGBufferAlpha)
			m_gBufferAlphaPassPipelineDesc.ps = ShaderCompiler::GetBytecode(m_gBufferAlphaPS);

		m_gBufferOpaquePassPipeline = device->CreatePipeline(m_gBufferOpaquePassPipelineDesc);
		m_gBufferAlphaPassPipeline = device->CreatePipeline(m_gBufferAlphaPassPipelineDesc);
	}

	// Lighting
	if (dirtyFullscreenVS || dirtyLightingPS)
	{
		if (dirtyFullscreenVS)
			m_lightingPassPipelineDesc.vs = ShaderCompiler::GetBytecode(m_fullscreenVS);

		if (dirtyLightingPS)
			m_lightingPassPipelineDesc.ps = ShaderCompiler::GetBytecode(m_lightingPS);

		m_lightingPassPipeline = device->CreatePipeline(m_lightingPassPipelineDesc);
	}

	// Debug
	if (dirtyFullscreenVS || dirtyDebugPS || dirtyDepthDebugPS)
	{
		if (dirtyFullscreenVS)
		{
			auto vs = ShaderCompiler::GetBytecode(m_fullscreenVS);
			m_debugPipelineDesc.vs = vs;
			m_depthDebugPipelineDesc.vs = vs;
		}

		if (dirtyDebugPS)
			m_debugPipelineDesc.ps = ShaderCompiler::GetBytecode(m_debugPS);

		if (dirtyDepthDebugPS)
			m_depthDebugPipelineDesc.ps = ShaderCompiler::GetBytecode(m_depthDebugPS);

		m_debugPipeline = device->CreatePipeline(m_debugPipelineDesc);
		m_depthDebugPipeline = device->CreatePipeline(m_depthDebugPipelineDesc);
	}

	// PBR Lighting
	if (dirtyFullscreenVS || dirtyPBRLightingPS)
	{
		if (dirtyFullscreenVS)
			m_PBRlightingPassPipelineDesc.vs = ShaderCompiler::GetBytecode(m_fullscreenVS);

		if (dirtyPBRLightingPS)
			m_PBRlightingPassPipelineDesc.ps = ShaderCompiler::GetBytecode(m_PBRlightingPS);

		m_PBRlightingPassPipeline = device->CreatePipeline(m_PBRlightingPassPipelineDesc);
	}

	// 마지막에 한 번만 clear
	if (dirtyDepthVS)        ShaderCompiler::ClearDirty(m_depthVS);

	if (dirtyGBufferVS)      ShaderCompiler::ClearDirty(m_gBufferVS);
	if (dirtyGBufferOpaque)  ShaderCompiler::ClearDirty(m_gBufferOpaquePS);
	if (dirtyGBufferAlpha)   ShaderCompiler::ClearDirty(m_gBufferAlphaPS);

	if (dirtyFullscreenVS)   ShaderCompiler::ClearDirty(m_fullscreenVS);
	if (dirtyLightingPS)     ShaderCompiler::ClearDirty(m_lightingPS);
	if (dirtyDebugPS)        ShaderCompiler::ClearDirty(m_debugPS);
	if (dirtyDepthDebugPS)   ShaderCompiler::ClearDirty(m_depthDebugPS);
	if (dirtyPBRLightingPS)  ShaderCompiler::ClearDirty(m_PBRlightingPS);
}

void Renderer::PrintGPUTimes(GraphicsDevice* device)
{
	float depthPrePassTime = device->GetTimestampMs(PassID::DepthPrePass);
	float gBufferPassTime = device->GetTimestampMs(PassID::GBufferPass);
	float gBufferAlphaPassTime = device->GetTimestampMs(PassID::GBufferAlphaPass);
	float PBRlightingPassTime = device->GetTimestampMs(PassID::PBRLightingPass);
	float skyboxPassTime = device->GetTimestampMs(PassID::SkyboxPass);

	char buffer[256];
	sprintf_s(
		buffer,
		"GPU Times | DepthPre: %6.3f ms | GBuffer: %6.3f ms | GBufferAlpha: %6.3f ms | PBRLighting: %6.3f ms | Skybox: %6.3f ms\n",
		depthPrePassTime,
		gBufferPassTime,
		gBufferAlphaPassTime,
		PBRlightingPassTime,
		skyboxPassTime);

	OutputDebugStringA(buffer);
}