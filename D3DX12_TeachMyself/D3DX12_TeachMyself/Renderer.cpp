#include "stdafx.h"
#include "Renderer.h"
#include "ShaderCompiler.h"
#include "CommandContext.h"
#include "GPUTimestampProfiler.h"
#include <chrono>
#include "AssetLoader.h"
#include "MokoLogger.h"
#include "MokoTime.h"
#include <random>
#include <imgui_impl_dx12.h>
#include "MokoMath.h"

static inline float PointAABBDistanceSq(const XMFLOAT3& p, const XMFLOAT3& aabbMin, const XMFLOAT3& aabbMax)
{
	float dx = (p.x < aabbMin.x) ? (aabbMin.x - p.x) : ((p.x > aabbMax.x) ? (p.x - aabbMax.x) : 0.0f);
	float dy = (p.y < aabbMin.y) ? (aabbMin.y - p.y) : ((p.y > aabbMax.y) ? (p.y - aabbMax.y) : 0.0f);
	float dz = (p.z < aabbMin.z) ? (aabbMin.z - p.z) : ((p.z > aabbMax.z) ? (p.z - aabbMax.z) : 0.0f);
	return dx * dx + dy * dy + dz * dz;
}
static inline bool AABBIntersectsSphere(const XMFLOAT3& aabbMin, const XMFLOAT3& aabbMax, const XMFLOAT3& center, float radius)
{
	return PointAABBDistanceSq(center, aabbMin, aabbMax) <= radius * radius;
}

void Renderer::Init(GraphicsDevice* device, AssetManager* asset)
{
	m_assetManager = asset;

	BufferDesc bufferDesc{};
	bufferDesc.size = MAX_TRANSFORMS * sizeof(GPUTransformData);
	bufferDesc.stride = sizeof(GPUTransformData);
	bufferDesc.access = MemoryAccess::GpuOnly;
	bufferDesc.usage = BufferUsage::Structured;
	m_transformBuffer = device->CreateBuffer(bufferDesc);

	BufferDesc debugLineVertexBufferDesc{};
	debugLineVertexBufferDesc.size = MAX_DEBUG_LINES * sizeof(DebugLineVertex);
	debugLineVertexBufferDesc.stride = sizeof(DebugLineVertex);
	debugLineVertexBufferDesc.access = MemoryAccess::CpuWrite;
	debugLineVertexBufferDesc.usage = BufferUsage::Vertex;
	m_debugLineBuffer = device->CreateBuffer(debugLineVertexBufferDesc);


	m_viewportWidth = 1280;
	m_viewportHeight = 720;

	ShaderCompiler::Reserve(50);

	m_fullscreenVS = ShaderCompiler::CompileFromFile(
		L"shaders_FullScreen_VS.hlsl",
		"main",
		"vs_6_6"
	);

	InitDepthPrePass(device);
	InitGBufferPass(device);
	InitDirectionalShadowPass(device);
	InitPointShadowPass(device);
	InitSSAOPass(device);
	InitGTAOPass(device);
	InitSSAOBilateralBlurPass(device);
	InitGTAOBilateralBlurPass(device);
	InitPBRLightingPass(device);
	InitSkyboxPass(device);
	InitPresentPass(device);

	InitDebugLinePass(device);

	CreateCubeMap(device);
	CreateIrradianceMap(device);
	CreateBRDFLUT(device);
	CreatePrefilteredEnvironmentMap(device);

	CubemapTextureDesc desc = { 1, 1, Format::D32_FLOAT, TextureUsage::DepthStencil };
	m_defaultCubemapTexture = device->CreateDSCubemapTexture(desc);

	hemisphereSamples.resize(32);

	std::uniform_real_distribution<float> dist(0.0f, 1.0f);
	std::mt19937 rng(std::random_device{}());

	for (int i = 0; i < 32; i++)
	{
		XMFLOAT3 sample(
			dist(rng) * 2.0f - 1.0f,
			dist(rng) * 2.0f - 1.0f,
			dist(rng)
		);

		XMVECTOR v = XMVector3Normalize(XMLoadFloat3(&sample));

		float scale = (float)i / 31.0f;
		scale = 0.1f + 0.9f * (scale * scale);

		v *= scale;

		XMStoreFloat4(&hemisphereSamples[i], v);
	}
}

void Renderer::Render(GraphicsDevice* device, CommandContext& ctx, const RenderScene& renderScene)
{
	ReloadPSO(device);

	if (m_needsResize)
		Resize(device);

	CollectDebugLines(renderScene);
	if (!m_debugLines.empty())
	{
		device->UpdateBuffer(
			m_debugLineBuffer,
			m_debugLines.data(),
			m_debugLines.size() * sizeof(DebugLineVertex));
	}

	if (!renderScene.transforms.empty())
	{
		device->UpdateBuffer(
			m_transformBuffer,
			renderScene.transforms.data(),
			renderScene.transforms.size() * sizeof(GPUTransformData));
	}

	RenderGraph graph(device);
	FrameContext fc = BuildFrameContext(device, ctx, graph, renderScene);

	UpdateIndirectArgBuffers(device, renderScene);

	BuildSceneGraph(device, graph, fc, renderScene);

	BuildPresentGraph(device, graph, fc);

	BuildDebugGraph(device, graph, fc);

	graph.Compile();
	graph.Execute(ctx);
}

void Renderer::Resize(GraphicsDevice* device)
{
	if (m_gbufferAlbedo.IsValid()) device->DestroyTexture(m_gbufferAlbedo);
	if (m_gbufferNormal.IsValid()) device->DestroyTexture(m_gbufferNormal);
	if (m_gbufferMR.IsValid()) device->DestroyTexture(m_gbufferMR);
	if (m_gbufferEmissive.IsValid()) device->DestroyTexture(m_gbufferEmissive);
	if (m_depthTexture.IsValid()) device->DestroyTexture(m_depthTexture);
	if (m_ssaoTexture.IsValid()) device->DestroyTexture(m_ssaoTexture);
	if (m_ssaoTempTexture.IsValid()) device->DestroyTexture(m_ssaoTempTexture);
	if (m_gtaoTexture.IsValid()) device->DestroyTexture(m_gtaoTexture);
	if (m_gtaoTempTexture.IsValid()) device->DestroyTexture(m_gtaoTempTexture);
	if (m_sceneColorTexture.IsValid()) device->DestroyTexture(m_sceneColorTexture);
	if (m_sceneColorLDRTexture.IsValid()) device->DestroyTexture(m_sceneColorLDRTexture);

	TextureDesc gbufferDesc =
	{
		m_resizeWidth,
		m_resizeHeight,
		1, 1,
		Format::R8G8B8A8_UNORM,
		TextureUsage::RenderTarget,
	};
	m_gbufferAlbedo = device->CreateRTTexture(gbufferDesc);

	gbufferDesc.format = Format::R16G16B16A16_FLOAT;
	m_gbufferNormal = device->CreateRTTexture(gbufferDesc);

	gbufferDesc.format = Format::R8G8B8A8_UNORM;
	m_gbufferMR = device->CreateRTTexture(gbufferDesc);

	gbufferDesc.format = Format::R11G11B10_FLOAT;
	m_gbufferEmissive = device->CreateRTTexture(gbufferDesc);

	TextureDesc depthDesc = 
	{ 
		m_resizeWidth, 
		m_resizeHeight, 
		1, 1,
		Format::D32_FLOAT, 
		TextureUsage::DepthStencil,
		false
	};
	m_depthTexture = device->CreateDSTexture(depthDesc);

	TextureDesc ssaoTextureDesc = 
	{
		m_resizeWidth / 2,
		m_resizeHeight / 2, 
		1, 1,
		Format::R8_UNORM, 
		TextureUsage::RenderTarget,
		false
	};
	m_ssaoTexture = device->CreateRTTexture(ssaoTextureDesc);
	m_ssaoTempTexture = device->CreateRTTexture(ssaoTextureDesc);

	TextureDesc gtaoTextureDesc = 
	{
		m_resizeWidth / 2, 
		m_resizeHeight / 2,
		1, 1,
		Format::R8G8B8A8_UNORM,
		TextureUsage::UnorderedAccess,
		false
	};
	m_gtaoTexture = device->CreateUAVTexture(gtaoTextureDesc);
	m_gtaoTempTexture = device->CreateUAVTexture(gtaoTextureDesc);

	TextureDesc sceneColorTextureDesc = 
	{
		m_resizeWidth, 
		m_resizeHeight, 
		1, 1,
		Format::R16G16B16A16_FLOAT,
		TextureUsage::RenderTarget,
		false
	};
	m_sceneColorTexture = device->CreateRTTexture(sceneColorTextureDesc);

	TextureDesc sceneColorLDRTextureDesc =
	{
		m_resizeWidth,
		m_resizeHeight,
		1, 1,
		Format::R8G8B8A8_UNORM_SRGB,
		TextureUsage::RenderTarget,
		false
	};
	m_sceneColorLDRTexture = device->CreateRTTexture(sceneColorLDRTextureDesc);

	m_viewportWidth = m_resizeWidth;
	m_viewportHeight = m_resizeHeight;

	m_needsResize = false;
}

void Renderer::OnViewportResize(uint32_t width, uint32_t height)
{
	m_resizeWidth = width;
	m_resizeHeight = height;
	m_needsResize = true;
}

DebugViewMode Renderer::GetDebugViewMode()
{
	return m_debugViewMode;
}

void Renderer::SetDebugViewMode(DebugViewMode viewMode)
{
	m_debugViewMode = viewMode;
}

void Renderer::DrawAABB(const XMFLOAT3& mn, const XMFLOAT3& mx, const XMFLOAT4& color)
{
	XMFLOAT3 c[8] = {
		{mn.x, mn.y, mn.z}, {mx.x, mn.y, mn.z},
		{mn.x, mx.y, mn.z}, {mx.x, mx.y, mn.z},
		{mn.x, mn.y, mx.z}, {mx.x, mn.y, mx.z},
		{mn.x, mx.y, mx.z}, {mx.x, mx.y, mx.z},
	};
	static const int edges[12][2] = {
		{0,1},{2,3},{4,5},{6,7},   // X edges
		{0,2},{1,3},{4,6},{5,7},   // Y edges
		{0,4},{1,5},{2,6},{3,7},   // Z edges
	};
	for (auto& e : edges)
	{
		m_debugLines.push_back({ c[e[0]], color });
		m_debugLines.push_back({ c[e[1]], color });
	}
}

bool Renderer::GetShowAABB()
{
	return m_showAABB;
}

void Renderer::SetShowAABB(bool value)
{
	m_showAABB = value;
}

