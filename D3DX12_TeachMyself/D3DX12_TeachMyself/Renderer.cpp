#define NOMINMAX

#include "Renderer.h"
#include "ShaderCompiler.h"
#include "CommandContext.h"
#include "GPUTimestampProfiler.h"
#include <chrono>
#include "AssetLoader.h"
#include "MokoLog.h"
#include "MokoTime.h"
#include <random>

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

void Renderer::Init(GraphicsDevice* device)
{
	m_width = device->GetWidth();
	m_height = device->GetHeight();

	ShaderCompiler::Reserve(50);

	m_fullscreenVS = ShaderCompiler::CompileFromFile(
		L"shaders_FullScreen_VS.hlsl",
		"main",
		"vs_5_0"
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
	InitDebugPass(device);


	CreateCubeMap(device);
	CreateIrradianceMap(device);
	CreateBRDFLUT(device);
	CreatePrefilteredEnvironmentMap(device);

	CubemapTextureDesc cubemapDesc = {};
	cubemapDesc.format = Format::R32_TYPELESS;
	cubemapDesc.usage = TextureUsage::DepthStencil;
	cubemapDesc.height = 1;
	cubemapDesc.width = 1;

	float defaultDepth = 1.0f;
	m_defaultCubemapTexture = device->CreateCubemapTexture(cubemapDesc, &defaultDepth);

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

	debugMode = DebugMode::PBR_Enabled;
}

void Renderer::Render(GraphicsDevice* device, const RenderScene& renderScene)
{
	ReloadPSO(device);

	if (m_needsResize)
		Resize(device);

	CommandContext& ctx = device->BeginFrame();

	RenderGraph graph(device);
	auto& frameData = renderScene.frameData;

	RGResourceDesc backBufferDesc = { m_width, m_height, Format::R8G8B8A8_UNORM, TextureUsage::RenderTarget};
	RGResourceHandle backBuffer = graph.ImportTexture(device->GetCurrentBackBuffer(), backBufferDesc, RGResourceState::Present);

	RGResourceDesc gbufferAlbedoDesc = { m_width, m_height, Format::R8G8B8A8_UNORM, TextureUsage::RenderTarget };
	RGResourceHandle gbufferAlbedo = graph.ImportTexture(m_gbufferAlbedo, gbufferAlbedoDesc, RGResourceState::RenderTarget);

	RGResourceDesc gbufferNormalDesc = { m_width, m_height, Format::R16G16B16A16_FLOAT, TextureUsage::RenderTarget };
	RGResourceHandle gbufferNormal = graph.ImportTexture(m_gbufferNormal, gbufferNormalDesc, RGResourceState::RenderTarget);

	RGResourceDesc gbufferMRDesc = { m_width, m_height, Format::R8G8B8A8_UNORM, TextureUsage::RenderTarget };
	RGResourceHandle gbufferMR = graph.ImportTexture(m_gbufferMR, gbufferMRDesc, RGResourceState::RenderTarget);

	RGResourceDesc depthTextrueDesc = { m_width, m_height, Format::R32_TYPELESS, TextureUsage::DepthStencil };
	RGResourceHandle depthTexture = graph.ImportTexture(m_depthTexture, depthTextrueDesc, RGResourceState::DepthWrite);

	RGResourceDesc cubeMapDesc = { m_width, m_height, Format::R16G16B16A16_FLOAT, TextureUsage::ShaderResource };
	RGResourceHandle cubeMap = graph.ImportTexture(m_cubemapTexture, cubeMapDesc, RGResourceState::ShaderResource);

	RGResourceDesc irradiacneMapDesc = { m_width, m_height, Format::R16G16B16A16_FLOAT, TextureUsage::ShaderResource };
	RGResourceHandle irradiacneMap = graph.ImportTexture(m_irradianceMapTexture, irradiacneMapDesc, RGResourceState::ShaderResource);

	RGResourceDesc prefilteredEnvMapDesc = { m_width, m_height, Format::R16G16B16A16_FLOAT, TextureUsage::ShaderResource };
	RGResourceHandle prefilteredEnvMap= graph.ImportTexture(m_prefilteredEnvMapTexture, prefilteredEnvMapDesc, RGResourceState::ShaderResource);

	RGResourceDesc brdfLUTDesc = { m_width, m_height, Format::R16G16B16A16_FLOAT, TextureUsage::ShaderResource };
	RGResourceHandle brdfLUT = graph.ImportTexture(m_brdfLUTTexture, brdfLUTDesc, RGResourceState::ShaderResource);

	RGResourceDesc shadowMapDesc = { 4096, 4096, Format::R32_TYPELESS, TextureUsage::DepthStencil };
	RGResourceHandle shadowMap = graph.ImportTexture(m_shadowMapTexture, shadowMapDesc, RGResourceState::DepthWrite);

	std::vector<RGResourceHandle> pointShadowMaps;
	RGResourceDesc pointShadowMapDesc = { 1024, 1024, Format::R32_TYPELESS, TextureUsage::DepthStencil };
	const int importedPointLightCount = std::min(MAX_POINT_LIGHTS, frameData.PointLightCount);
	for (int i = 0; i < importedPointLightCount; i++)
	{
		pointShadowMaps.push_back(graph.ImportTexture(m_pointShadowMapTextures[i], pointShadowMapDesc, RGResourceState::DepthWrite));
	}

	RGResourceDesc ssaoTextureDesc = { m_width / 2, m_height / 2, Format::R8_UNORM, TextureUsage::RenderTarget };
	RGResourceHandle ssaoTexture = graph.ImportTexture(m_ssaoTexture, ssaoTextureDesc, RGResourceState::RenderTarget);

	RGResourceDesc ssaoNoiseTextureDesc = { 4, 4, Format::R32G32B32A32_FLOAT, TextureUsage::ShaderResource };
	RGResourceHandle ssaoNoiseTexture = graph.ImportTexture(m_ssaoTexture, ssaoNoiseTextureDesc, RGResourceState::ShaderResource);

	RGResourceDesc ssaoTempTextureDesc = { m_width / 2, m_height / 2, Format::R8_UNORM, TextureUsage::RenderTarget };
	RGResourceHandle ssaoTempTexture = graph.ImportTexture(m_ssaoTempTexture, ssaoTempTextureDesc, RGResourceState::RenderTarget);

	RGResourceDesc gtaoTextureDesc = { m_width, m_height, Format::R8G8B8A8_UNORM, TextureUsage::UnorderedAccess };
	RGResourceHandle gtaoTexture = graph.ImportTexture(m_gtaoTexture, gtaoTextureDesc, RGResourceState::UnorderedAccess);
	
	RGResourceDesc gtaoTempTextureDesc = { m_width, m_height, Format::R8G8B8A8_UNORM, TextureUsage::UnorderedAccess };
	RGResourceHandle gtaoTempTexture = graph.ImportTexture(m_gtaoTempTexture, gtaoTempTextureDesc, RGResourceState::UnorderedAccess);

	PerFrameCB perFrame;
	XMMATRIX vp = XMLoadFloat4x4(&frameData.ViewMatrix) * XMLoadFloat4x4(&frameData.ProjMatrix);
	XMStoreFloat4x4(&perFrame.ViewProj, XMMatrixTranspose(vp));
	XMMATRIX inverseVP = XMMatrixInverse(nullptr, vp);
	XMStoreFloat4x4(&perFrame.InvViewProj, inverseVP);
	perFrame.CameraPos = frameData.CameraPos;
	XMStoreFloat2(&perFrame.ScreenSize, { static_cast<float>(m_width), static_cast<float>(m_height) });
	

	LightCB light;
	light.Direction = frameData.DirectionalLightDir;
	light.Color = frameData.DirectionalLightColor;
	light.Intensity = frameData.DirectionalLightIntensity;
	light.Ambient = { 0.03f, 0.03f, 0.03f };

	light.PointLightCount = frameData.PointLightCount;
	for (int i = 0; i < std::min(MAX_POINT_LIGHTS, frameData.PointLightCount); i++)
	{
		auto pointLight = frameData.PointLights[i];
		light.PointLights[i] = { pointLight.Position, pointLight.Radius, pointLight.Color, pointLight.Intensity };
	}

	XMVECTOR targetPos = XMVectorSet(0, 0, 0, 0); 
	XMVECTOR lightPos = XMVectorSubtract(targetPos, XMVectorScale(XMLoadFloat3(&frameData.DirectionalLightDir), 50.0f));
	XMMATRIX lightView = XMMatrixLookAtLH(lightPos, targetPos, XMVectorSet(0, 1, 0, 0));
	XMMATRIX lightProj = XMMatrixOrthographicLH(100.0f, 100.0f, 1.0f, 300.0f);
	XMMATRIX lightVP = lightView * lightProj;




	PointShadowCB pointShadow;
	const XMFLOAT3 targets[6] = { {1,0,0},{-1,0,0},{0,1,0},{0,-1,0},{0,0,1},{0,0,-1} };
	const XMFLOAT3 ups[6] = { {0,1,0},{0,1,0},{0,0,-1},{0,0,1},{0,1,0},{0,1,0} };
	for (int lightIdx = 0; lightIdx < frameData.PointLightCount; lightIdx++)
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
		backBuffer, gbufferAlbedo, gbufferNormal, gbufferMR,
		depthTexture, cubeMap,
		irradiacneMap, prefilteredEnvMap, brdfLUT, shadowMap, pointShadowMaps,
		importedPointLightCount,
		std::move(fcPointLights),
		ssaoTexture, ssaoNoiseTexture, ssaoTempTexture, gtaoTexture, gtaoTempTexture,
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
		m_width / 4.0f,
		m_height / 4.0f
	);

	for (int i = 0; i < 32; i++)
	{
		ssaoCB.Samples[i] = hemisphereSamples[i];
	}


	BilateralBlurCB blurCB;
	blurCB.DepthSigma = 80;
	blurCB.NormalSigma = 12;
	blurCB.TexelSize = { 2.0f / m_width, 2.0f / m_height };


	GTAOCB gtaoCB;
	XMStoreFloat4x4(&gtaoCB.View, XMMatrixTranspose(XMLoadFloat4x4(&frameData.ViewMatrix)));
	XMStoreFloat4x4(&gtaoCB.InvProj, XMMatrixInverse(nullptr, XMMatrixTranspose(XMLoadFloat4x4(&frameData.ProjMatrix))));
	XMStoreFloat2(&gtaoCB.InvRes, { 1.0f / m_width, 1.0f / m_height });
	gtaoCB.Radius = 0.5f;
	gtaoCB.FalloffStart = gtaoCB.Radius * 0.75f;
	gtaoCB.FalloffEnd = gtaoCB.Radius;
	gtaoCB.NumSlices = 2;
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

	AddDepthPrePass(device, graph, fc, renderScene);
	AddGBufferPass(device, graph, fc, renderScene);
	AddDirectionalShadowPass(device, graph, fc, renderScene);
	AddPointShadowPass(device, graph, fc, renderScene);

	
	AddSSAOPass(device, graph, fc, renderScene);
	AddGTAOPass(device, graph, fc, renderScene);

	//AddSSAOBilateralBlurPass(device, graph, fc, renderScene);
	AddGTAOBilateralBlurPass(device, graph, fc, renderScene);

	if (debugMode == DebugMode::PBR_Enabled)
	{
		AddPBRLightingPass(device, graph, fc, renderScene);
	}
	else
	{
		AddDebugPass(device, graph, fc, renderScene);
	}

	AddSkyboxPass(device, graph, fc, renderScene);

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
	bool dirtyDebugPS = ShaderCompiler::IsDirty(m_debugPS);
	bool dirtyDepthDebugPS = ShaderCompiler::IsDirty(m_depthDebugPS);
	bool dirtyPBRLightingPS = ShaderCompiler::IsDirty(m_PBRlightingPS);

	bool dirtyGTAOCS = ShaderCompiler::IsDirty(m_GTAOCS);

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

	if (dirtyGTAOCS)
	{
		m_GTAOComputePipelineDesc.cs = ShaderCompiler::GetBytecode(m_GTAOCS);
		m_GTAOComputePipeline = device->CreateComputePipeline(m_GTAOComputePipelineDesc);
	}

	// 마지막에 한 번만 clear
	if (dirtyDepthVS)        ShaderCompiler::ClearDirty(m_depthVS);

	if (dirtyGBufferVS)      ShaderCompiler::ClearDirty(m_gBufferVS);
	if (dirtyGBufferOpaque)  ShaderCompiler::ClearDirty(m_gBufferOpaquePS);
	if (dirtyGBufferAlpha)   ShaderCompiler::ClearDirty(m_gBufferAlphaPS);

	if (dirtyFullscreenVS)   ShaderCompiler::ClearDirty(m_fullscreenVS);
	if (dirtyDebugPS)        ShaderCompiler::ClearDirty(m_debugPS);
	if (dirtyDepthDebugPS)   ShaderCompiler::ClearDirty(m_depthDebugPS);
	if (dirtyPBRLightingPS)  ShaderCompiler::ClearDirty(m_PBRlightingPS);

	if (dirtyGTAOCS)		 ShaderCompiler::ClearDirty(m_GTAOCS);
}