void Renderer::ReloadPSO(GraphicsDevice* device)
{
	ShaderCompiler::CheckForChanges();
	bool dirtyDepthVS = ShaderCompiler::IsDirty(m_depthVS);

	bool dirtyGBufferVS = ShaderCompiler::IsDirty(m_gBufferVS);
	bool dirtyGBufferOpaque = ShaderCompiler::IsDirty(m_gBufferOpaquePS);
	bool dirtyGBufferAlpha = ShaderCompiler::IsDirty(m_gBufferAlphaPS);

	bool dirtyShadowMapVS = ShaderCompiler::IsDirty(m_shadowMapVS);
	bool dirtyPointShadowMapVS = ShaderCompiler::IsDirty(m_pointShadowMapVS);

	bool dirtyFullscreenVS = ShaderCompiler::IsDirty(m_fullscreenVS);
	bool dirtySSAOVS = ShaderCompiler::IsDirty(m_ssaoVS);
	bool dirtySSAOPS = ShaderCompiler::IsDirty(m_ssaoPS);
	bool dirtyBilateralBlurPSVertical = ShaderCompiler::IsDirty(m_bilateralBlurPS_Vertical);
	bool dirtyBilateralBlurPSHorizontal = ShaderCompiler::IsDirty(m_bilateralBlurPS_Horizontal);
	bool dirtyPBRLightingPS = ShaderCompiler::IsDirty(m_PBRlightingPS);
	bool dirtySkyboxPS = ShaderCompiler::IsDirty(m_skyboxPS);
	bool dirtyPresentPS = ShaderCompiler::IsDirty(m_presentPS);

	bool dirtyGTAOCS = ShaderCompiler::IsDirty(m_GTAOCS);
	bool dirtyBilateralBlurCS = ShaderCompiler::IsDirty(m_bilateralBlurCS);

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

	// SSAO
	if (dirtySSAOVS || dirtySSAOPS)
	{
		if (dirtySSAOVS)
			m_SSAOPipelineDesc.vs = ShaderCompiler::GetBytecode(m_ssaoVS);
		if (dirtySSAOPS)
			m_SSAOPipelineDesc.ps = ShaderCompiler::GetBytecode(m_ssaoPS);

		m_SSAOPipeline = device->CreatePipeline(m_SSAOPipelineDesc);
	}

	// Directional / point shadow
	if (dirtyShadowMapVS)
	{
		m_shadowMapPipelineDesc.vs = ShaderCompiler::GetBytecode(m_shadowMapVS);
		m_shadowMapPipeline = device->CreatePipeline(m_shadowMapPipelineDesc);
	}

	if (dirtyPointShadowMapVS)
	{
		m_pointShadowMapPipelineDesc.vs = ShaderCompiler::GetBytecode(m_pointShadowMapVS);
		m_pointShadowMapPipeline = device->CreatePipeline(m_pointShadowMapPipelineDesc);
	}

	// SSAO bilateral blur
	if (dirtyFullscreenVS || dirtyBilateralBlurPSVertical || dirtyBilateralBlurPSHorizontal)
	{
		if (dirtyFullscreenVS)
			m_bilateralBlurPipelineDesc.vs = ShaderCompiler::GetBytecode(m_fullscreenVS);
		if (dirtyBilateralBlurPSVertical)
			m_bilateralBlurPipelineDesc.ps = ShaderCompiler::GetBytecode(m_bilateralBlurPS_Vertical);

		m_bilateralBlurPipeline_Vertical = device->CreatePipeline(m_bilateralBlurPipelineDesc);

		if (dirtyBilateralBlurPSHorizontal)
			m_bilateralBlurPipelineDesc.ps = ShaderCompiler::GetBytecode(m_bilateralBlurPS_Horizontal);

		m_bilateralBlurPipeline_Horizontal = device->CreatePipeline(m_bilateralBlurPipelineDesc);
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

	// Skybox
	if (dirtyFullscreenVS || dirtySkyboxPS)
	{
		if (dirtyFullscreenVS)
			m_skyboxPipelineDesc.vs = ShaderCompiler::GetBytecode(m_fullscreenVS);
		if (dirtySkyboxPS)
			m_skyboxPipelineDesc.ps = ShaderCompiler::GetBytecode(m_skyboxPS);

		m_skyboxPipeline = device->CreatePipeline(m_skyboxPipelineDesc);
	}

	// Present
	if (dirtyFullscreenVS || dirtyPresentPS)
	{
		if (dirtyFullscreenVS)
			m_presentPipelineDesc.vs = ShaderCompiler::GetBytecode(m_fullscreenVS);
		if (dirtyPresentPS)
			m_presentPipelineDesc.ps = ShaderCompiler::GetBytecode(m_presentPS);

		m_presentPipeline = device->CreatePipeline(m_presentPipelineDesc);
	}

	// GTAO
	if (dirtyGTAOCS)
	{
		m_GTAOComputePipelineDesc.cs = ShaderCompiler::GetBytecode(m_GTAOCS);
		m_GTAOComputePipeline = device->CreateComputePipeline(m_GTAOComputePipelineDesc);
	}

	// GTAO bilateral blur
	if (dirtyBilateralBlurCS)
	{
		m_bilateralBlurComputePipelineDesc.cs = ShaderCompiler::GetBytecode(m_bilateralBlurCS);
		m_bilateralBlurComputePipeline = device->CreateComputePipeline(m_bilateralBlurComputePipelineDesc);
	}

	// 마지막에 한 번만 clear
	if (dirtyDepthVS)        ShaderCompiler::ClearDirty(m_depthVS);

	if (dirtyGBufferVS)      ShaderCompiler::ClearDirty(m_gBufferVS);
	if (dirtyGBufferOpaque)  ShaderCompiler::ClearDirty(m_gBufferOpaquePS);
	if (dirtyGBufferAlpha)   ShaderCompiler::ClearDirty(m_gBufferAlphaPS);
	if (dirtyShadowMapVS)    ShaderCompiler::ClearDirty(m_shadowMapVS);
	if (dirtyPointShadowMapVS) ShaderCompiler::ClearDirty(m_pointShadowMapVS);

	if (dirtyFullscreenVS)   ShaderCompiler::ClearDirty(m_fullscreenVS);
	if (dirtySSAOVS)         ShaderCompiler::ClearDirty(m_ssaoVS);
	if (dirtySSAOPS)         ShaderCompiler::ClearDirty(m_ssaoPS);
	if (dirtyBilateralBlurPSVertical) ShaderCompiler::ClearDirty(m_bilateralBlurPS_Vertical);
	if (dirtyBilateralBlurPSHorizontal) ShaderCompiler::ClearDirty(m_bilateralBlurPS_Horizontal);
	if (dirtyPBRLightingPS)  ShaderCompiler::ClearDirty(m_PBRlightingPS);
	if (dirtySkyboxPS)       ShaderCompiler::ClearDirty(m_skyboxPS);
	if (dirtyPresentPS)      ShaderCompiler::ClearDirty(m_presentPS);

	if (dirtyGTAOCS)		 ShaderCompiler::ClearDirty(m_GTAOCS);
	if (dirtyBilateralBlurCS) ShaderCompiler::ClearDirty(m_bilateralBlurCS);
}

void Renderer::CreateCubeMap(GraphicsDevice* device)
{
	RootSignatureDesc equirectConvertRSDesc = {};
	equirectConvertRSDesc.allowIA = false;
	equirectConvertRSDesc.rootParamDescs.push_back({ RootParamType::DescriptorTable, RangeType::SRV, 0, 0, 1, ShaderVisibility::All });
	equirectConvertRSDesc.rootParamDescs.push_back({ RootParamType::DescriptorTable, RangeType::UAV, 0, 0, 1, ShaderVisibility::All });
	equirectConvertRSDesc.rootParamDescs.push_back({ RootParamType::RootConstants, RangeType::CBV, 0, 0, 1, ShaderVisibility::All });
	equirectConvertRSDesc.staticSamplers.push_back({ SamplerFilter::Bilinear, SamplerAddressMode::Wrap, 0, ShaderVisibility::All });

	m_equirectConvertCS = ShaderCompiler::CompileFromFile(
		L"shaders_Skybox_CS.hlsl",
		"CSMain",
		"cs_6_6"
	);

	ComputePipelineDesc equirectConvertPSODesc = {};
	equirectConvertPSODesc.rootSignatureDesc = equirectConvertRSDesc;
	equirectConvertPSODesc.cs = ShaderCompiler::GetBytecode(m_equirectConvertCS);
	m_equirectCSPipeline = device->CreateComputePipeline(equirectConvertPSODesc);

	AssetLoader loader;
	int w, h, ch;
	//float* hdrData = loader.LoadHDR("../citrus_orchard_puresky_4k.hdr", w, h, ch, 4);
	float* hdrData = loader.LoadHDR("../venice_sunset_4k.hdr", w, h, ch, 4);
	//float* hdrData = loader.LoadHDR("../qwantani_dusk_2_puresky_4k.hdr", w, h, ch, 4);
	//float* hdrData = loader.LoadHDR("../qwantani_night_4k.hdr", w, h, ch, 4);

	SubresourceData sub{
	.data = hdrData,
	.rowPitch = w * sizeof(float) * 4,
	.slicePitch = w * h * sizeof(float) * 4,
	};
	TextureInitDesc init{
		.desc = {
			.width = static_cast<uint32_t>(w), 
			.height = static_cast<uint32_t>(h),
			.mipLevels = 1, .arraySize = 1,
			.format = Format::R32G32B32A32_FLOAT,
			.isCubemap = false,
		},
		.subresources = std::span(&sub, 1),
	};
	m_equirectTexture = device->CreateTexture(init);

	loader.FreeImage(hdrData);

	const uint32_t kCubemapSize = 4096;

	CubemapTextureDesc cubemapDesc = {};
	cubemapDesc.width = kCubemapSize;
	cubemapDesc.height = kCubemapSize;
	cubemapDesc.usage = TextureUsage::UnorderedAccess;
	cubemapDesc.format = Format::R16G16B16A16_FLOAT;
	m_cubemapTexture = device->CreateUAVCubemapTexture(cubemapDesc);

	device->ExecuteImmediate(
		[&](CommandContext& ctx) {
			ctx.SetComputePipeline(m_equirectCSPipeline);
			ctx.SetComputeDescriptorTable(0, device->GetSRVHandle(m_equirectTexture));
			ctx.SetComputeDescriptorTable(1, device->GetUAVHandle(m_cubemapTexture));
			ctx.SetComputeRootConstants(2, &kCubemapSize, 1);
			ctx.Dispatch(kCubemapSize / 8, kCubemapSize / 8, 6);

			ctx.TransitionBarrier(m_cubemapTexture, RGResourceState::UnorderedAccess, RGResourceState::ShaderResource);
		}
	);
}
void Renderer::CreateIrradianceMap(GraphicsDevice* device)
{
	// Irradiance Map
	RootSignatureDesc irradianceMapRSDesc = {};
	irradianceMapRSDesc.allowIA = false;
	irradianceMapRSDesc.rootParamDescs.push_back({ RootParamType::DescriptorTable, RangeType::SRV, 0, 0, 1, ShaderVisibility::All });
	irradianceMapRSDesc.rootParamDescs.push_back({ RootParamType::DescriptorTable, RangeType::UAV, 0, 0, 1, ShaderVisibility::All });
	irradianceMapRSDesc.staticSamplers.push_back({ SamplerFilter::Bilinear, SamplerAddressMode::Wrap, 0, ShaderVisibility::All });

	m_irradianceMapCS = ShaderCompiler::CompileFromFile(
		L"shaders_IrradianceMap_CS.hlsl",
		"CSMain",
		"cs_6_6"
	);

	ComputePipelineDesc irradianceMapPSODesc = {};
	irradianceMapPSODesc.rootSignatureDesc = irradianceMapRSDesc;
	irradianceMapPSODesc.cs = ShaderCompiler::GetBytecode(m_irradianceMapCS);
	m_irradianceMapCSPipeline = device->CreateComputePipeline(irradianceMapPSODesc);

	CubemapTextureDesc irradianceCubeMapDesc = {};
	irradianceCubeMapDesc.width = 32;
	irradianceCubeMapDesc.height = 32;
	irradianceCubeMapDesc.usage = TextureUsage::UnorderedAccess;
	irradianceCubeMapDesc.format = Format::R16G16B16A16_FLOAT;
	m_irradianceMapTexture = device->CreateUAVCubemapTexture(irradianceCubeMapDesc);

	device->ExecuteImmediate(
		[&](CommandContext& ctx) {
			ctx.SetComputePipeline(m_irradianceMapCSPipeline);
			ctx.SetComputeDescriptorTable(0, device->GetSRVHandle(m_cubemapTexture));
			ctx.SetComputeDescriptorTable(1, device->GetUAVHandle(m_irradianceMapTexture));
			ctx.Dispatch(32 / 8, 32 / 8, 6);

			ctx.TransitionBarrier(m_irradianceMapTexture, RGResourceState::UnorderedAccess, RGResourceState::ShaderResource);
		}
	);
	// Irradiance Map
}
void Renderer::CreatePrefilteredEnvironmentMap(GraphicsDevice* device)
{
	// PreFiltered Environment Map
	RootSignatureDesc perfilteredEnvironmentMapDesc = {};
	perfilteredEnvironmentMapDesc.allowIA = false;
	perfilteredEnvironmentMapDesc.rootParamDescs.push_back({ RootParamType::DescriptorTable, RangeType::SRV, 0, 0, 1, ShaderVisibility::All });
	perfilteredEnvironmentMapDesc.rootParamDescs.push_back({ RootParamType::DescriptorTable, RangeType::UAV, 0, 0, 1, ShaderVisibility::All });
	perfilteredEnvironmentMapDesc.rootParamDescs.push_back({ RootParamType::RootConstants, RangeType::CBV, 0, 0, 1, ShaderVisibility::All });
	perfilteredEnvironmentMapDesc.staticSamplers.push_back({ SamplerFilter::Bilinear, SamplerAddressMode::Wrap, 0, ShaderVisibility::All });

	m_prefilteredEnvironmentMapCS = ShaderCompiler::CompileFromFile(
		L"shaders_PrefilteredEnvironmentMap_CS.hlsl",
		"CSMain",
		"cs_6_6"
	);

	ComputePipelineDesc prefilteredEnvironmentMapPSODesc = {};
	prefilteredEnvironmentMapPSODesc.rootSignatureDesc = perfilteredEnvironmentMapDesc;
	prefilteredEnvironmentMapPSODesc.cs = ShaderCompiler::GetBytecode(m_prefilteredEnvironmentMapCS);
	m_prefilteredEnvironmentMapCSPipeline = device->CreateComputePipeline(prefilteredEnvironmentMapPSODesc);

	const uint32_t mipLevels = 5;

	const uint32_t kPrefilteredMapSize = 512;

	CubemapTextureDesc prefiltredEnvironmentCubeMapDesc = {};
	prefiltredEnvironmentCubeMapDesc.width = kPrefilteredMapSize;
	prefiltredEnvironmentCubeMapDesc.height = kPrefilteredMapSize;
	prefiltredEnvironmentCubeMapDesc.usage = TextureUsage::UnorderedAccess;
	prefiltredEnvironmentCubeMapDesc.format = Format::R16G16B16A16_FLOAT;
	prefiltredEnvironmentCubeMapDesc.mipLevels = mipLevels;
	m_prefilteredEnvMapTexture = device->CreateUAVCubemapTexture(prefiltredEnvironmentCubeMapDesc);

	device->ExecuteImmediate(
		[&](CommandContext& ctx) {
			ctx.SetComputePipeline(m_prefilteredEnvironmentMapCSPipeline);
			ctx.SetComputeDescriptorTable(0, device->GetSRVHandle(m_cubemapTexture));

			for (uint32_t mip = 0; mip < mipLevels; mip++)
			{
				float roughness = (float)mip / (float(mipLevels - 1));
				uint32_t mipSize = 512 >> mip;

				ctx.SetComputeDescriptorTable(1, device->GetUAVHandle(m_prefilteredEnvMapTexture, mip));
				ctx.SetComputeRootConstants(2, &roughness, 1);
				ctx.Dispatch(
					std::max(mipSize / 8u, 1u),
					std::max(mipSize / 8u, 1u),
					6
				);
			}

			ctx.TransitionBarrier(m_prefilteredEnvMapTexture, RGResourceState::UnorderedAccess, RGResourceState::ShaderResource);
		}
	);
	// PreFiltered Environment Map
}
void Renderer::CreateBRDFLUT(GraphicsDevice* device)
{
	// BRDF LUT 
	TextureDesc brdfLUTDesc = { 512, 512, 1, 1, Format::R16G16_FLOAT, TextureUsage::UnorderedAccess, false };
	m_brdfLUTTexture = device->CreateUAVTexture(brdfLUTDesc);


	m_brdfLUTCS = ShaderCompiler::CompileFromFile(
		L"shaders_brdfLUT_CS.hlsl",
		"CSMain",
		"cs_6_6"
	);

	RootSignatureDesc brdfLUTRSDesc = {};
	brdfLUTRSDesc.allowIA = false;
	brdfLUTRSDesc.rootParamDescs.push_back({ RootParamType::DescriptorTable, RangeType::UAV, 0, 0, 1, ShaderVisibility::All });

	ComputePipelineDesc brdfLUTPipelineDesc = {};
	brdfLUTPipelineDesc.cs = ShaderCompiler::GetBytecode(m_brdfLUTCS);
	brdfLUTPipelineDesc.rootSignatureDesc = brdfLUTRSDesc;

	m_brdfLUTCSPipeline = device->CreateComputePipeline(brdfLUTPipelineDesc);

	device->ExecuteImmediate(
		[&](CommandContext& ctx) {
			ctx.SetComputePipeline(m_brdfLUTCSPipeline);
			ctx.SetComputeDescriptorTable(0, device->GetUAVHandle(m_brdfLUTTexture));
			ctx.Dispatch(512 / 16, 512 / 16, 1);

			ctx.TransitionBarrier(m_brdfLUTTexture, RGResourceState::UnorderedAccess, RGResourceState::ShaderResource);
		}
	);
	// BRDF LUT
}

void Renderer::CollectDebugLines(const RenderScene& scene)
{
	m_debugLines.clear();

	if (!m_showAABB) return;

	for (const auto& obj : scene.renderObjects)
	{
		XMFLOAT3 corners[8] = 
		{
			{ obj.aabbMin.x, obj.aabbMin.y, obj.aabbMin.z },
			{ obj.aabbMax.x, obj.aabbMin.y, obj.aabbMin.z },
			{ obj.aabbMin.x, obj.aabbMax.y, obj.aabbMin.z },
			{ obj.aabbMax.x, obj.aabbMax.y, obj.aabbMin.z },

			{ obj.aabbMin.x, obj.aabbMin.y, obj.aabbMax.z },
			{ obj.aabbMax.x, obj.aabbMin.y, obj.aabbMax.z },
			{ obj.aabbMin.x, obj.aabbMax.y, obj.aabbMax.z },
			{ obj.aabbMax.x, obj.aabbMax.y, obj.aabbMax.z },
		};

		static const int edges[12][2] = {
			{0,1},{2,3},{4,5},{6,7},
			{0,2},{1,3},{4,6},{5,7},
			{0,4},{1,5},{2,6},{3,7},
		};

		XMFLOAT4 color = { 0, 1, 0, 1 };  

		for (auto& e : edges)
		{
			m_debugLines.push_back({ corners[e[0]], color });
			m_debugLines.push_back({ corners[e[1]], color });
		}
	}
}

void Renderer::BuildCascadeShadowMatrices(
	const XMMATRIX& invViewProj,
	FXMVECTOR lightDir,
	const XMFLOAT3& sceneAABBMin,
	const XMFLOAT3& sceneAABBMax,
	float nearClip,
	float farClip,
	float cascadeSplits[CASCADE_COUNT + 1],
	XMMATRIX outLightViewProj[CASCADE_COUNT])
{
	float lambda = 0.5f;

	for (int i = 0; i <= CASCADE_COUNT; i++)
	{
		float t = (float)i / CASCADE_COUNT;
		float logSplit = nearClip * powf(farClip / nearClip, t);
		float linearSplit = nearClip + (farClip - nearClip) * t;
		cascadeSplits[i] = lambda * logSplit + (1.0f - lambda) * linearSplit;
	}

	XMVECTOR ndcCorners[8] =
	{
		{ -1, -1, 0, 1 }, { -1,  1, 0, 1 }, {  1,  1, 0, 1 }, {  1, -1, 0, 1 },
		{ -1, -1, 1, 1 }, { -1,  1, 1, 1 }, {  1,  1, 1, 1 }, {  1, -1, 1, 1 }
	};

	XMVECTOR worldCorners[8];
	for (int i = 0; i < 8; i++)
	{
		worldCorners[i] = XMVector4Transform(ndcCorners[i], invViewProj);
		worldCorners[i] = XMVectorDivide(worldCorners[i], XMVectorSplatW(worldCorners[i]));
	}

	XMFLOAT3 sceneCorners[8] =
	{
		{ sceneAABBMin.x, sceneAABBMin.y, sceneAABBMin.z },
		{ sceneAABBMax.x, sceneAABBMin.y, sceneAABBMin.z },
		{ sceneAABBMin.x, sceneAABBMax.y, sceneAABBMin.z },
		{ sceneAABBMax.x, sceneAABBMax.y, sceneAABBMin.z },
		{ sceneAABBMin.x, sceneAABBMin.y, sceneAABBMax.z },
		{ sceneAABBMax.x, sceneAABBMin.y, sceneAABBMax.z },
		{ sceneAABBMin.x, sceneAABBMax.y, sceneAABBMax.z },
		{ sceneAABBMax.x, sceneAABBMax.y, sceneAABBMax.z }
	};

	for (int c = 0; c < CASCADE_COUNT; c++)
	{
		float cNear = cascadeSplits[c];
		float cFar = cascadeSplits[c + 1];

		float nearT = (cNear - nearClip) / (farClip - nearClip);
		float farT = (cFar - nearClip) / (farClip - nearClip);

		XMVECTOR cascadeCorners[8];
		for (int i = 0; i < 4; i++)
		{
			XMVECTOR dir = XMVectorSubtract(worldCorners[i + 4], worldCorners[i]);

			cascadeCorners[i] =
				XMVectorAdd(worldCorners[i], XMVectorScale(dir, nearT));

			cascadeCorners[i + 4] =
				XMVectorAdd(worldCorners[i], XMVectorScale(dir, farT));
		}

		XMVECTOR center = XMVectorZero();
		for (int i = 0; i < 8; i++)
			center = XMVectorAdd(center, cascadeCorners[i]);

		center = XMVectorScale(center, 1.0f / 8.0f);

		float sceneDiagonal = XMVectorGetX(
			XMVector3Length(
				XMVectorSubtract(
					XMLoadFloat3(&sceneAABBMax),
					XMLoadFloat3(&sceneAABBMin))));
		sceneDiagonal = std::max(0.1f, sceneDiagonal);

		const XMVECTOR lightForward = XMVectorNegate(lightDir);
		XMMATRIX lightView = XMMatrixLookAtLH(
			XMVectorSubtract(center, XMVectorScale(lightDir, sceneDiagonal)),
			center,
			GetStableUpVector(lightForward));

		XMFLOAT3 mins = { FLT_MAX, FLT_MAX, FLT_MAX };
		XMFLOAT3 maxs = { -FLT_MAX, -FLT_MAX, -FLT_MAX };

		for (int i = 0; i < 8; i++)
		{
			XMVECTOR v = XMVector4Transform(cascadeCorners[i], lightView);

			XMFLOAT3 p;
			XMStoreFloat3(&p, v);

			mins.x = std::min(mins.x, p.x);
			mins.y = std::min(mins.y, p.y);
			mins.z = std::min(mins.z, p.z);

			maxs.x = std::max(maxs.x, p.x);
			maxs.y = std::max(maxs.y, p.y);
			maxs.z = std::max(maxs.z, p.z);
		}

		XMFLOAT3 sceneMins = { FLT_MAX, FLT_MAX, FLT_MAX };
		XMFLOAT3 sceneMaxs = { -FLT_MAX, -FLT_MAX, -FLT_MAX };
		for (int i = 0; i < 8; i++)
		{
			XMVECTOR v = XMVector4Transform(XMLoadFloat3(&sceneCorners[i]), lightView);

			XMFLOAT3 p;
			XMStoreFloat3(&p, v);

			sceneMins.x = std::min(sceneMins.x, p.x);
			sceneMins.y = std::min(sceneMins.y, p.y);
			sceneMins.z = std::min(sceneMins.z, p.z);

			sceneMaxs.x = std::max(sceneMaxs.x, p.x);
			sceneMaxs.y = std::max(sceneMaxs.y, p.y);
			sceneMaxs.z = std::max(sceneMaxs.z, p.z);
		}
		mins.x = std::max(mins.x, sceneMins.x);
		mins.y = std::max(mins.y, sceneMins.y);
		maxs.x = std::min(maxs.x, sceneMaxs.x);
		maxs.y = std::min(maxs.y, sceneMaxs.y);

		if (mins.x >= maxs.x || mins.y >= maxs.y)
		{
			mins.x = std::min(mins.x, sceneMins.x);
			mins.y = std::min(mins.y, sceneMins.y);
			maxs.x = std::max(maxs.x, sceneMaxs.x);
			maxs.y = std::max(maxs.y, sceneMaxs.y);
		}
		mins.z = std::min(mins.z, sceneMins.z);
		maxs.z = std::max(maxs.z, sceneMaxs.z);

		float cascadeWidth = maxs.x - mins.x;
		float cascadeHeight = maxs.y - mins.y;

		float texelSizeX = cascadeWidth / 2048.0f;
		float texelSizeY = cascadeHeight / 2048.0f;

		mins.x = floorf(mins.x / texelSizeX) * texelSizeX;
		maxs.x = floorf(maxs.x / texelSizeX) * texelSizeX;
		mins.y = floorf(mins.y / texelSizeY) * texelSizeY;
		maxs.y = floorf(maxs.y / texelSizeY) * texelSizeY;

		XMMATRIX lightProj = XMMatrixOrthographicOffCenterLH(
			mins.x, maxs.x,
			mins.y, maxs.y,
			mins.z, maxs.z);

		outLightViewProj[c] = lightView * lightProj;
	}
}


FrameContext Renderer::BuildFrameContext(GraphicsDevice* device, CommandContext& ctx, RenderGraph& graph, const RenderScene& renderScene)
{
	auto& frameData = renderScene.frameData;

	RGTextureDesc backBufferDesc = { m_viewportWidth, m_viewportHeight, Format::R8G8B8A8_UNORM, TextureUsage::RenderTarget };
	RGTextureHandle backBuffer = graph.ImportTexture(device->GetCurrentBackBuffer(), backBufferDesc, RGResourceState::Present);

	RGTextureDesc gbufferAlbedoDesc = { m_viewportWidth, m_viewportHeight, Format::R8G8B8A8_UNORM, TextureUsage::RenderTarget };
	RGTextureHandle gbufferAlbedo = graph.ImportTexture(m_gbufferAlbedo, gbufferAlbedoDesc, RGResourceState::RenderTarget);

	RGTextureDesc gbufferNormalDesc = { m_viewportWidth, m_viewportHeight, Format::R16G16B16A16_FLOAT, TextureUsage::RenderTarget };
	RGTextureHandle gbufferNormal = graph.ImportTexture(m_gbufferNormal, gbufferNormalDesc, RGResourceState::RenderTarget);

	RGTextureDesc gbufferMRDesc = { m_viewportWidth, m_viewportHeight, Format::R8G8B8A8_UNORM, TextureUsage::RenderTarget };
	RGTextureHandle gbufferMR = graph.ImportTexture(m_gbufferMR, gbufferMRDesc, RGResourceState::RenderTarget);

	RGTextureDesc gbufferEmissiveDesc = { m_viewportWidth, m_viewportHeight, Format::R11G11B10_FLOAT, TextureUsage::RenderTarget };
	RGTextureHandle gbufferEmissive = graph.ImportTexture(m_gbufferEmissive, gbufferEmissiveDesc, RGResourceState::RenderTarget);

	RGTextureDesc depthTextrueDesc = { m_viewportWidth, m_viewportHeight, Format::R32_TYPELESS, TextureUsage::DepthStencil };
	RGTextureHandle depthTexture = graph.ImportTexture(m_depthTexture, depthTextrueDesc, RGResourceState::DepthWrite);

	RGTextureDesc cubeMapDesc = { 4096, 4096, Format::R16G16B16A16_FLOAT, TextureUsage::ShaderResource };
	RGTextureHandle cubeMap = graph.ImportTexture(m_cubemapTexture, cubeMapDesc, RGResourceState::ShaderResource);

	RGTextureDesc irradiacneMapDesc = { 32, 32, Format::R16G16B16A16_FLOAT, TextureUsage::ShaderResource };
	RGTextureHandle irradiacneMap = graph.ImportTexture(m_irradianceMapTexture, irradiacneMapDesc, RGResourceState::ShaderResource);

	RGTextureDesc prefilteredEnvMapDesc = { 512, 512, Format::R16G16B16A16_FLOAT, TextureUsage::ShaderResource };
	RGTextureHandle prefilteredEnvMap = graph.ImportTexture(m_prefilteredEnvMapTexture, prefilteredEnvMapDesc, RGResourceState::ShaderResource);

	RGTextureDesc brdfLUTDesc = { 512, 512, Format::R16G16B16A16_FLOAT, TextureUsage::ShaderResource };
	RGTextureHandle brdfLUT = graph.ImportTexture(m_brdfLUTTexture, brdfLUTDesc, RGResourceState::ShaderResource);

	RGTextureDesc shadowMapDesc = { 4096, 4096, Format::R32_TYPELESS, TextureUsage::DepthStencil };
	RGTextureHandle shadowMap = graph.ImportTexture(m_shadowMapTexture, shadowMapDesc, RGResourceState::DepthWrite);

	std::vector<RGTextureHandle> pointShadowMaps;
	RGTextureDesc pointShadowMapDesc = { 1024, 1024, Format::R32_TYPELESS, TextureUsage::DepthStencil };
	const int importedPointLightCount = std::min(MAX_POINT_LIGHTS, frameData.PointLightCount);
	for (int i = 0; i < MAX_POINT_LIGHTS; i++)
	{
		pointShadowMaps.push_back(graph.ImportTexture(m_pointShadowMapTextures[i], pointShadowMapDesc, RGResourceState::DepthWrite));
	}

	RGTextureDesc ssaoTextureDesc = { m_viewportWidth / 2, m_viewportHeight / 2, Format::R8_UNORM, TextureUsage::RenderTarget };
	RGTextureHandle ssaoTexture = graph.ImportTexture(m_ssaoTexture, ssaoTextureDesc, RGResourceState::RenderTarget);

	RGTextureDesc ssaoNoiseTextureDesc = { 4, 4, Format::R32G32B32A32_FLOAT, TextureUsage::ShaderResource };
	RGTextureHandle ssaoNoiseTexture = graph.ImportTexture(m_ssaoNoiseTexture, ssaoNoiseTextureDesc, RGResourceState::ShaderResource);

	RGTextureDesc ssaoTempTextureDesc = { m_viewportWidth / 2, m_viewportHeight / 2, Format::R8_UNORM, TextureUsage::RenderTarget };
	RGTextureHandle ssaoTempTexture = graph.ImportTexture(m_ssaoTempTexture, ssaoTempTextureDesc, RGResourceState::RenderTarget);

	RGTextureDesc gtaoTextureDesc = { m_viewportWidth / 2, m_viewportHeight / 2, Format::R8G8B8A8_UNORM, TextureUsage::UnorderedAccess };
	RGTextureHandle gtaoTexture = graph.ImportTexture(m_gtaoTexture, gtaoTextureDesc, RGResourceState::UnorderedAccess);

	RGTextureDesc gtaoTempTextureDesc = { m_viewportWidth / 2, m_viewportHeight / 2, Format::R8G8B8A8_UNORM, TextureUsage::UnorderedAccess };
	RGTextureHandle gtaoTempTexture = graph.ImportTexture(m_gtaoTempTexture, gtaoTempTextureDesc, RGResourceState::UnorderedAccess);

	RGTextureDesc sceneColorTextureDesc = { m_viewportWidth, m_viewportHeight, Format::R16G16B16A16_FLOAT, TextureUsage::RenderTarget };
	RGTextureHandle sceneColorTexture = graph.ImportTexture(m_sceneColorTexture, sceneColorTextureDesc, RGResourceState::RenderTarget);

	RGTextureDesc sceneColorLDRTextureDesc = { m_viewportWidth, m_viewportHeight, Format::R8G8B8A8_UNORM_SRGB, TextureUsage::RenderTarget };
	RGTextureHandle sceneColorLDRTexture = graph.ImportTexture(m_sceneColorLDRTexture, sceneColorLDRTextureDesc, RGResourceState::RenderTarget);

	RGBufferDesc shadowArgBufferDesc = { MAX_SHADOW_DRAWS * sizeof(ShadowIndirectCommand), sizeof(ShadowIndirectCommand), BufferUsage::IndirectArgument};
	RGBufferHandle shadowArgBuffer = graph.ImportBuffer(m_shadowArgBuffer, shadowArgBufferDesc, RGResourceState::IndirectArgument);


	PerFrameCB perFrame;
	XMMATRIX vp = XMLoadFloat4x4(&frameData.ViewMatrix) * XMLoadFloat4x4(&frameData.ProjMatrix);
	XMStoreFloat4x4(&perFrame.ViewProj, XMMatrixTranspose(vp));
	XMMATRIX inverseVP = XMMatrixInverse(nullptr, vp);
	XMStoreFloat4x4(&perFrame.InvViewProj, inverseVP);
	perFrame.CameraPos = frameData.CameraPos;
	XMStoreFloat2(&perFrame.ScreenSize, { static_cast<float>(m_viewportWidth), static_cast<float>(m_viewportHeight) });


	LightCB light;
	light.Direction = frameData.DirectionalLightDir;
	light.Color = frameData.DirectionalLightColor;
	light.Intensity = frameData.DirectionalLightIntensity;
	light.Ambient = frameData.Ambient;

	light.PointLightCount = importedPointLightCount;
	for (int i = 0; i < importedPointLightCount; i++)
	{
		auto pointLight = frameData.PointLights[i];
		light.PointLights[i] = { pointLight.Position, pointLight.Radius, pointLight.Color, pointLight.Intensity };
	}

	XMVECTOR targetPos = XMVectorSet(0, 0, 0, 0);
	XMVECTOR lightPos = XMVectorSubtract(targetPos, XMVectorScale(XMLoadFloat3(&frameData.DirectionalLightDir), 50.0f));
	XMMATRIX lightView = XMMatrixLookAtLH(lightPos, targetPos, GetStableUpVector(XMVectorSubtract(targetPos, lightPos)));
	XMMATRIX lightProj = XMMatrixOrthographicLH(100.0f, 100.0f, 1.0f, 300.0f);
	XMMATRIX lightVP = lightView * lightProj;



	PointShadowCB pointShadow;
	const XMFLOAT3 targets[6] = { {1,0,0},{-1,0,0},{0,1,0},{0,-1,0},{0,0,1},{0,0,-1} };
	const XMFLOAT3 ups[6] = { {0,1,0},{0,1,0},{0,0,-1},{0,0,1},{0,1,0},{0,1,0} };
	for (int lightIdx = 0; lightIdx < importedPointLightCount; lightIdx++)
	{
		pointShadow.pointShadowData[lightIdx].LightPos = frameData.PointLights[lightIdx].Position;
		pointShadow.pointShadowData[lightIdx].LightRadius = frameData.PointLights[lightIdx].Radius;

		XMMATRIX proj = XMMatrixPerspectiveFovLH(XM_PIDIV2, 1.0f, 0.1f, frameData.PointLights[lightIdx].Radius);
		for (int f = 0; f < 6; f++)
		{
			XMVECTOR eye = XMLoadFloat3(&frameData.PointLights[lightIdx].Position);
			XMVECTOR target = eye + XMLoadFloat3(&targets[f]);
			XMMATRIX view = XMMatrixLookAtLH(eye, target, XMLoadFloat3(&ups[f]));
			XMStoreFloat4x4(&pointShadow.pointShadowData[lightIdx].FaceVP[f], XMMatrixTranspose(view * proj));
		}
	}

	std::vector<PointLightData> fcPointLights;
	fcPointLights.reserve(importedPointLightCount);
	for (int i = 0; i < importedPointLightCount; i++)
	{
		fcPointLights.push_back(frameData.PointLights[i]);
	}

	FrameContext fc = {
		backBuffer, gbufferAlbedo, gbufferNormal, gbufferMR, gbufferEmissive,
		depthTexture, cubeMap,
		irradiacneMap, prefilteredEnvMap, brdfLUT, shadowMap, pointShadowMaps,
		importedPointLightCount,
		std::move(fcPointLights),
		ssaoTexture, ssaoNoiseTexture, ssaoTempTexture, gtaoTexture, gtaoTempTexture,
		sceneColorTexture, sceneColorLDRTexture,

		shadowArgBuffer,

		{}, {}, {}, {}, {}, {}
	};


	SSAOCB ssaoCB;
	XMStoreFloat4x4(&ssaoCB.ViewMatrix, XMMatrixTranspose(XMLoadFloat4x4(&frameData.ViewMatrix)));
	XMStoreFloat4x4(&ssaoCB.ProjMatrix, XMMatrixTranspose(XMLoadFloat4x4(&frameData.ProjMatrix)));
	XMStoreFloat4x4(&ssaoCB.InvProjMatrix, XMMatrixTranspose(XMMatrixInverse(nullptr, XMLoadFloat4x4(&frameData.ProjMatrix))));
	ssaoCB.SampleRadius = 0.7f;
	ssaoCB.Bias = 0.04f;
	ssaoCB.Power = 2.0f;
	ssaoCB.KernelSize = 32;
	ssaoCB.NoiseScale = XMFLOAT2(
		m_viewportWidth / 4.0f,
		m_viewportHeight / 4.0f
	);

	for (int i = 0; i < 32; i++)
	{
		ssaoCB.Samples[i] = hemisphereSamples[i];
	}


	BilateralBlurCB blurCB;
	blurCB.DepthSigma = 80;
	blurCB.NormalSigma = 12;
	blurCB.TexelSize = { 2.0f / m_viewportWidth, 2.0f / m_viewportHeight };


	GTAOCB gtaoCB;
	XMStoreFloat4x4(&gtaoCB.View, XMMatrixTranspose(XMLoadFloat4x4(&frameData.ViewMatrix)));
	XMStoreFloat4x4(&gtaoCB.InvProj, XMMatrixTranspose(XMMatrixInverse(nullptr, XMLoadFloat4x4(&frameData.ProjMatrix))));
	XMStoreFloat2(&gtaoCB.InvRes, { 1.0f / m_viewportWidth, 1.0f / m_viewportHeight });
	gtaoCB.Radius = 0.6f;
	gtaoCB.FalloffStart = gtaoCB.Radius * 0.35f;
	gtaoCB.FalloffEnd = gtaoCB.Radius;
	gtaoCB.NumSlices = 4;
	gtaoCB.NumSteps = 6;
	gtaoCB.FrameIndex = 0;


	float cascadeSplits[CASCADE_COUNT + 1];

	BuildCascadeShadowMatrices(
		inverseVP,
		XMLoadFloat3(&frameData.DirectionalLightDir),
		renderScene.sceneAABBMin, renderScene.sceneAABBMax,
		0.1f, 100.0f,
		cascadeSplits,
		fc.LightViewProj);

	ShadowCB shadow;
	for (int c = 0; c < CASCADE_COUNT; c++)
	{
		XMStoreFloat4x4(&shadow.LightViewProj[c], XMMatrixTranspose(fc.LightViewProj[c]));
		shadow.cascadeSplits[c] = cascadeSplits[c + 1];
	}

	fc.perFrameCB = ctx.UpdateConstantBuffer(&perFrame, sizeof(PerFrameCB));
	fc.lightCB = ctx.UpdateConstantBuffer(&light, sizeof(LightCB));
	fc.shadowCB = ctx.UpdateConstantBuffer(&shadow, sizeof(ShadowCB));
	fc.pointShadowCB = ctx.UpdateConstantBuffer(&pointShadow, sizeof(PointShadowCB));
	fc.ssaoCB = ctx.UpdateConstantBuffer(&ssaoCB, sizeof(SSAOCB));
	fc.bilateralBlurCB = ctx.UpdateConstantBuffer(&blurCB, sizeof(BilateralBlurCB));
	fc.gtaoCB = ctx.UpdateConstantBuffer(&gtaoCB, sizeof(GTAOCB));

	return fc;
}

void Renderer::UpdateIndirectArgBuffers(GraphicsDevice* device, const RenderScene& scene)
{
	std::vector<ShadowIndirectCommand> cmds;
	cmds.reserve(scene.renderObjects.size());
	for (const auto& obj : scene.renderObjects)
	{
		if (m_assetManager->Materials().Get(obj.material)->alphaMode != AlphaMode::Opaque)
			continue;

		GPUIndexBufferView ibv = device->GetIndexBufferView(
			obj.indexBuffer,
			obj.indexOffset * sizeof(uint32_t),
			obj.indexCount * sizeof(uint32_t),
			Format::R32_UINT);

		ShadowIndirectCommand cmd = {};
		cmd.objIdx = obj.transformIdx;
		cmd.vertexBufferIdx = obj.vertexBufferIndex;
		cmd.ibv.BufferLocation = ibv.gpuAddress;
		cmd.ibv.SizeInBytes = ibv.sizeInBytes;
		cmd.ibv.Format = DXGI_FORMAT_R32_UINT;
		cmd.drawArgs = { obj.indexCount, 1, 0, 0, 0 };

		cmds.push_back(cmd);
	}
	m_shadowDrawCount = (uint32_t)cmds.size();
	if (m_shadowDrawCount > 0)
		device->UpdateBuffer(m_shadowArgBuffer, cmds.data(), cmds.size() * sizeof(ShadowIndirectCommand));
}

void Renderer::BuildSceneGraph(GraphicsDevice* device, RenderGraph& graph, FrameContext& fc, const RenderScene& renderScene)
{
	AddDepthPrePass(device, graph, fc, renderScene);
	AddGBufferPass(device, graph, fc, renderScene);
	AddDirectionalShadowPass(device, graph, fc, renderScene);
	AddPointShadowPass(device, graph, fc, renderScene);

	//AddSSAOPass(device, graph, fc, renderScene);
	AddGTAOPass(device, graph, fc, renderScene);

	AddGTAOBilateralBlurPass(device, graph, fc, renderScene);
	AddPBRLightingPass(device, graph, fc, renderScene);
	AddSkyboxPass(device, graph, fc, renderScene);
}

void Renderer::BuildPresentGraph(GraphicsDevice * device, RenderGraph & graph, FrameContext & fc)
{
	AddPresentPass(device, graph, fc);
}

void Renderer::BuildDebugGraph(GraphicsDevice* device, RenderGraph& graph, FrameContext& fc)
{
	AddDebugLinePass(device, graph, fc);
}

void Renderer::InitDepthPrePass(GraphicsDevice* device)
{
	m_depthVS = ShaderCompiler::CompileFromFile(
		L"shaders_Depth_VS.hlsl",
		"main",
		"vs_6_6"
	);

	RootSignatureDesc depthPassRSDesc = {};
	depthPassRSDesc.cbvSrvUavHeapDirectlyIndexed = true;
	depthPassRSDesc.rootParamDescs.push_back({ RootParamType::RootCBV, RangeType::CBV, 0, 0, 1, ShaderVisibility::Vertex });
	depthPassRSDesc.rootParamDescs.push_back({ RootParamType::RootSRV, RangeType::SRV, 0, 0, 1, ShaderVisibility::Vertex });
	depthPassRSDesc.rootParamDescs.push_back({ RootParamType::RootConstants, RangeType::CBV, 1, 0, 2, ShaderVisibility::Vertex });

	m_depthPrePassPipelinDesc = {
		depthPassRSDesc,
		ShaderCompiler::GetBytecode(m_depthVS), {}, {},
		{ Format::UNKNOWN }, Format::D32_FLOAT,
		true, true, ComparisonFunc::Less,
		CullMode::Back 
	};


	m_depthPrePassPipeline = device->CreatePipeline(m_depthPrePassPipelinDesc);

	TextureDesc depthTextureDesc = { m_viewportWidth, m_viewportHeight, 1, 1, Format::D32_FLOAT, TextureUsage::DepthStencil, false };
	m_depthTexture = device->CreateDSTexture(depthTextureDesc);
}
void Renderer::InitGBufferPass(GraphicsDevice* device)
{
	TextureDesc albedoTextureDesc = { m_viewportWidth, m_viewportHeight, 1, 1, Format::R8G8B8A8_UNORM, TextureUsage::RenderTarget, false };
	m_gbufferAlbedo = device->CreateRTTexture(albedoTextureDesc);
	TextureDesc normalTextureDesc = { m_viewportWidth, m_viewportHeight, 1, 1, Format::R16G16B16A16_FLOAT, TextureUsage::RenderTarget, false };
	m_gbufferNormal = device->CreateRTTexture(normalTextureDesc);
	TextureDesc mrTextureDesc = { m_viewportWidth, m_viewportHeight, 1, 1, Format::R8G8B8A8_UNORM, TextureUsage::RenderTarget, false };
	m_gbufferMR = device->CreateRTTexture(mrTextureDesc);
	TextureDesc emissiveTextureDesc = { m_viewportWidth, m_viewportHeight, 1, 1, Format::R11G11B10_FLOAT, TextureUsage::RenderTarget, false };
	m_gbufferEmissive = device->CreateRTTexture(emissiveTextureDesc);

	RootSignatureDesc gBufferPassRSDesc = {};
	gBufferPassRSDesc.cbvSrvUavHeapDirectlyIndexed = true;
	gBufferPassRSDesc.rootParamDescs.push_back({ RootParamType::RootCBV,        RangeType::CBV, 0, 0, 1, ShaderVisibility::Vertex });
	gBufferPassRSDesc.rootParamDescs.push_back({ RootParamType::RootSRV,        RangeType::SRV, 1, 0, 1, ShaderVisibility::Vertex });
	gBufferPassRSDesc.rootParamDescs.push_back({ RootParamType::RootConstants,  RangeType::CBV, 2, 0, 3, ShaderVisibility::All });
	gBufferPassRSDesc.rootParamDescs.push_back({ RootParamType::RootSRV,        RangeType::SRV, 0, 0, 1, ShaderVisibility::Pixel });
	gBufferPassRSDesc.staticSamplers.push_back({ SamplerFilter::Trilinear,		SamplerAddressMode::Wrap, 0, ShaderVisibility::Pixel });

	m_gBufferVS = ShaderCompiler::CompileFromFile(
		L"shaders_GBuffer_VS.hlsl",
		"main",
		"vs_6_6"
	);

	m_gBufferOpaquePS = ShaderCompiler::CompileFromFile(
		L"shaders_GBufferOpaque_PS.hlsl",
		"main",
		"ps_6_6"
	);


	m_gBufferOpaquePassPipelineDesc = {
		gBufferPassRSDesc,
		ShaderCompiler::GetBytecode(m_gBufferVS), ShaderCompiler::GetBytecode(m_gBufferOpaquePS), {},
		{ Format::R8G8B8A8_UNORM, Format::R16G16B16A16_FLOAT, Format::R8G8B8A8_UNORM, Format::R11G11B10_FLOAT },
		Format::D32_FLOAT,
		true, false, ComparisonFunc::Equal,
		CullMode::Back,
		PrimitiveTopology::TriangleList,
	};

	m_gBufferOpaquePassPipeline = device->CreatePipeline(m_gBufferOpaquePassPipelineDesc);


	RootSignatureDesc gBufferAlphaPassRSDesc = {};
	gBufferAlphaPassRSDesc.cbvSrvUavHeapDirectlyIndexed = true;
	gBufferAlphaPassRSDesc.rootParamDescs.push_back({ RootParamType::RootCBV,        RangeType::CBV, 0, 0, 1, ShaderVisibility::Vertex });
	gBufferAlphaPassRSDesc.rootParamDescs.push_back({ RootParamType::RootSRV,        RangeType::SRV, 1, 0, 1, ShaderVisibility::Vertex });
	gBufferAlphaPassRSDesc.rootParamDescs.push_back({ RootParamType::RootConstants,  RangeType::CBV, 2, 0, 3, ShaderVisibility::All });
	gBufferAlphaPassRSDesc.rootParamDescs.push_back({ RootParamType::RootSRV,        RangeType::SRV, 0, 0, 1, ShaderVisibility::Pixel });
	gBufferAlphaPassRSDesc.staticSamplers.push_back({ SamplerFilter::Trilinear,		 SamplerAddressMode::Wrap, 0, ShaderVisibility::Pixel });

	m_gBufferAlphaPS = ShaderCompiler::CompileFromFile(
		L"shaders_GBufferAlpha_PS.hlsl",
		"main",
		"ps_6_6"
	);

	m_gBufferAlphaPassPipelineDesc = {
		gBufferAlphaPassRSDesc,
		ShaderCompiler::GetBytecode(m_gBufferVS), ShaderCompiler::GetBytecode(m_gBufferAlphaPS), {},
		{ Format::R8G8B8A8_UNORM, Format::R16G16B16A16_FLOAT, Format::R8G8B8A8_UNORM, Format::R11G11B10_FLOAT },
		Format::D32_FLOAT,
		true, true, ComparisonFunc::LessEqual,
		CullMode::None,
		PrimitiveTopology::TriangleList,
	};
	m_gBufferAlphaPassPipeline = device->CreatePipeline(m_gBufferAlphaPassPipelineDesc);
}
void Renderer::InitDirectionalShadowPass(GraphicsDevice* device)
{
	RootSignatureDesc shadowMapRSDesc = {};
	shadowMapRSDesc.allowIA = true;
	shadowMapRSDesc.cbvSrvUavHeapDirectlyIndexed = true;
	shadowMapRSDesc.rootParamDescs.push_back({ RootParamType::RootCBV, RangeType::CBV, 0, 0, 1, ShaderVisibility::Vertex });
	shadowMapRSDesc.rootParamDescs.push_back({ RootParamType::RootSRV, RangeType::SRV, 0, 0, 1, ShaderVisibility::Vertex });
	shadowMapRSDesc.rootParamDescs.push_back({ RootParamType::RootConstants, RangeType::CBV, 1, 0, 2, ShaderVisibility::Vertex });

	m_shadowMapVS = ShaderCompiler::CompileFromFile(
		L"shaders_shadowMap_VS.hlsl",
		"main",
		"vs_6_6"
	);

	m_shadowMapPipelineDesc = {};
	m_shadowMapPipelineDesc.rootSignatureDesc = shadowMapRSDesc;
	m_shadowMapPipelineDesc.vs = ShaderCompiler::GetBytecode(m_shadowMapVS);
	m_shadowMapPipelineDesc.ps = {};
	m_shadowMapPipelineDesc.vertexAttributes = {};
	m_shadowMapPipelineDesc.rtvFormats = { Format::UNKNOWN };
	m_shadowMapPipelineDesc.dsvFormat = Format::D32_FLOAT;
	m_shadowMapPipelineDesc.depthEnable = true;
	m_shadowMapPipelineDesc.depthWrite = true;
	m_shadowMapPipelineDesc.depthFunc = ComparisonFunc::LessEqual;
	m_shadowMapPipelineDesc.cullMode = CullMode::Back;

	m_shadowMapPipeline = device->CreatePipeline(m_shadowMapPipelineDesc);


	IndirectArgDesc shadowArgs[] = {
		{.type = IndirectArgType::Constant, .rootParamIdx = 2, .destOffset = 0, .num32Bit = 2},
		{.type = IndirectArgType::IndexBufferView },
		{.type = IndirectArgType::DrawIndexed },
	};
	m_shadowCmdSig = device->CreateCommandSignature(sizeof(ShadowIndirectCommand), shadowArgs, m_shadowMapPipeline);

	TextureDesc shadowMapDesc = { 4096, 4096, 1, 1, Format::D32_FLOAT, TextureUsage::DepthStencil, false };
	m_shadowMapTexture = device->CreateDSTexture(shadowMapDesc);

	BufferDesc argBufDesc = {};
	argBufDesc.size = MAX_SHADOW_DRAWS * sizeof(ShadowIndirectCommand);
	argBufDesc.usage = BufferUsage::IndirectArgument;
	argBufDesc.access = MemoryAccess::GpuOnly;
	m_shadowArgBuffer = device->CreateBuffer(argBufDesc);
}
void Renderer::InitPointShadowPass(GraphicsDevice* device)
{
	RootSignatureDesc pointShadowMapRSDesc = {};
	pointShadowMapRSDesc.allowIA = true;
	pointShadowMapRSDesc.cbvSrvUavHeapDirectlyIndexed = true;
	pointShadowMapRSDesc.rootParamDescs.push_back({ RootParamType::RootCBV, RangeType::CBV, 0, 0, 1, ShaderVisibility::Vertex });
	pointShadowMapRSDesc.rootParamDescs.push_back({ RootParamType::RootSRV, RangeType::SRV, 0, 0, 1, ShaderVisibility::Vertex });
	pointShadowMapRSDesc.rootParamDescs.push_back({ RootParamType::RootConstants, RangeType::CBV, 1, 0, 4, ShaderVisibility::Vertex });

	m_pointShadowMapVS = ShaderCompiler::CompileFromFile(
		L"shaders_pointShadowMap_VS.hlsl",
		"main",
		"vs_6_6"
	);

	m_pointShadowMapPipelineDesc = {};
	m_pointShadowMapPipelineDesc.rootSignatureDesc = pointShadowMapRSDesc;
	m_pointShadowMapPipelineDesc.vs = ShaderCompiler::GetBytecode(m_pointShadowMapVS);
	m_pointShadowMapPipelineDesc.ps = {};
	m_pointShadowMapPipelineDesc.vertexAttributes = {};
	m_pointShadowMapPipelineDesc.rtvFormats = { Format::UNKNOWN };
	m_pointShadowMapPipelineDesc.dsvFormat = Format::D32_FLOAT;
	m_pointShadowMapPipelineDesc.depthEnable = true;
	m_pointShadowMapPipelineDesc.depthWrite = true;
	m_pointShadowMapPipelineDesc.depthFunc = ComparisonFunc::LessEqual;
	m_pointShadowMapPipelineDesc.cullMode = CullMode::Back;

	m_pointShadowMapPipeline = device->CreatePipeline(m_pointShadowMapPipelineDesc);

	CubemapTextureDesc pointShadowMapDesc = { 1024, 1024, Format::D32_FLOAT, TextureUsage::DepthStencil };

	for (int i = 0; i < MAX_POINT_LIGHTS; i++)
	{
		m_pointShadowMapTextures.push_back(device->CreateDSCubemapTexture(pointShadowMapDesc));
	}
}
void Renderer::InitSSAOPass(GraphicsDevice* device)
{
	RootSignatureDesc ssaoPassRSDesc = {};
	ssaoPassRSDesc.allowIA = true;
	ssaoPassRSDesc.rootParamDescs.push_back({ RootParamType::RootCBV, RangeType::CBV, 0, 0, 1, ShaderVisibility::Pixel });
	ssaoPassRSDesc.rootParamDescs.push_back({ RootParamType::RootCBV, RangeType::CBV, 1, 0, 1, ShaderVisibility::Pixel });
	ssaoPassRSDesc.rootParamDescs.push_back({ RootParamType::DescriptorTable, RangeType::SRV, 0, 0, 1, ShaderVisibility::Pixel });
	ssaoPassRSDesc.rootParamDescs.push_back({ RootParamType::DescriptorTable, RangeType::SRV, 1, 0, 1, ShaderVisibility::Pixel });
	ssaoPassRSDesc.rootParamDescs.push_back({ RootParamType::DescriptorTable, RangeType::SRV, 2, 0, 1, ShaderVisibility::Pixel });
	ssaoPassRSDesc.staticSamplers.push_back({ SamplerFilter::Point, SamplerAddressMode::Clamp, 0, ShaderVisibility::Pixel });
	ssaoPassRSDesc.staticSamplers.push_back({ SamplerFilter::Bilinear, SamplerAddressMode::Wrap, 1, ShaderVisibility::Pixel });

	std::vector<VertexAttribute> vertexAttributes;
	vertexAttributes.push_back({ Semantic::POSITION, Format::R32G32B32_FLOAT, 0 });
	vertexAttributes.push_back({ Semantic::NORMAL,   Format::R32G32B32_FLOAT, 0 });
	vertexAttributes.push_back({ Semantic::TANGENT, Format::R32G32B32A32_FLOAT, 0 });
	vertexAttributes.push_back({ Semantic::TEXCOORD, Format::R32G32_FLOAT, 0 });

	m_ssaoVS = ShaderCompiler::CompileFromFile(
		L"shaders_FullScreen_VS.hlsl",
		"main",
		"vs_6_6"
	);
	m_ssaoPS = ShaderCompiler::CompileFromFile(
		L"shaders_SSAO_PS.hlsl",
		"main",
		"ps_6_6"
	);

	m_SSAOPipelineDesc = {
		ssaoPassRSDesc,
		ShaderCompiler::GetBytecode(m_ssaoVS), ShaderCompiler::GetBytecode(m_ssaoPS), vertexAttributes,
		{Format::R8_UNORM},
		Format::UNKNOWN,
		false, false, ComparisonFunc::Equal,
		CullMode::None };
	m_SSAOPipeline = device->CreatePipeline(m_SSAOPipelineDesc);

	TextureDesc ssaoTextureDesc = { m_viewportWidth / 2, m_viewportHeight / 2, 1, 1, Format::R8_UNORM, TextureUsage::RenderTarget, false };
	m_ssaoTexture = device->CreateRTTexture(ssaoTextureDesc);

	std::vector<XMFLOAT4> noise(16);
	std::uniform_real_distribution<float> dist(0.0f, 1.0f);
	std::default_random_engine rng;
	for (int i = 0; i < 16; i++)
	{
		XMFLOAT3 n(
			dist(rng) * 2.0f - 1.0f,
			dist(rng) * 2.0f - 1.0f,
			0.0f
		);

		XMVECTOR v = XMVector3Normalize(XMLoadFloat3(&n));
		XMStoreFloat4(&noise[i], v);
	}

	SubresourceData sub = { noise.data(), 4 * sizeof(XMFLOAT4), 4 * 4 * sizeof(XMFLOAT4) };
	TextureInitDesc ssaoNoiseTextureDesc =
	{
		{
			4, 4,
			1, 1,
			Format::R32G32B32A32_FLOAT,
			TextureUsage::ShaderResource,
			false
		},
		std::span<const SubresourceData>(&sub, 1)
	};

	m_ssaoNoiseTexture = device->CreateTexture(ssaoNoiseTextureDesc);
}
void Renderer::InitGTAOPass(GraphicsDevice* device)
{
	RootSignatureDesc GTAOPassRSDesc = {};
	GTAOPassRSDesc.allowIA = false;
	GTAOPassRSDesc.rootParamDescs.push_back({ RootParamType::RootCBV, RangeType::CBV, 0, 0, 1, ShaderVisibility::All });
	GTAOPassRSDesc.rootParamDescs.push_back({ RootParamType::DescriptorTable, RangeType::UAV, 0, 0, 1, ShaderVisibility::All });
	GTAOPassRSDesc.rootParamDescs.push_back({ RootParamType::DescriptorTable, RangeType::SRV, 0, 0, 1, ShaderVisibility::All });
	GTAOPassRSDesc.rootParamDescs.push_back({ RootParamType::DescriptorTable, RangeType::SRV, 1, 0, 1, ShaderVisibility::All });
	GTAOPassRSDesc.staticSamplers.push_back({ SamplerFilter::Point, SamplerAddressMode::Clamp, 0, ShaderVisibility::All });

	m_GTAOCS = ShaderCompiler::CompileFromFile(
		L"shaders_GTAO_CS.hlsl",
		"CSMain",
		"cs_6_6"
	);

	m_GTAOComputePipelineDesc = {};
	m_GTAOComputePipelineDesc.rootSignatureDesc = GTAOPassRSDesc;
	m_GTAOComputePipelineDesc.cs = ShaderCompiler::GetBytecode(m_GTAOCS);
	m_GTAOComputePipeline = device->CreateComputePipeline(m_GTAOComputePipelineDesc);

	TextureDesc gtaoTextureDesc = {};
	gtaoTextureDesc.width = m_viewportWidth / 2;
	gtaoTextureDesc.height = m_viewportHeight / 2;
	gtaoTextureDesc.format = Format::R8G8B8A8_UNORM;
	gtaoTextureDesc.usage = TextureUsage::UnorderedAccess;
	m_gtaoTexture = device->CreateUAVTexture(gtaoTextureDesc);
	m_gtaoTempTexture = device->CreateUAVTexture(gtaoTextureDesc);
}
void Renderer::InitSSAOBilateralBlurPass(GraphicsDevice* device)
{
	m_bilateralBlurPS_Vertical = ShaderCompiler::CompileFromFile(
		L"shaders_BilateralBlurVertical_PS.hlsl",
		"main",
		"ps_6_6"
	);

	m_bilateralBlurPS_Horizontal = ShaderCompiler::CompileFromFile(
		L"shaders_BilateralBlurHorizontal_PS.hlsl",
		"main",
		"ps_6_6"
	);

	std::vector<VertexAttribute> vertexAttributes;
	vertexAttributes.push_back({ Semantic::POSITION, Format::R32G32B32_FLOAT, 0 });
	vertexAttributes.push_back({ Semantic::NORMAL,   Format::R32G32B32_FLOAT, 0 });
	vertexAttributes.push_back({ Semantic::TANGENT, Format::R32G32B32A32_FLOAT, 0 });
	vertexAttributes.push_back({ Semantic::TEXCOORD, Format::R32G32_FLOAT, 0 });

	RootSignatureDesc bilateralBluPassRSDesc = {};
	bilateralBluPassRSDesc.allowIA = true;
	bilateralBluPassRSDesc.rootParamDescs.push_back({ RootParamType::RootCBV, RangeType::CBV, 0, 0, 1, ShaderVisibility::Pixel });
	bilateralBluPassRSDesc.rootParamDescs.push_back({ RootParamType::DescriptorTable, RangeType::SRV, 0, 0, 1, ShaderVisibility::Pixel });
	bilateralBluPassRSDesc.rootParamDescs.push_back({ RootParamType::DescriptorTable, RangeType::SRV, 1, 0, 1, ShaderVisibility::Pixel });
	bilateralBluPassRSDesc.rootParamDescs.push_back({ RootParamType::DescriptorTable, RangeType::SRV, 2, 0, 1, ShaderVisibility::Pixel });
	bilateralBluPassRSDesc.staticSamplers.push_back({ SamplerFilter::Bilinear, SamplerAddressMode::Wrap, 0, ShaderVisibility::Pixel });

	m_bilateralBlurPipelineDesc = {
		bilateralBluPassRSDesc,
		ShaderCompiler::GetBytecode(m_fullscreenVS), ShaderCompiler::GetBytecode(m_bilateralBlurPS_Vertical), vertexAttributes,
		{Format::R8_UNORM}, Format::UNKNOWN,
		false, false, ComparisonFunc::Equal,
		CullMode::None };
	m_bilateralBlurPipeline_Vertical = device->CreatePipeline(m_bilateralBlurPipelineDesc);

	m_bilateralBlurPipelineDesc.ps = ShaderCompiler::GetBytecode(m_bilateralBlurPS_Horizontal);
	m_bilateralBlurPipeline_Horizontal = device->CreatePipeline(m_bilateralBlurPipelineDesc);

	TextureDesc ssaoTempTextureDesc = { m_viewportWidth / 2, m_viewportHeight / 2, 1, 1, Format::R8_UNORM, TextureUsage::RenderTarget, false };
	m_ssaoTempTexture = device->CreateRTTexture(ssaoTempTextureDesc);
}
void Renderer::InitGTAOBilateralBlurPass(GraphicsDevice* device)
{
	m_bilateralBlurCS = ShaderCompiler::CompileFromFile(
		L"shaders_BilateralBlur_CS.hlsl",
		"CSMain",
		"cs_6_6"
	);

	RootSignatureDesc bilateralBlurPassRSDesc = {};
	bilateralBlurPassRSDesc.allowIA = false;
	bilateralBlurPassRSDesc.rootParamDescs.push_back({ RootParamType::RootCBV, RangeType::CBV, 0, 0, 1, ShaderVisibility::All });
	bilateralBlurPassRSDesc.rootParamDescs.push_back({ RootParamType::DescriptorTable, RangeType::SRV, 0, 0, 1, ShaderVisibility::All });
	bilateralBlurPassRSDesc.rootParamDescs.push_back({ RootParamType::DescriptorTable, RangeType::SRV, 1, 0, 1, ShaderVisibility::All });
	bilateralBlurPassRSDesc.rootParamDescs.push_back({ RootParamType::DescriptorTable, RangeType::SRV, 2, 0, 1, ShaderVisibility::All });
	bilateralBlurPassRSDesc.rootParamDescs.push_back({ RootParamType::DescriptorTable, RangeType::UAV, 0, 0, 1, ShaderVisibility::All });
	bilateralBlurPassRSDesc.staticSamplers.push_back({ SamplerFilter::Bilinear, SamplerAddressMode::Wrap, 0, ShaderVisibility::All });

	m_bilateralBlurComputePipelineDesc = {};
	m_bilateralBlurComputePipelineDesc.rootSignatureDesc = bilateralBlurPassRSDesc;
	m_bilateralBlurComputePipelineDesc.cs = ShaderCompiler::GetBytecode(m_bilateralBlurCS);
	m_bilateralBlurComputePipeline = device->CreateComputePipeline(m_bilateralBlurComputePipelineDesc);
}
void Renderer::InitPBRLightingPass(GraphicsDevice* device)
{
	RootSignatureDesc PBRlightingPassRSDesc = {};
	PBRlightingPassRSDesc.rootParamDescs.push_back({ RootParamType::RootCBV, RangeType::CBV, 0, 0, 1, ShaderVisibility::Pixel });
	PBRlightingPassRSDesc.rootParamDescs.push_back({ RootParamType::RootCBV, RangeType::CBV, 1, 0, 1, ShaderVisibility::Pixel });
	PBRlightingPassRSDesc.rootParamDescs.push_back({ RootParamType::RootCBV, RangeType::CBV, 2, 0, 1, ShaderVisibility::Pixel });
	PBRlightingPassRSDesc.rootParamDescs.push_back({ RootParamType::DescriptorTable, RangeType::SRV, 0, 0, 1, ShaderVisibility::Pixel });
	PBRlightingPassRSDesc.rootParamDescs.push_back({ RootParamType::DescriptorTable, RangeType::SRV, 1, 0, 1, ShaderVisibility::Pixel });
	PBRlightingPassRSDesc.rootParamDescs.push_back({ RootParamType::DescriptorTable, RangeType::SRV, 2, 0, 1, ShaderVisibility::Pixel });
	PBRlightingPassRSDesc.rootParamDescs.push_back({ RootParamType::DescriptorTable, RangeType::SRV, 3, 0, 1, ShaderVisibility::Pixel });
	PBRlightingPassRSDesc.rootParamDescs.push_back({ RootParamType::DescriptorTable, RangeType::SRV, 4, 0, 1, ShaderVisibility::Pixel });
	PBRlightingPassRSDesc.rootParamDescs.push_back({ RootParamType::DescriptorTable, RangeType::SRV, 5, 0, 1, ShaderVisibility::Pixel });
	PBRlightingPassRSDesc.rootParamDescs.push_back({ RootParamType::DescriptorTable, RangeType::SRV, 6, 0, 1, ShaderVisibility::Pixel });
	PBRlightingPassRSDesc.rootParamDescs.push_back({ RootParamType::DescriptorTable, RangeType::SRV, 7, 0, 1, ShaderVisibility::Pixel });
	PBRlightingPassRSDesc.rootParamDescs.push_back({ RootParamType::DescriptorTable, RangeType::SRV, 8, 0, 1, ShaderVisibility::Pixel });
	PBRlightingPassRSDesc.rootParamDescs.push_back({ RootParamType::DescriptorTable, RangeType::SRV, 9, 0, 1, ShaderVisibility::Pixel });
	PBRlightingPassRSDesc.rootParamDescs.push_back({ RootParamType::DescriptorTable, RangeType::SRV, 10, 0, 1, ShaderVisibility::Pixel });
	PBRlightingPassRSDesc.rootParamDescs.push_back({ RootParamType::DescriptorTable, RangeType::SRV, 11, 0, MAX_POINT_LIGHTS, ShaderVisibility::Pixel });
	PBRlightingPassRSDesc.staticSamplers.push_back({ SamplerFilter::Point, SamplerAddressMode::Clamp, 0, ShaderVisibility::Pixel });
	PBRlightingPassRSDesc.staticSamplers.push_back({ SamplerFilter::Bilinear, SamplerAddressMode::Wrap, 1, ShaderVisibility::Pixel });
	PBRlightingPassRSDesc.staticSamplers.push_back({ SamplerFilter::Comparison, SamplerAddressMode::Border, 2, ShaderVisibility::Pixel });

	std::vector<VertexAttribute> vertexAttributes;
	vertexAttributes.push_back({ Semantic::POSITION, Format::R32G32B32_FLOAT, 0 });
	vertexAttributes.push_back({ Semantic::NORMAL,   Format::R32G32B32_FLOAT, 0 });
	vertexAttributes.push_back({ Semantic::TANGENT, Format::R32G32B32A32_FLOAT, 0 });
	vertexAttributes.push_back({ Semantic::TEXCOORD, Format::R32G32_FLOAT, 0 });

	m_PBRlightingPS = ShaderCompiler::CompileFromFile(
		L"shaders_PBRLighting_PS.hlsl",
		"main",
		"ps_6_6"
	);

	m_PBRlightingPassPipelineDesc = {
		PBRlightingPassRSDesc,
		ShaderCompiler::GetBytecode(m_fullscreenVS), ShaderCompiler::GetBytecode(m_PBRlightingPS), vertexAttributes,
		{ Format::R16G16B16A16_FLOAT },
		Format::UNKNOWN,
		false, false, ComparisonFunc::Equal,
		CullMode::None };
	m_PBRlightingPassPipeline = device->CreatePipeline(m_PBRlightingPassPipelineDesc);
}
void Renderer::InitSkyboxPass(GraphicsDevice* device)
{
	RootSignatureDesc skyboxPassRSDesc = {};
	skyboxPassRSDesc.rootParamDescs.push_back({ RootParamType::RootCBV, RangeType::CBV, 0, 0, 1, ShaderVisibility::Pixel });
	skyboxPassRSDesc.rootParamDescs.push_back({ RootParamType::DescriptorTable, RangeType::SRV, 0, 0, 1, ShaderVisibility::Pixel });
	skyboxPassRSDesc.staticSamplers.push_back({ SamplerFilter::Bilinear, SamplerAddressMode::Clamp, 0, ShaderVisibility::Pixel });

	m_skyboxPS = ShaderCompiler::CompileFromFile(
		L"shaders_Skybox_PS.hlsl", "main", "ps_6_6"
	);

	m_skyboxPipelineDesc = {};
	m_skyboxPipelineDesc.rootSignatureDesc = skyboxPassRSDesc;
	m_skyboxPipelineDesc.vs = ShaderCompiler::GetBytecode(m_fullscreenVS);
	m_skyboxPipelineDesc.ps = ShaderCompiler::GetBytecode(m_skyboxPS);
	m_skyboxPipelineDesc.rtvFormats = { Format::R16G16B16A16_FLOAT };
	m_skyboxPipelineDesc.dsvFormat = Format::D32_FLOAT;
	m_skyboxPipelineDesc.depthEnable = true;
	m_skyboxPipelineDesc.depthWrite = false;
	m_skyboxPipelineDesc.depthFunc = ComparisonFunc::LessEqual;
	m_skyboxPipelineDesc.cullMode = CullMode::None;
	m_skyboxPipeline = device->CreatePipeline(m_skyboxPipelineDesc);
}
void Renderer::InitPresentPass(GraphicsDevice* device)
{
	RootSignatureDesc presentPassRSDesc = {};
	presentPassRSDesc.rootParamDescs.push_back({ RootParamType::DescriptorTable, RangeType::SRV, 0, 0, 1, ShaderVisibility::Pixel });
	presentPassRSDesc.rootParamDescs.push_back({ RootParamType::DescriptorTable, RangeType::SRV, 1, 0, 1, ShaderVisibility::Pixel });
	presentPassRSDesc.rootParamDescs.push_back({ RootParamType::DescriptorTable, RangeType::SRV, 2, 0, 1, ShaderVisibility::Pixel });
	presentPassRSDesc.rootParamDescs.push_back({ RootParamType::DescriptorTable, RangeType::SRV, 3, 0, 1, ShaderVisibility::Pixel });
	presentPassRSDesc.rootParamDescs.push_back({ RootParamType::DescriptorTable, RangeType::SRV, 4, 0, 1, ShaderVisibility::Pixel });
	presentPassRSDesc.rootParamDescs.push_back({ RootParamType::DescriptorTable, RangeType::SRV, 5, 0, 1, ShaderVisibility::Pixel });
	presentPassRSDesc.rootParamDescs.push_back({ RootParamType::DescriptorTable, RangeType::SRV, 6, 0, 1, ShaderVisibility::Pixel });
	presentPassRSDesc.rootParamDescs.push_back({ RootParamType::RootConstants, RangeType::CBV, 0, 0, 1, ShaderVisibility::Pixel });
	presentPassRSDesc.staticSamplers.push_back({ SamplerFilter::Bilinear, SamplerAddressMode::Clamp, 0, ShaderVisibility::Pixel });

	m_presentPS = ShaderCompiler::CompileFromFile(
		L"shaders_Present_PS.hlsl", "main", "ps_6_6"
	);

	m_presentPipelineDesc = {};
	m_presentPipelineDesc.rootSignatureDesc = presentPassRSDesc;
	m_presentPipelineDesc.vs = ShaderCompiler::GetBytecode(m_fullscreenVS);
	m_presentPipelineDesc.ps = ShaderCompiler::GetBytecode(m_presentPS);
	m_presentPipelineDesc.rtvFormats = { Format::R8G8B8A8_UNORM_SRGB };
	m_presentPipelineDesc.dsvFormat = Format::D32_FLOAT;
	m_presentPipelineDesc.depthEnable = false;
	m_presentPipelineDesc.depthWrite = false;
	m_presentPipelineDesc.depthFunc = ComparisonFunc::LessEqual;
	m_presentPipelineDesc.cullMode = CullMode::None;
	m_presentPipeline = device->CreatePipeline(m_presentPipelineDesc);

	TextureDesc sceneColorTextureDesc = { m_viewportWidth, m_viewportHeight, 1, 1, Format::R16G16B16A16_FLOAT, TextureUsage::RenderTarget, false };
	m_sceneColorTexture = device->CreateRTTexture(sceneColorTextureDesc);

	TextureDesc sceneColorLDRTextureDesc = {
		m_viewportWidth, m_viewportHeight, 1, 1,
		Format::R8G8B8A8_UNORM_SRGB,   
		TextureUsage::RenderTarget,
		false
	};
	m_sceneColorLDRTexture = device->CreateRTTexture(sceneColorLDRTextureDesc);
}
void Renderer::InitDebugLinePass(GraphicsDevice* device)
{
	RootSignatureDesc debugLineRSDesc{};
	debugLineRSDesc.allowIA = true;
	debugLineRSDesc.rootParamDescs.push_back({ RootParamType::RootCBV, RangeType::CBV, 0, 0, 1, ShaderVisibility::Vertex });

	m_debugLineVS = ShaderCompiler::CompileFromFile(L"shaders_DebugLine_VS.hlsl", "main", "vs_6_6");
	m_debugLinePS = ShaderCompiler::CompileFromFile(L"shaders_DebugLine_PS.hlsl", "main", "ps_6_6");

	PipelineDesc debugLinePSODesc{};
	debugLinePSODesc.rootSignatureDesc = debugLineRSDesc;
	debugLinePSODesc.vs = ShaderCompiler::GetBytecode(m_debugLineVS);
	debugLinePSODesc.ps = ShaderCompiler::GetBytecode(m_debugLinePS);
	debugLinePSODesc.vertexAttributes = {
		{ Semantic::POSITION, Format::R32G32B32_FLOAT, 0 },
		{ Semantic::COLOR,    Format::R32G32B32A32_FLOAT, 0 },
	};
	debugLinePSODesc.rtvFormats = { Format::R8G8B8A8_UNORM_SRGB };
	debugLinePSODesc.dsvFormat = Format::D32_FLOAT;
	debugLinePSODesc.depthEnable = true;
	debugLinePSODesc.depthWrite = false;
	debugLinePSODesc.depthFunc = ComparisonFunc::LessEqual;
	debugLinePSODesc.cullMode = CullMode::None;
	debugLinePSODesc.topology = PrimitiveTopology::LineList;

	m_debugLinePipeline = device->CreatePipeline(debugLinePSODesc);
}


void Renderer::AddDepthPrePass(GraphicsDevice* device, RenderGraph& graph, FrameContext& fc, const RenderScene& scene)
{
	graph.AddPass(
		"DepthPrePass",
		[&](RGBuilder& builder) {
			builder.Write(fc.depthTexture, RGResourceState::DepthWrite);
		},
		[this, &fc, device, &scene](CommandContext& passCtx) {
			passCtx.BeginTimestamp(PassID::DepthPrePass);
			{
				passCtx.ClearDepthStencil(m_depthTexture, 1.0f);
				passCtx.SetRenderTarget(0, {}, m_depthTexture);
				passCtx.SetPipeline(m_depthPrePassPipeline);
				passCtx.SetViewport(0, 0, (float)m_viewportWidth, (float)m_viewportHeight);
				passCtx.SetScissorRect(0, 0, (LONG)m_viewportWidth, (LONG)m_viewportHeight);

				passCtx.BindConstantBuffer(0, fc.perFrameCB);
				passCtx.BindRootSRV(1, m_transformBuffer);

				for (const auto& obj : scene.renderObjects)
				{
					if (m_assetManager->Materials().Get(obj.material)->alphaMode == AlphaMode::Opaque)
					{
						struct DrawConstants { uint32_t objIdx; uint32_t vbIdx; };
						DrawConstants dc{ obj.transformIdx, obj.vertexBufferIndex };
						passCtx.SetRootConstants(2, &dc, 2);

						passCtx.SetIndexBuffer(obj.indexBuffer);
						passCtx.DrawIndexed(obj.indexCount, obj.indexOffset, 0);
					}
				}
			}
			passCtx.EndTimestamp(PassID::DepthPrePass);
		}
	);
}
void Renderer::AddGBufferPass(GraphicsDevice* device, RenderGraph& graph, FrameContext& fc, const RenderScene& scene)
{
	graph.AddPass(
		"GBufferOpaquePass",
		[&](RGBuilder& builder) {
			builder.Read(fc.depthTexture, RGResourceState::DepthRead);

			builder.Write(fc.gbufferAlbedo,   RGResourceState::RenderTarget);
			builder.Write(fc.gbufferNormal,   RGResourceState::RenderTarget);
			builder.Write(fc.gbufferMR,       RGResourceState::RenderTarget);
			builder.Write(fc.gbufferEmissive, RGResourceState::RenderTarget);
		},
		[this, &fc, device, &scene](CommandContext& passCtx) {
			passCtx.BeginTimestamp(PassID::GBufferPass);
			{
				GPUTextureHandle renderTargets[] = { m_gbufferAlbedo, m_gbufferNormal, m_gbufferMR, m_gbufferEmissive };
				passCtx.ClearRenderTargets(4, renderTargets, clearColor);
				passCtx.SetRenderTarget(4, renderTargets, m_depthTexture);

				passCtx.SetPipeline(m_gBufferOpaquePassPipeline);
				passCtx.SetViewport(0, 0, (float)m_viewportWidth, (float)m_viewportHeight);
				passCtx.SetScissorRect(0, 0, (LONG)m_viewportWidth, (LONG)m_viewportHeight);

				passCtx.BindConstantBuffer(0, fc.perFrameCB);
				passCtx.BindRootSRV(1, m_transformBuffer);
				passCtx.BindRootSRV(3, m_assetManager->Materials().GetGPUBuffer());

				for (const auto& obj : scene.renderObjects)
				{
					if (m_assetManager->Materials().Get(obj.material)->alphaMode == AlphaMode::Opaque)
					{
						struct DrawConstants { uint32_t objIdx; uint32_t matIdx; uint32_t vbIdx; };
						DrawConstants dc{ obj.transformIdx, obj.material.index, obj.vertexBufferIndex };
						passCtx.SetRootConstants(2, &dc, 3);

						passCtx.SetIndexBuffer(obj.indexBuffer);
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
			builder.Write(fc.depthTexture, RGResourceState::DepthWrite);
			builder.Write(fc.gbufferAlbedo, RGResourceState::RenderTarget);
			builder.Write(fc.gbufferNormal, RGResourceState::RenderTarget);
			builder.Write(fc.gbufferMR, RGResourceState::RenderTarget);
			builder.Write(fc.gbufferEmissive, RGResourceState::RenderTarget);
		},
		[this, &fc, device, &scene](CommandContext& passCtx) {
			passCtx.BeginTimestamp(PassID::GBufferAlphaPass);
			{
				GPUTextureHandle renderTargets[] = { m_gbufferAlbedo, m_gbufferNormal, m_gbufferMR, m_gbufferEmissive };
				passCtx.SetRenderTarget(4, renderTargets, m_depthTexture);
				passCtx.SetPipeline(m_gBufferAlphaPassPipeline);

				passCtx.BindConstantBuffer(0, fc.perFrameCB);
				passCtx.BindRootSRV(1, m_transformBuffer);
				passCtx.BindRootSRV(3, m_assetManager->Materials().GetGPUBuffer());
				for (const auto& obj : scene.renderObjects)
				{
					if (m_assetManager->Materials().Get(obj.material)->alphaMode == AlphaMode::Mask)
					{
						struct DrawConstants { uint32_t objIdx; uint32_t matIdx; uint32_t vbIdx; };
						DrawConstants dc{ obj.transformIdx, obj.material.index, obj.vertexBufferIndex };
						passCtx.SetRootConstants(2, &dc, 3);

						passCtx.SetIndexBuffer(obj.indexBuffer);
						passCtx.DrawIndexed(obj.indexCount, obj.indexOffset, 0);
					}
				}
			}
			passCtx.EndTimestamp(PassID::GBufferAlphaPass);
		}
	);
}
void Renderer::AddDirectionalShadowPass(GraphicsDevice* device, RenderGraph& graph, FrameContext& fc, const RenderScene& scene)
{
	graph.AddPass(
		"DirectionalShadowPass",
		[&](RGBuilder& builder) {
			builder.Read(fc.shadowArgBuffer, RGResourceState::IndirectArgument);

			builder.Write(fc.shadowMap, RGResourceState::DepthWrite);
		},
		[this, &fc, device, &scene](CommandContext& passCtx) {
			{
				passCtx.BeginTimestamp(PassID::DirectionalShadowPass);
				{
					passCtx.ClearDepthStencil(m_shadowMapTexture, 1.0f);
					passCtx.SetRenderTarget(0, {}, m_shadowMapTexture);
					passCtx.SetPipeline(m_shadowMapPipeline);

					constexpr int SHADOW_ATLAS_SIZE = 4096;
					constexpr int CASCADE_TILE_SIZE = 2048;
					int offsets[4][2] = { {0,0}, {2048,0}, {0,2048}, {2048,2048} };

					for (int c = 0; c < CASCADE_COUNT; c++)
					{
						passCtx.SetViewport(offsets[c][0], offsets[c][1], 2048, 2048, 0.0f, 1.0f);
						passCtx.SetScissorRect(offsets[c][0], offsets[c][1], offsets[c][0] + 2048, offsets[c][1] + 2048);

						XMFLOAT4X4 transposed;
						XMStoreFloat4x4(&transposed, XMMatrixTranspose(fc.LightViewProj[c]));
						auto cascadeCB = passCtx.UpdateConstantBuffer(&transposed, sizeof(XMFLOAT4X4));
						passCtx.BindConstantBuffer(0, cascadeCB);
						passCtx.BindRootSRV(1, m_transformBuffer);

						passCtx.ExecuteIndirect(m_shadowCmdSig, m_shadowDrawCount, m_shadowArgBuffer, 0);
					}
				}
				passCtx.EndTimestamp(PassID::DirectionalShadowPass);
			}
		}
	);
}
void Renderer::AddPointShadowPass(GraphicsDevice* device, RenderGraph& graph, FrameContext& fc, const RenderScene& scene)
{
	if (fc.pointLightCount <= 0)
		return;

	graph.AddPass(
		"PointShadowPass",
		[&](RGBuilder& builder) {
			for (int lightIdx = 0; lightIdx < fc.pointLightCount; lightIdx++)
			{
				builder.Write(fc.pointShadowMaps[lightIdx], RGResourceState::DepthWrite);
			}
		},
		[this, &fc, device, &scene](CommandContext& passCtx) {
			{
				passCtx.BeginTimestamp(PassID::PointShadowPass);
				{
					passCtx.SetPipeline(m_pointShadowMapPipeline);

					passCtx.BindConstantBuffer(0, fc.pointShadowCB);
					passCtx.BindRootSRV(1, m_transformBuffer);

					for (int lightIdx = 0; lightIdx < fc.pointLightCount; lightIdx++)
					{
						const auto& pointLight = fc.pointLights[lightIdx];

						std::vector<const RenderObject*> visibleObjects;
						visibleObjects.reserve(scene.renderObjects.size());

						for (const auto& obj : scene.renderObjects)
						{
							if (m_assetManager->Materials().Get(obj.material)->alphaMode != AlphaMode::Opaque)
								continue;
							if (!AABBIntersectsSphere(obj.aabbMin, obj.aabbMax, pointLight.Position, pointLight.Radius))
								continue;
							visibleObjects.push_back(&obj);
						}

						if (visibleObjects.empty())
							continue;

						uint32_t lastVB = UINT32_MAX;
						uint32_t lastIB = UINT32_MAX;

						for (int face = 0; face < 6; face++)
						{
							passCtx.ClearDepthStencil(m_pointShadowMapTextures[lightIdx], 1.0f, face);
							passCtx.SetRenderTarget(0, {}, m_pointShadowMapTextures[lightIdx], face);
							passCtx.SetViewport(0, 0, 1024, 1024);
							passCtx.SetScissorRect(0, 0, 1024, 1024);

							for (const RenderObject* objPtr : visibleObjects)
							{
								const auto& obj = *objPtr;

								PointShadowConstants shadowConstants;
								shadowConstants.lightIdx = lightIdx;
								shadowConstants.faceIdx = face;
								shadowConstants.transformIdx = obj.transformIdx;
								shadowConstants.vertexBufferIndex = obj.vertexBufferIndex;
								passCtx.SetRootConstants(2, &shadowConstants, 4);

								if (obj.indexBuffer.id != lastIB)
								{
									passCtx.SetIndexBuffer(obj.indexBuffer);
									lastIB = obj.indexBuffer.id;
								}
								passCtx.DrawIndexed(obj.indexCount, obj.indexOffset, 0);
							}
						}
					}
				}
				passCtx.EndTimestamp(PassID::PointShadowPass);
			}
		}
	);
}
void Renderer::AddSSAOPass(GraphicsDevice* device, RenderGraph& graph, FrameContext& fc, const RenderScene& scene)
{
	graph.AddPass(
		"SSAOPass",
		[&](RGBuilder& builder) {
			builder.Read(fc.depthTexture, RGResourceState::ShaderResource);
			builder.Read(fc.gbufferNormal, RGResourceState::ShaderResource);
			builder.Read(fc.ssaoNoiseTexture, RGResourceState::ShaderResource);

			builder.Write(fc.ssaoTexture, RGResourceState::RenderTarget);
		},
		[this, &fc, device](CommandContext& passCtx) {
			{
				passCtx.ClearRenderTarget(m_ssaoTexture, clearColor);
				passCtx.SetRenderTarget(1, &m_ssaoTexture, {});
				passCtx.SetPipeline(m_SSAOPipeline);
				passCtx.SetViewport(0, 0, (float)(m_viewportWidth / 2), (float)(m_viewportHeight / 2));
				passCtx.SetScissorRect(0, 0, (LONG)(m_viewportWidth / 2), (LONG)(m_viewportHeight / 2));

				passCtx.BindConstantBuffer(0, fc.perFrameCB);
				passCtx.BindConstantBuffer(1, fc.ssaoCB);

				passCtx.BindTexture(2, m_depthTexture);
				passCtx.BindTexture(3, m_gbufferNormal);
				passCtx.BindTexture(4, m_ssaoNoiseTexture);

				passCtx.Draw(3, 0);
			}
		}
	);
}
void Renderer::AddGTAOPass(GraphicsDevice* device, RenderGraph& graph, FrameContext& fc, const RenderScene& scene)
{
	graph.AddPass(
		"GTAOPass",
		[&](RGBuilder& builder) {
			builder.Read(fc.depthTexture, RGResourceState::ShaderResource);
			builder.Read(fc.gbufferNormal, RGResourceState::ShaderResource);

			builder.Write(fc.gtaoTexture, RGResourceState::UnorderedAccess);
		},
		[this, &fc, device](CommandContext& passCtx) {
			passCtx.BeginTimestamp(PassID::GTAOPass);
			{
				passCtx.SetComputePipeline(m_GTAOComputePipeline);
				passCtx.BindComputeConstantBuffer(0, fc.gtaoCB);
				passCtx.SetComputeDescriptorTable(1, device->GetUAVHandle(m_gtaoTexture));
				passCtx.SetComputeDescriptorTable(2, device->GetSRVHandle(m_depthTexture));
				passCtx.SetComputeDescriptorTable(3, device->GetSRVHandle(m_gbufferNormal));
				passCtx.Dispatch((m_viewportWidth / 2 + 7) / 8, (m_viewportHeight / 2 + 7) / 8, 1);
			}
			passCtx.EndTimestamp(PassID::GTAOPass);
		}
	);
}
void Renderer::AddSSAOBilateralBlurPass(GraphicsDevice* device, RenderGraph& graph, FrameContext& fc, const RenderScene& scene)
{
	graph.AddPass(
		"BilateralBlurPass_Horizontal",
		[&](RGBuilder& builder) {
			builder.Read(fc.ssaoTexture, RGResourceState::ShaderResource);
			builder.Read(fc.depthTexture, RGResourceState::ShaderResource);
			builder.Read(fc.gbufferNormal, RGResourceState::ShaderResource);

			builder.Write(fc.ssaoTempTexture, RGResourceState::RenderTarget);
		},
		[this, &fc, device](CommandContext& passCtx) {
			{
				passCtx.ClearRenderTarget(m_ssaoTempTexture, clearColor);
				passCtx.SetRenderTarget(1, &m_ssaoTempTexture, {});
				passCtx.SetPipeline(m_bilateralBlurPipeline_Horizontal);
				passCtx.SetViewport(0, 0, (float)(m_viewportWidth / 2), (float)(m_viewportHeight / 2));
				passCtx.SetScissorRect(0, 0, (LONG)(m_viewportWidth / 2), (LONG)(m_viewportHeight / 2));

				passCtx.BindConstantBuffer(0, fc.bilateralBlurCB);

				passCtx.BindTexture(1, m_ssaoTexture);
				passCtx.BindTexture(2, m_depthTexture);
				passCtx.BindTexture(3, m_gbufferNormal);

				passCtx.Draw(3, 0);
			}
		}
	);

	graph.AddPass(
		"BilateralBlurPass_Vertical",
		[&](RGBuilder& builder) {
			builder.Read(fc.ssaoTempTexture, RGResourceState::ShaderResource);
			builder.Read(fc.depthTexture, RGResourceState::ShaderResource);
			builder.Read(fc.gbufferNormal, RGResourceState::ShaderResource);

			builder.Write(fc.ssaoTexture, RGResourceState::RenderTarget);
		},
		[this, &fc, device](CommandContext& passCtx) {
			{
				passCtx.ClearRenderTarget(m_ssaoTexture, clearColor);
				passCtx.SetRenderTarget(1, &m_ssaoTexture, {});
				passCtx.SetPipeline(m_bilateralBlurPipeline_Vertical);
				passCtx.SetViewport(0, 0, (float)(m_viewportWidth / 2), (float)(m_viewportHeight / 2));
				passCtx.SetScissorRect(0, 0, (LONG)(m_viewportWidth / 2), (LONG)(m_viewportHeight / 2));

				passCtx.BindConstantBuffer(0, fc.bilateralBlurCB);

				passCtx.BindTexture(1, m_ssaoTempTexture);
				passCtx.BindTexture(2, m_depthTexture);
				passCtx.BindTexture(3, m_gbufferNormal);

				passCtx.Draw(3, 0);
			}
		}
	);
}
void Renderer::AddGTAOBilateralBlurPass(GraphicsDevice* device, RenderGraph& graph, FrameContext& fc, const RenderScene& scene)
{
	graph.AddPass(
		"BilateralBlurPass_Horizontal",
		[&](RGBuilder& builder) {
			builder.Read(fc.gtaoTexture, RGResourceState::ShaderResource);
			builder.Read(fc.depthTexture, RGResourceState::ShaderResource);
			builder.Read(fc.gbufferNormal, RGResourceState::ShaderResource);

			builder.Write(fc.gtaoTempTexture, RGResourceState::UnorderedAccess);
		},
		[this, &fc, device](CommandContext& passCtx) {
			{
				passCtx.SetComputePipeline(m_bilateralBlurComputePipeline);

				GTAOBilateralBlurCB cb{ {1.0f / (m_viewportWidth / 2), 1.0f / (m_viewportHeight / 2)}, {1,0}, 0.1f, 32.0f, 6 };
				fc.gtaoBilateralBlurCB = passCtx.UpdateConstantBuffer(&cb, sizeof(GTAOBilateralBlurCB));
				passCtx.BindComputeConstantBuffer(0, fc.gtaoBilateralBlurCB);

				passCtx.SetComputeDescriptorTable(1, device->GetSRVHandle(m_gtaoTexture));
				passCtx.SetComputeDescriptorTable(2, device->GetSRVHandle(m_depthTexture));
				passCtx.SetComputeDescriptorTable(3, device->GetSRVHandle(m_gbufferNormal));
				passCtx.SetComputeDescriptorTable(4, device->GetUAVHandle(m_gtaoTempTexture));
				passCtx.Dispatch((m_viewportWidth / 2 + 7) / 8, (m_viewportHeight / 2 + 7) / 8, 1);
			}
		}
	);

	graph.AddPass(
		"BilateralBlurPass_Vertical",
		[&](RGBuilder& builder) {
			builder.Read(fc.gtaoTempTexture, RGResourceState::ShaderResource);
			builder.Read(fc.depthTexture, RGResourceState::ShaderResource);
			builder.Read(fc.gbufferNormal, RGResourceState::ShaderResource);

			builder.Write(fc.gtaoTexture, RGResourceState::UnorderedAccess);
		},
		[this, &fc, device](CommandContext& passCtx) {
			{
				passCtx.SetComputePipeline(m_bilateralBlurComputePipeline);

				GTAOBilateralBlurCB cb{ {1.0f / (m_viewportWidth / 2), 1.0f / (m_viewportHeight / 2)}, {0,1}, 0.1f, 32.0f, 6 };
				fc.gtaoBilateralBlurCB = passCtx.UpdateConstantBuffer(&cb, sizeof(GTAOBilateralBlurCB));
				passCtx.BindComputeConstantBuffer(0, fc.gtaoBilateralBlurCB);

				passCtx.SetComputeDescriptorTable(1, device->GetSRVHandle(m_gtaoTempTexture));
				passCtx.SetComputeDescriptorTable(2, device->GetSRVHandle(m_depthTexture));
				passCtx.SetComputeDescriptorTable(3, device->GetSRVHandle(m_gbufferNormal));
				passCtx.SetComputeDescriptorTable(4, device->GetUAVHandle(m_gtaoTexture));
				passCtx.Dispatch((m_viewportWidth / 2 + 7) / 8, (m_viewportHeight / 2 + 7) / 8, 1);
			}
		}
	);
}
void Renderer::AddPBRLightingPass(GraphicsDevice* device, RenderGraph& graph, FrameContext& fc, const RenderScene& scene)
{
	graph.AddPass(
		"PBRLightingPass",
		[&](RGBuilder& builder) {
			builder.Read(fc.depthTexture, RGResourceState::ShaderResource);
			builder.Read(fc.gbufferAlbedo, RGResourceState::ShaderResource);
			builder.Read(fc.gbufferNormal, RGResourceState::ShaderResource);
			builder.Read(fc.gbufferMR, RGResourceState::ShaderResource);
			builder.Read(fc.gbufferEmissive, RGResourceState::ShaderResource);
			builder.Read(fc.irradianceMap, RGResourceState::ShaderResource);
			builder.Read(fc.prefilteredEnvMap, RGResourceState::ShaderResource);
			builder.Read(fc.brdfLutTexture, RGResourceState::ShaderResource);
			builder.Read(fc.shadowMap, RGResourceState::ShaderResource);
			builder.Read(fc.ssaoTexture, RGResourceState::ShaderResource);
			builder.Read(fc.gtaoTexture, RGResourceState::ShaderResource);

			for (auto& resource : fc.pointShadowMaps)
			{
				builder.Read(resource, RGResourceState::ShaderResource);
			}

			builder.Write(fc.scenecolor, RGResourceState::RenderTarget);
		},
		[this, &fc, device](CommandContext& passCtx) {
			passCtx.BeginTimestamp(PassID::PBRLightingPass);
			{
				passCtx.ClearRenderTarget(m_sceneColorTexture, clearColor);
				passCtx.SetRenderTarget(1, &m_sceneColorTexture, {});
				passCtx.SetPipeline(m_PBRlightingPassPipeline);
				passCtx.SetViewport(0, 0, (float)m_viewportWidth, (float)m_viewportHeight);
				passCtx.SetScissorRect(0, 0, (LONG)m_viewportWidth, (LONG)m_viewportHeight);

				passCtx.BindConstantBuffer(0, fc.perFrameCB);
				passCtx.BindConstantBuffer(1, fc.lightCB);
				passCtx.BindConstantBuffer(2, fc.shadowCB);

				passCtx.BindTexture(3, m_depthTexture);
				passCtx.BindTexture(4, m_gbufferAlbedo);
				passCtx.BindTexture(5, m_gbufferNormal);
				passCtx.BindTexture(6, m_gbufferMR);
				passCtx.BindTexture(7, m_gbufferEmissive);
				passCtx.BindTexture(8, m_irradianceMapTexture);
				passCtx.BindTexture(9, m_prefilteredEnvMapTexture);
				passCtx.BindTexture(10, m_brdfLUTTexture);
				passCtx.BindTexture(11, m_shadowMapTexture);
				passCtx.BindTexture(12, m_ssaoTexture);
				passCtx.BindTexture(13, m_gtaoTexture);
				passCtx.BindTexture(14, m_pointShadowMapTextures[0]);

				passCtx.Draw(3, 0);
			}
			passCtx.EndTimestamp(PassID::PBRLightingPass);
		}
	);
}
void Renderer::AddSkyboxPass(GraphicsDevice* device, RenderGraph& graph, FrameContext& fc, const RenderScene& scene)
{
	graph.AddPass(
		"SkyboxPass",
		[&](RGBuilder& builder) {
			builder.Read(fc.depthTexture, RGResourceState::DepthRead);
			builder.Read(fc.skyboxTexture, RGResourceState::ShaderResource);
			builder.Write(fc.scenecolor, RGResourceState::RenderTarget);
		},
		[this, &fc, device](CommandContext& passCtx) {
			passCtx.BeginTimestamp(PassID::SkyboxPass);

			passCtx.SetRenderTarget(1, &m_sceneColorTexture, m_depthTexture);
			passCtx.SetPipeline(m_skyboxPipeline);
			passCtx.SetViewport(0, 0, (float)m_viewportWidth, (float)m_viewportHeight);
			passCtx.SetScissorRect(0, 0, (LONG)m_viewportWidth, (LONG)m_viewportHeight);

			passCtx.BindConstantBuffer(0, fc.perFrameCB);

			passCtx.BindTexture(1, m_cubemapTexture);

			passCtx.Draw(3, 0);

			passCtx.EndTimestamp(PassID::SkyboxPass);
		}
	);
}
void Renderer::AddPresentPass(GraphicsDevice* device, RenderGraph& graph, FrameContext& fc)
{
	graph.AddPass(
		"PresentPass",
		[&](RGBuilder& builder) {
			builder.Read(fc.depthTexture, RGResourceState::ShaderResource);
			builder.Read(fc.gbufferAlbedo, RGResourceState::ShaderResource);
			builder.Read(fc.gbufferNormal, RGResourceState::ShaderResource);
			builder.Read(fc.gbufferMR, RGResourceState::ShaderResource);
			builder.Read(fc.gbufferEmissive, RGResourceState::ShaderResource);
			builder.Read(fc.shadowMap, RGResourceState::ShaderResource);
			builder.Read(fc.gtaoTexture, RGResourceState::ShaderResource);
			builder.Read(fc.scenecolor, RGResourceState::ShaderResource);

			builder.Write(fc.sceneColorLDR, RGResourceState::RenderTarget);
		},
		[this, &fc, device](CommandContext& passCtx) {
			passCtx.BeginTimestamp(PassID::PresentPass);
			{
				passCtx.SetRenderTarget(1, &m_sceneColorLDRTexture, {});
				passCtx.SetPipeline(m_presentPipeline);
				passCtx.SetViewport(0, 0, (float)m_viewportWidth, (float)m_viewportHeight);
				passCtx.SetScissorRect(0, 0, (LONG)m_viewportWidth, (LONG)m_viewportHeight);
				passCtx.BindTexture(0, m_sceneColorTexture);
				passCtx.BindTexture(1, m_gbufferAlbedo);
				passCtx.BindTexture(2, m_gbufferNormal);
				passCtx.BindTexture(3, m_gbufferMR);
				passCtx.BindTexture(4, m_gbufferEmissive);
				passCtx.BindTexture(5, m_depthTexture);
				passCtx.BindTexture(6, m_gtaoTexture);

				auto debugMode = (uint32_t)m_debugViewMode;
				passCtx.SetRootConstants(7, &debugMode, 1);

				passCtx.Draw(3, 0);
			}
			passCtx.EndTimestamp(PassID::PresentPass);
		}
	);
}
void Renderer::AddDebugLinePass(GraphicsDevice* device, RenderGraph& graph, FrameContext& fc)
{
	if (!m_showAABB) return;
	if (m_debugLines.empty()) return;

	graph.AddPass(
		"DebugLinePass",
		[&](RGBuilder& builder) {
			builder.Read(fc.depthTexture, RGResourceState::DepthRead);

			builder.Write(fc.sceneColorLDR, RGResourceState::RenderTarget);
		},
		[this, &fc, device](CommandContext& passCtx) {
			passCtx.SetPipeline(m_debugLinePipeline);
			passCtx.SetRenderTarget(1, &m_sceneColorLDRTexture, m_depthTexture);
			passCtx.SetViewport(0, 0, (float)m_viewportWidth, (float)m_viewportHeight);
			passCtx.SetScissorRect(0, 0, (LONG)m_viewportWidth, (LONG)m_viewportHeight);

			passCtx.BindConstantBuffer(0, fc.perFrameCB);

			passCtx.SetVertexBuffer(m_debugLineBuffer);

			passCtx.Draw((uint32_t)m_debugLines.size(), 0);
		}
	);
}