void Renderer::PrintGPUTimes(GraphicsDevice* device)
{
	float depthPrePassTime = device->GetTimestampMs(PassID::DepthPrePass);
	float gBufferPassTime = device->GetTimestampMs(PassID::GBufferPass);
	float gBufferAlphaPassTime = device->GetTimestampMs(PassID::GBufferAlphaPass);
	float directionalShadowPassTime = device->GetTimestampMs(PassID::DirectionalShadowPass);
	float pointShadowPassTime = device->GetTimestampMs(PassID::PointShadowPass);
	float PBRlightingPassTime = device->GetTimestampMs(PassID::PBRLightingPass);
	float skyboxPassTime = device->GetTimestampMs(PassID::SkyboxPass);

	LOG_INFO(
		"[GPU Time] | DepthPre: %6.3f ms | GBuffer: %6.3f ms | GBufferAlpha: %6.3f ms | DirectionalShadow: %6.3f ms | PointShadow : %6.3fms | PBRLighting: %6.3f ms | Skybox: %6.3f ms\n",
		depthPrePassTime,
		gBufferPassTime,
		gBufferAlphaPassTime,
		directionalShadowPassTime,
		pointShadowPassTime,
		PBRlightingPassTime,
		skyboxPassTime);
}


void Renderer::CreateCubeMap(GraphicsDevice* device)
{
	RootSignatureDesc equirectConvertRSDesc = {};
	equirectConvertRSDesc.allowIA = false;
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
	//float* hdrData = loader.LoadHDR("../citrus_orchard_puresky_4k.hdr", w, h, ch, 4);
	float* hdrData = loader.LoadHDR("../venice_sunset_4k.hdr", w, h, ch, 4);
	//float* hdrData = loader.LoadHDR("../qwantani_dusk_2_puresky_4k.hdr", w, h, ch, 4);

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
	cubemapDesc.usage = TextureUsage::UnorderedAccess;
	cubemapDesc.format = Format::R16G16B16A16_FLOAT;
	m_cubemapTexture = device->CreateCubemapTexture(cubemapDesc);


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
	irradianceMapRSDesc.rootParamDescs.push_back({ RootParamType::DescriptorTable, RangeType::SRV, 0, 1, ShaderVisibility::All });
	irradianceMapRSDesc.rootParamDescs.push_back({ RootParamType::DescriptorTable, RangeType::UAV, 0, 1, ShaderVisibility::All });
	irradianceMapRSDesc.staticSamplers.push_back({ SamplerFilter::Bilinear, SamplerAddressMode::Wrap, 0, ShaderVisibility::All });

	m_irradianceMapCS = ShaderCompiler::CompileFromFile(
		L"shaders_IrradianceMap_CS.hlsl",
		"CSMain",
		"cs_5_1"
	);

	ComputePipelineDesc irradianceMapPSODesc = {};
	irradianceMapPSODesc.rootSignatureDesc = irradianceMapRSDesc;
	irradianceMapPSODesc.cs = ShaderCompiler::GetBytecode(m_irradianceMapCS);
	m_irradianceMapCSPipeline = device->CreateComputePipeline(irradianceMapPSODesc);

	CubemapTextureDesc irradianceCubeMapDesc = {};
	irradianceCubeMapDesc.width = 32;
	irradianceCubeMapDesc.usage = TextureUsage::UnorderedAccess;
	irradianceCubeMapDesc.height = 32;
	irradianceCubeMapDesc.format = Format::R16G16B16A16_FLOAT;
	m_irradianceMapTexture = device->CreateCubemapTexture(irradianceCubeMapDesc);

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
	perfilteredEnvironmentMapDesc.rootParamDescs.push_back({ RootParamType::DescriptorTable, RangeType::SRV, 0, 1, ShaderVisibility::All });
	perfilteredEnvironmentMapDesc.rootParamDescs.push_back({ RootParamType::DescriptorTable, RangeType::UAV, 0, 1, ShaderVisibility::All });
	perfilteredEnvironmentMapDesc.rootParamDescs.push_back({ RootParamType::RootConstants, RangeType::CBV, 0, 1, ShaderVisibility::All });
	perfilteredEnvironmentMapDesc.staticSamplers.push_back({ SamplerFilter::Bilinear, SamplerAddressMode::Wrap, 0, ShaderVisibility::All });

	m_prefilteredEnvironmentMapCS = ShaderCompiler::CompileFromFile(
		L"shaders_PrefilteredEnvironmentMap_CS.hlsl",
		"CSMain",
		"cs_5_1"
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
	m_prefilteredEnvMapTexture = device->CreateCubemapTexture(prefiltredEnvironmentCubeMapDesc);

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
	TextureDesc brdfLUTDesc = { 512, 512, Format::R16G16_FLOAT, TextureUsage::UnorderedAccess };
	m_brdfLUTTexture = device->CreateTexture(brdfLUTDesc);


	m_brdfLUTCS = ShaderCompiler::CompileFromFile(
		L"shaders_brdfLUT_CS.hlsl",
		"CSMain",
		"cs_5_1"
	);

	RootSignatureDesc brdfLUTRSDesc = {};
	brdfLUTRSDesc.allowIA = false;
	brdfLUTRSDesc.rootParamDescs.push_back({ RootParamType::DescriptorTable, RangeType::UAV, 0, 1, ShaderVisibility::All });

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

		XMMATRIX lightView = XMMatrixLookAtLH(
			XMVectorSubtract(center, XMVectorScale(lightDir, 100.0f)),
			center,
			XMVectorSet(0, 1, 0, 0));

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

		float sceneZMin = FLT_MAX;
		for (int i = 0; i < 8; i++)
		{
			XMVECTOR v = XMVector4Transform(XMLoadFloat3(&sceneCorners[i]), lightView);

			XMFLOAT3 p;
			XMStoreFloat3(&p, v);

			sceneZMin = std::min(sceneZMin, p.z);
		}

		mins.z = sceneZMin;

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

void Renderer::InitDepthPrePass(GraphicsDevice* device)
{
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
}
void Renderer::InitGBufferPass(GraphicsDevice* device)
{
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

	std::vector<VertexAttribute> vertexAttributes;
	vertexAttributes.push_back({ Semantic::POSITION, Format::R32G32B32_FLOAT, 0 });
	vertexAttributes.push_back({ Semantic::NORMAL,   Format::R32G32B32_FLOAT, 0 });
	vertexAttributes.push_back({ Semantic::TANGENT, Format::R32G32B32A32_FLOAT, 0 });
	vertexAttributes.push_back({ Semantic::TEXCOORD, Format::R32G32_FLOAT, 0 });


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
}
void Renderer::InitDirectionalShadowPass(GraphicsDevice* device)
{
	std::vector<VertexAttribute> vertexAttributes;
	vertexAttributes.push_back({ Semantic::POSITION, Format::R32G32B32_FLOAT, 0 });
	vertexAttributes.push_back({ Semantic::NORMAL,   Format::R32G32B32_FLOAT, 0 });
	vertexAttributes.push_back({ Semantic::TANGENT, Format::R32G32B32A32_FLOAT, 0 });
	vertexAttributes.push_back({ Semantic::TEXCOORD, Format::R32G32_FLOAT, 0 });

	RootSignatureDesc shadowMapRSDesc = {};
	shadowMapRSDesc.allowIA = true;
	shadowMapRSDesc.rootParamDescs.push_back({ RootParamType::RootCBV, RangeType::CBV, 0, 1, ShaderVisibility::Vertex });
	shadowMapRSDesc.rootParamDescs.push_back({ RootParamType::RootCBV, RangeType::CBV, 1, 1, ShaderVisibility::Vertex });
	shadowMapRSDesc.rootParamDescs.push_back({ RootParamType::RootCBV, RangeType::CBV, 2, 1, ShaderVisibility::Vertex });

	m_shadowMapVS = ShaderCompiler::CompileFromFile(
		L"shaders_shadowMap_VS.hlsl",
		"main",
		"vs_5_0"
	);


	PipelineDesc shadowMapPSODesc = {};
	shadowMapPSODesc.rootSignatureDesc = shadowMapRSDesc;
	shadowMapPSODesc.vs = ShaderCompiler::GetBytecode(m_shadowMapVS);
	shadowMapPSODesc.ps = {};
	shadowMapPSODesc.vertexAttributes = vertexAttributes;
	shadowMapPSODesc.rtvFormats = { Format::UNKNOWN };
	shadowMapPSODesc.dsvFormat = Format::D32_FLOAT;
	shadowMapPSODesc.depthEnable = true;
	shadowMapPSODesc.depthWrite = true;
	shadowMapPSODesc.depthFunc = ComparisonFunc::LessEqual;
	shadowMapPSODesc.cullMode = CullMode::Back;

	m_shadowMapPipeline = device->CreatePipeline(shadowMapPSODesc);

	TextureDesc shadowMapDesc = { 4096, 4096, Format::D32_FLOAT, TextureUsage::DepthStencil };
	m_shadowMapTexture = device->CreateTexture(shadowMapDesc, nullptr);
}
void Renderer::InitPointShadowPass(GraphicsDevice* device)
{
	std::vector<VertexAttribute> vertexAttributes;
	vertexAttributes.push_back({ Semantic::POSITION, Format::R32G32B32_FLOAT, 0 });
	vertexAttributes.push_back({ Semantic::NORMAL,   Format::R32G32B32_FLOAT, 0 });
	vertexAttributes.push_back({ Semantic::TANGENT, Format::R32G32B32A32_FLOAT, 0 });
	vertexAttributes.push_back({ Semantic::TEXCOORD, Format::R32G32_FLOAT, 0 });

	RootSignatureDesc pointShadowMapRSDesc = {};
	pointShadowMapRSDesc.allowIA = true;
	pointShadowMapRSDesc.rootParamDescs.push_back({ RootParamType::RootCBV, RangeType::CBV, 0, 1, ShaderVisibility::Vertex });
	pointShadowMapRSDesc.rootParamDescs.push_back({ RootParamType::RootCBV, RangeType::CBV, 1, 1, ShaderVisibility::Vertex });
	pointShadowMapRSDesc.rootParamDescs.push_back({ RootParamType::RootConstants, RangeType::CBV, 2, 2, ShaderVisibility::Vertex });

	m_pointShadowMapVS = ShaderCompiler::CompileFromFile(
		L"shaders_pointShadowMap_VS.hlsl",
		"main",
		"vs_5_0"
	);


	PipelineDesc pointShadowMapPSODesc = {};
	pointShadowMapPSODesc.rootSignatureDesc = pointShadowMapRSDesc;
	pointShadowMapPSODesc.vs = ShaderCompiler::GetBytecode(m_pointShadowMapVS);
	pointShadowMapPSODesc.ps = {};
	pointShadowMapPSODesc.vertexAttributes = vertexAttributes;
	pointShadowMapPSODesc.rtvFormats = { Format::UNKNOWN };
	pointShadowMapPSODesc.dsvFormat = Format::D32_FLOAT;
	pointShadowMapPSODesc.depthEnable = true;
	pointShadowMapPSODesc.depthWrite = true;
	pointShadowMapPSODesc.depthFunc = ComparisonFunc::LessEqual;
	pointShadowMapPSODesc.cullMode = CullMode::Back;

	m_pointShadowMapPipeline = device->CreatePipeline(pointShadowMapPSODesc);

	CubemapTextureDesc pointShadowMapDesc = { 1024, 1024, Format::D32_FLOAT, TextureUsage::DepthStencil };

	for (int i = 0; i < MAX_POINT_LIGHTS; i++)
	{
		m_pointShadowMapTextures.push_back(device->CreateCubemapTexture(pointShadowMapDesc, nullptr));
	}
}
void Renderer::InitSSAOPass(GraphicsDevice* device)
{
	RootSignatureDesc ssaoPassRSDesc = {};
	ssaoPassRSDesc.allowIA = true;
	ssaoPassRSDesc.rootParamDescs.push_back({ RootParamType::RootCBV, RangeType::CBV, 0, 1, ShaderVisibility::Pixel });
	ssaoPassRSDesc.rootParamDescs.push_back({ RootParamType::RootCBV, RangeType::CBV, 1, 1, ShaderVisibility::Pixel });
	ssaoPassRSDesc.rootParamDescs.push_back({ RootParamType::DescriptorTable, RangeType::SRV, 0, 1, ShaderVisibility::Pixel });
	ssaoPassRSDesc.rootParamDescs.push_back({ RootParamType::DescriptorTable, RangeType::SRV, 1, 1, ShaderVisibility::Pixel });
	ssaoPassRSDesc.rootParamDescs.push_back({ RootParamType::DescriptorTable, RangeType::SRV, 2, 1, ShaderVisibility::Pixel });
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
		"vs_5_0"
	);
	m_ssaoPS = ShaderCompiler::CompileFromFile(
		L"shaders_SSAO_PS.hlsl",
		"main",
		"ps_5_0"
	);

	m_SSAOPipelineDesc = {
		ssaoPassRSDesc,
		ShaderCompiler::GetBytecode(m_ssaoVS), ShaderCompiler::GetBytecode(m_ssaoPS), vertexAttributes,
		{Format::R8_UNORM},
		Format::UNKNOWN,
		false, false, ComparisonFunc::Equal,
		CullMode::None };
	m_SSAOPipeline = device->CreatePipeline(m_SSAOPipelineDesc);

	TextureDesc ssaoTextureDesc = { m_width / 2, m_height / 2, Format::R8_UNORM, TextureUsage::RenderTarget };
	m_ssaoTexture = device->CreateTexture(ssaoTextureDesc, nullptr);

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
	TextureDesc ssaoNoiseTextureDesc = {};
	ssaoNoiseTextureDesc.width = 4;
	ssaoNoiseTextureDesc.height = 4;
	ssaoNoiseTextureDesc.format = Format::R32G32B32A32_FLOAT;
	ssaoNoiseTextureDesc.usage = TextureUsage::ShaderResource;

	m_ssaoNoiseTexture = device->CreateTexture(ssaoNoiseTextureDesc, noise.data());
}
void Renderer::InitGTAOPass(GraphicsDevice* device)
{
	RootSignatureDesc GTAOPassRSDesc = {};
	GTAOPassRSDesc.allowIA = false;
	GTAOPassRSDesc.rootParamDescs.push_back({ RootParamType::RootCBV, RangeType::CBV, 0, 1, ShaderVisibility::All });
	GTAOPassRSDesc.rootParamDescs.push_back({ RootParamType::DescriptorTable, RangeType::UAV, 0, 1, ShaderVisibility::All });
	GTAOPassRSDesc.rootParamDescs.push_back({ RootParamType::DescriptorTable, RangeType::SRV, 0, 1, ShaderVisibility::All });
	GTAOPassRSDesc.rootParamDescs.push_back({ RootParamType::DescriptorTable, RangeType::SRV, 1, 1, ShaderVisibility::All });
	GTAOPassRSDesc.staticSamplers.push_back({ SamplerFilter::Point, SamplerAddressMode::Clamp, 0, ShaderVisibility::All });

	m_GTAOCS = ShaderCompiler::CompileFromFile(
		L"shaders_GTAO_CS.hlsl",
		"CSMain",
		"cs_5_1"
	);

	m_GTAOComputePipelineDesc = {};
	m_GTAOComputePipelineDesc.rootSignatureDesc = GTAOPassRSDesc;
	m_GTAOComputePipelineDesc.cs = ShaderCompiler::GetBytecode(m_GTAOCS);
	m_GTAOComputePipeline = device->CreateComputePipeline(m_GTAOComputePipelineDesc);

	TextureDesc gtaoTextureDesc = {};
	gtaoTextureDesc.width = m_width;
	gtaoTextureDesc.height = m_height;
	gtaoTextureDesc.format = Format::R8G8B8A8_UNORM;
	gtaoTextureDesc.usage = TextureUsage::UnorderedAccess;
	m_gtaoTexture = device->CreateTexture(gtaoTextureDesc);
	m_gtaoTempTexture = device->CreateTexture(gtaoTextureDesc);
}
void Renderer::InitSSAOBilateralBlurPass(GraphicsDevice* device)
{
	m_bilateralBlurPS_Vertical = ShaderCompiler::CompileFromFile(
		L"shaders_BilateralBlurVertical_PS.hlsl",
		"main",
		"ps_5_0"
	);

	m_bilateralBlurPS_Horizontal = ShaderCompiler::CompileFromFile(
		L"shaders_BilateralBlurHorizontal_PS.hlsl",
		"main",
		"ps_5_0"
	);

	std::vector<VertexAttribute> vertexAttributes;
	vertexAttributes.push_back({ Semantic::POSITION, Format::R32G32B32_FLOAT, 0 });
	vertexAttributes.push_back({ Semantic::NORMAL,   Format::R32G32B32_FLOAT, 0 });
	vertexAttributes.push_back({ Semantic::TANGENT, Format::R32G32B32A32_FLOAT, 0 });
	vertexAttributes.push_back({ Semantic::TEXCOORD, Format::R32G32_FLOAT, 0 });

	RootSignatureDesc bilateralBluPassRSDesc = {};
	bilateralBluPassRSDesc.allowIA = true;
	bilateralBluPassRSDesc.rootParamDescs.push_back({ RootParamType::RootCBV, RangeType::CBV, 0, 1, ShaderVisibility::Pixel });
	bilateralBluPassRSDesc.rootParamDescs.push_back({ RootParamType::DescriptorTable, RangeType::SRV, 0, 1, ShaderVisibility::Pixel });
	bilateralBluPassRSDesc.rootParamDescs.push_back({ RootParamType::DescriptorTable, RangeType::SRV, 1, 1, ShaderVisibility::Pixel });
	bilateralBluPassRSDesc.rootParamDescs.push_back({ RootParamType::DescriptorTable, RangeType::SRV, 2, 1, ShaderVisibility::Pixel });
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

	TextureDesc ssaoTempTextureDesc = { m_width / 2, m_height / 2, Format::R8_UNORM, TextureUsage::RenderTarget };
	m_ssaoTempTexture = device->CreateTexture(ssaoTempTextureDesc, nullptr);
}
void Renderer::InitGTAOBilateralBlurPass(GraphicsDevice* device)
{
	m_bilateralBlurCS = ShaderCompiler::CompileFromFile(
		L"shaders_BilateralBlur_CS.hlsl",
		"CSMain",
		"cs_5_1"
	);

	RootSignatureDesc bilateralBlurPassRSDesc = {};
	bilateralBlurPassRSDesc.allowIA = false;
	bilateralBlurPassRSDesc.rootParamDescs.push_back({ RootParamType::RootCBV, RangeType::CBV, 0, 1, ShaderVisibility::All });
	bilateralBlurPassRSDesc.rootParamDescs.push_back({ RootParamType::DescriptorTable, RangeType::SRV, 0, 1, ShaderVisibility::All });
	bilateralBlurPassRSDesc.rootParamDescs.push_back({ RootParamType::DescriptorTable, RangeType::SRV, 1, 1, ShaderVisibility::All });
	bilateralBlurPassRSDesc.rootParamDescs.push_back({ RootParamType::DescriptorTable, RangeType::SRV, 2, 1, ShaderVisibility::All });
	bilateralBlurPassRSDesc.rootParamDescs.push_back({ RootParamType::DescriptorTable, RangeType::UAV, 0, 1, ShaderVisibility::All });
	bilateralBlurPassRSDesc.staticSamplers.push_back({ SamplerFilter::Bilinear, SamplerAddressMode::Wrap, 0, ShaderVisibility::All });

	m_bilateralBlurComputePipelineDesc = {};
	m_bilateralBlurComputePipelineDesc.rootSignatureDesc = bilateralBlurPassRSDesc;
	m_bilateralBlurComputePipelineDesc.cs = ShaderCompiler::GetBytecode(m_bilateralBlurCS);
	m_bilateralBlurComputePipeline = device->CreateComputePipeline(m_bilateralBlurComputePipelineDesc);

	TextureDesc gtaoTempTextureDesc = {};
	gtaoTempTextureDesc.width = m_width;
	gtaoTempTextureDesc.height = m_height;
	gtaoTempTextureDesc.format = Format::R8G8B8A8_UNORM;
	gtaoTempTextureDesc.usage = TextureUsage::UnorderedAccess;
	m_gtaoTempTexture = device->CreateTexture(gtaoTempTextureDesc);
}
void Renderer::InitPBRLightingPass(GraphicsDevice* device)
{
	RootSignatureDesc PBRlightingPassRSDesc = {};
	PBRlightingPassRSDesc.rootParamDescs.push_back({ RootParamType::RootCBV, RangeType::CBV, 0, 1, ShaderVisibility::Pixel });
	PBRlightingPassRSDesc.rootParamDescs.push_back({ RootParamType::RootCBV, RangeType::CBV, 1, 1, ShaderVisibility::Pixel });
	PBRlightingPassRSDesc.rootParamDescs.push_back({ RootParamType::RootCBV, RangeType::CBV, 2, 1, ShaderVisibility::Pixel });
	PBRlightingPassRSDesc.rootParamDescs.push_back({ RootParamType::DescriptorTable, RangeType::SRV, 0, 1, ShaderVisibility::Pixel });
	PBRlightingPassRSDesc.rootParamDescs.push_back({ RootParamType::DescriptorTable, RangeType::SRV, 1, 1, ShaderVisibility::Pixel });
	PBRlightingPassRSDesc.rootParamDescs.push_back({ RootParamType::DescriptorTable, RangeType::SRV, 2, 1, ShaderVisibility::Pixel });
	PBRlightingPassRSDesc.rootParamDescs.push_back({ RootParamType::DescriptorTable, RangeType::SRV, 3, 1, ShaderVisibility::Pixel });
	PBRlightingPassRSDesc.rootParamDescs.push_back({ RootParamType::DescriptorTable, RangeType::SRV, 4, 1, ShaderVisibility::Pixel });
	PBRlightingPassRSDesc.rootParamDescs.push_back({ RootParamType::DescriptorTable, RangeType::SRV, 5, 1, ShaderVisibility::Pixel });
	PBRlightingPassRSDesc.rootParamDescs.push_back({ RootParamType::DescriptorTable, RangeType::SRV, 6, 1, ShaderVisibility::Pixel });
	PBRlightingPassRSDesc.rootParamDescs.push_back({ RootParamType::DescriptorTable, RangeType::SRV, 7, 1, ShaderVisibility::Pixel });
	PBRlightingPassRSDesc.rootParamDescs.push_back({ RootParamType::DescriptorTable, RangeType::SRV, 8, 1, ShaderVisibility::Pixel });
	PBRlightingPassRSDesc.rootParamDescs.push_back({ RootParamType::DescriptorTable, RangeType::SRV, 9, 1, ShaderVisibility::Pixel });
	PBRlightingPassRSDesc.rootParamDescs.push_back({ RootParamType::DescriptorTable, RangeType::SRV, 10, MAX_POINT_LIGHTS, ShaderVisibility::Pixel });
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
		"ps_5_1"
	);

	m_PBRlightingPassPipelineDesc = {
		PBRlightingPassRSDesc,
		ShaderCompiler::GetBytecode(m_fullscreenVS), ShaderCompiler::GetBytecode(m_PBRlightingPS), vertexAttributes,
		{ Format::R8G8B8A8_UNORM },
		Format::UNKNOWN,
		false, false, ComparisonFunc::Equal,
		CullMode::None };
	m_PBRlightingPassPipeline = device->CreatePipeline(m_PBRlightingPassPipelineDesc);
}
void Renderer::InitSkyboxPass(GraphicsDevice* device)
{
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
}
void Renderer::InitDebugPass(GraphicsDevice* device)
{
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
				passCtx.SetViewport(0, 0, (float)m_width, (float)m_height);
				passCtx.SetScissorRect(0, 0, (LONG)m_width, (LONG)m_height);

				passCtx.BindConstantBuffer(0, fc.perFrameCB);

				for (const auto& obj : scene.renderObjects)
				{
					if (obj.material.alphaMode == AlphaMode::Opaque)
					{
						auto perObjectCBHandle = passCtx.UpdateConstantBuffer(&obj.world, sizeof(PerObjectCB));
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
}
void Renderer::AddGBufferPass(GraphicsDevice* device, RenderGraph& graph, FrameContext& fc, const RenderScene& scene)
{
	graph.AddPass(
		"GBufferOpaquePass",
		[&](RGBuilder& builder) {
			builder.Read(fc.depthTexture, RGResourceState::DepthRead);
			builder.Write(fc.gbufferAlbedo, RGResourceState::RenderTarget);
			builder.Write(fc.gbufferNormal, RGResourceState::RenderTarget);
			builder.Write(fc.gbufferMR, RGResourceState::RenderTarget);
		},
		[this, &fc, device, &scene](CommandContext& passCtx) {
			passCtx.BeginTimestamp(PassID::GBufferPass);
			{
				TextureHandle renderTargets[] = { m_gbufferAlbedo, m_gbufferNormal, m_gbufferMR };
				passCtx.ClearRenderTargets(3, renderTargets, clearColor);
				passCtx.SetRenderTarget(3, renderTargets, m_depthTexture);

				passCtx.SetPipeline(m_gBufferOpaquePassPipeline);
				passCtx.SetViewport(0, 0, (float)m_width, (float)m_height);
				passCtx.SetScissorRect(0, 0, (LONG)m_width, (LONG)m_height);

				passCtx.BindConstantBuffer(0, fc.perFrameCB);
				for (const auto& obj : scene.renderObjects)
				{
					if (obj.material.alphaMode == AlphaMode::Opaque)
					{
						MaterialConstants matConst;
						matConst.alphaCutoff = obj.material.alphaCutoff;
						matConst.metallicFactor = obj.material.metallicFactor;
						matConst.roughnessFactor = obj.material.roughnessFactor;

						passCtx.SetRootConstants(5, &matConst, 3);

						auto perObjectCBHandle = passCtx.UpdateConstantBuffer(&obj.world, sizeof(PerObjectCB));
						passCtx.SetVertexBuffer(obj.vertexBuffer);
						passCtx.SetIndexBuffer(obj.indexBuffer);

						passCtx.BindConstantBuffer(1, perObjectCBHandle);
						passCtx.BindTexture(2, obj.material.baseColor);
						passCtx.BindTexture(3, obj.material.normal);
						passCtx.BindTexture(4, obj.material.metallicRoughness);

						assert(obj.indexBuffer.IsValid());
						assert(obj.vertexBuffer.IsValid());
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
		},
		[this, &fc, device, &scene](CommandContext& passCtx) {
			passCtx.BeginTimestamp(PassID::GBufferAlphaPass);
			{
				TextureHandle renderTargets[] = { m_gbufferAlbedo, m_gbufferNormal, m_gbufferMR };
				passCtx.SetRenderTarget(3, renderTargets, m_depthTexture);
				passCtx.SetPipeline(m_gBufferAlphaPassPipeline);
				passCtx.BindConstantBuffer(0, fc.perFrameCB);
				for (const auto& obj : scene.renderObjects)
				{
					if (obj.material.alphaMode == AlphaMode::Mask)
					{
						MaterialConstants matConst;
						matConst.alphaCutoff = obj.material.alphaCutoff;
						matConst.metallicFactor = obj.material.metallicFactor;
						matConst.roughnessFactor = obj.material.roughnessFactor;
						passCtx.SetRootConstants(5, &matConst, 3);

						auto perObjectCBHandle = passCtx.UpdateConstantBuffer(&obj.world, sizeof(PerObjectCB));

						passCtx.SetVertexBuffer(obj.vertexBuffer);
						passCtx.SetIndexBuffer(obj.indexBuffer);

						passCtx.BindConstantBuffer(1, perObjectCBHandle);
						passCtx.BindTexture(2, obj.material.baseColor);
						passCtx.BindTexture(3, obj.material.normal);
						passCtx.BindTexture(4, obj.material.metallicRoughness);

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

						for (const auto& obj : scene.renderObjects)
						{
							if (obj.material.alphaMode == AlphaMode::Opaque)
							{
								auto perObjectCB = passCtx.UpdateConstantBuffer(&obj.world, sizeof(PerObjectCB));
								passCtx.BindConstantBuffer(1, perObjectCB);
								passCtx.SetVertexBuffer(obj.vertexBuffer);
								passCtx.SetIndexBuffer(obj.indexBuffer);
								passCtx.DrawIndexed(obj.indexCount, obj.indexOffset, 0);
							}
						}
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

					for (int lightIdx = 0; lightIdx < fc.pointLightCount; lightIdx++)
					{
						const auto& pointLight = fc.pointLights[lightIdx];

						std::vector<const RenderObject*> visibleObjects;
						visibleObjects.reserve(scene.renderObjects.size());

						for (const auto& obj : scene.renderObjects)
						{
							if (obj.material.alphaMode != AlphaMode::Opaque)
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

							PointShadowConstants shadowConstants;
							shadowConstants.lightIdx = lightIdx;
							shadowConstants.faceIdx = face;
							passCtx.SetRootConstants(2, &shadowConstants, 2);

							for (const RenderObject* objPtr : visibleObjects)
							{
								const auto& obj = *objPtr;

								auto perObjectCB = passCtx.UpdateConstantBuffer(&obj.world, sizeof(PerObjectCB));
								passCtx.BindConstantBuffer(1, perObjectCB);

								if (obj.vertexBuffer.id != lastVB)
								{
									passCtx.SetVertexBuffer(obj.vertexBuffer);
									lastVB = obj.vertexBuffer.id;
								}
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
				passCtx.SetViewport(0, 0, (float)(m_width / 2), (float)(m_height / 2));
				passCtx.SetScissorRect(0, 0, (LONG)(m_width / 2), (LONG)(m_height / 2));

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
			{
				passCtx.SetComputePipeline(m_GTAOComputePipeline);
				passCtx.BindComputeConstantBuffer(0, fc.gtaoCB);
				passCtx.SetComputeDescriptorTable(1, device->GetUAVHandle(m_gtaoTexture));
				passCtx.SetComputeDescriptorTable(2, device->GetSRVHandle(m_depthTexture));
				passCtx.SetComputeDescriptorTable(3, device->GetSRVHandle(m_gbufferNormal));
				passCtx.Dispatch((m_width + 7) / 8, (m_height + 7) / 8, 1);
			}
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
				passCtx.SetViewport(0, 0, (float)(m_width / 2), (float)(m_height / 2));
				passCtx.SetScissorRect(0, 0, (LONG)(m_width / 2), (LONG)(m_height / 2));

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
				passCtx.SetViewport(0, 0, (float)(m_width / 2), (float)(m_height / 2));
				passCtx.SetScissorRect(0, 0, (LONG)(m_width / 2), (LONG)(m_height / 2));

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

				GTAOBilateralBlurCB cb{ {1.0f / m_width, 1.0f / m_height}, {1,0}, 0.1f, 32.0f, 6 };
				fc.gtaoBilateralBlurCB = passCtx.UpdateConstantBuffer(&cb, sizeof(GTAOBilateralBlurCB));
				passCtx.BindComputeConstantBuffer(0, fc.gtaoBilateralBlurCB);

				passCtx.SetComputeDescriptorTable(1, device->GetSRVHandle(m_gtaoTexture));
				passCtx.SetComputeDescriptorTable(2, device->GetSRVHandle(m_depthTexture));
				passCtx.SetComputeDescriptorTable(3, device->GetSRVHandle(m_gbufferNormal));
				passCtx.SetComputeDescriptorTable(4, device->GetUAVHandle(m_gtaoTempTexture));
				passCtx.Dispatch((m_width + 7) / 8, (m_height + 7) / 8, 1);
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

				GTAOBilateralBlurCB cb{ {1.0f / m_width, 1.0f / m_height}, {0,1}, 0.1f, 32.0f, 6 };
				fc.gtaoBilateralBlurCB = passCtx.UpdateConstantBuffer(&cb, sizeof(GTAOBilateralBlurCB));
				passCtx.BindComputeConstantBuffer(0, fc.gtaoBilateralBlurCB);

				passCtx.SetComputeDescriptorTable(1, device->GetSRVHandle(m_gtaoTempTexture));
				passCtx.SetComputeDescriptorTable(2, device->GetSRVHandle(m_depthTexture));
				passCtx.SetComputeDescriptorTable(3, device->GetSRVHandle(m_gbufferNormal));
				passCtx.SetComputeDescriptorTable(4, device->GetUAVHandle(m_gtaoTexture));
				passCtx.Dispatch((m_width + 7) / 8, (m_height + 7) / 8, 1);
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

			builder.Write(fc.backBuffer, RGResourceState::RenderTarget);
		},
		[this, &fc, device](CommandContext& passCtx) {
			passCtx.BeginTimestamp(PassID::PBRLightingPass);
			{
				passCtx.ClearRenderTarget(device->GetCurrentBackBuffer(), clearColor);
				passCtx.SetRenderTarget(1, device->GetCurrentBackBufferPtr(), {});
				passCtx.SetPipeline(m_PBRlightingPassPipeline);
				passCtx.SetViewport(0, 0, (float)m_width, (float)m_height);
				passCtx.SetScissorRect(0, 0, (LONG)m_width, (LONG)m_height);

				passCtx.BindConstantBuffer(0, fc.perFrameCB);
				passCtx.BindConstantBuffer(1, fc.lightCB);
				passCtx.BindConstantBuffer(2, fc.shadowCB);

				passCtx.BindTexture(3, m_depthTexture);
				passCtx.BindTexture(4, m_gbufferAlbedo);
				passCtx.BindTexture(5, m_gbufferNormal);
				passCtx.BindTexture(6, m_gbufferMR);
				passCtx.BindTexture(7, m_irradianceMapTexture);
				passCtx.BindTexture(8, m_prefilteredEnvMapTexture);
				passCtx.BindTexture(9, m_brdfLUTTexture);
				passCtx.BindTexture(10, m_shadowMapTexture);
				passCtx.BindTexture(11, m_ssaoTexture);
				passCtx.BindTexture(12, m_gtaoTexture);
				passCtx.BindTexture(13, m_pointShadowMapTextures[0]);

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
			builder.Write(fc.backBuffer, RGResourceState::RenderTarget);
		},
		[this, &fc, device](CommandContext& passCtx) {
			passCtx.BeginTimestamp(PassID::SkyboxPass);

			passCtx.SetRenderTarget(1, device->GetCurrentBackBufferPtr(), m_depthTexture);
			passCtx.SetPipeline(m_skyboxPipeline);
			passCtx.SetViewport(0, 0, (float)m_width, (float)m_height);
			passCtx.SetScissorRect(0, 0, (LONG)m_width, (LONG)m_height);

			passCtx.BindConstantBuffer(0, fc.perFrameCB);

			passCtx.BindTexture(1, m_cubemapTexture);

			passCtx.Draw(3, 0);

			passCtx.EndTimestamp(PassID::SkyboxPass);
		}
	);
}
void Renderer::AddDebugPass(GraphicsDevice* device, RenderGraph& graph, FrameContext& fc, const RenderScene& scene)
{
	graph.AddPass(
		"DebugPass",
		[&](RGBuilder& builder) {
			builder.Read(fc.depthTexture, RGResourceState::ShaderResource);
			builder.Read(fc.gbufferAlbedo, RGResourceState::ShaderResource);
			builder.Read(fc.gbufferNormal, RGResourceState::ShaderResource);
			builder.Read(fc.gbufferMR, RGResourceState::ShaderResource);

			builder.Write(fc.backBuffer, RGResourceState::RenderTarget);
		},
		[this, &fc, device](CommandContext& passCtx) {

			passCtx.ClearRenderTarget(device->GetCurrentBackBuffer(), clearColor);
			passCtx.SetRenderTarget(1, device->GetCurrentBackBufferPtr(), {});

			if (debugMode == DebugMode::DepthTexture)
			{
				passCtx.SetPipeline(m_depthDebugPipeline);
				passCtx.BindTexture(0, m_depthTexture);
			}
			else if (debugMode == DebugMode::Albedo)
			{
				passCtx.SetPipeline(m_debugPipeline);
				passCtx.BindTexture(0, m_gbufferAlbedo);
			}
			else if (debugMode == DebugMode::Normal)
			{
				passCtx.SetPipeline(m_debugPipeline);
				passCtx.BindTexture(0, m_gbufferNormal);
			}
			else if (debugMode == DebugMode::MR)
			{
				passCtx.SetPipeline(m_debugPipeline);
				passCtx.BindTexture(0, m_gbufferMR);
			}
			else if (debugMode == DebugMode::BRDF_LUT)
			{
				passCtx.SetPipeline(m_debugPipeline);
				passCtx.BindTexture(0, m_brdfLUTTexture);
			}
			else if (debugMode == DebugMode::IrradianceMap)
			{
				passCtx.SetPipeline(m_debugPipeline);
				passCtx.BindTexture(0, m_irradianceMapTexture);
			}
			else if (debugMode == DebugMode::PreFilteredEnvrionmentMap)
			{
				passCtx.SetPipeline(m_debugPipeline);
				passCtx.BindTexture(0, m_prefilteredEnvMapTexture);
			}
			passCtx.SetViewport(0, 0, (float)m_width, (float)m_height);
			passCtx.SetScissorRect(0, 0, (LONG)m_width, (LONG)m_height);

			passCtx.Draw(3, 0);
		}
	);
}

