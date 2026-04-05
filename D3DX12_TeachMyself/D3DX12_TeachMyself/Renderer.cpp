#include "Renderer.h"
#include "ShaderCompiler.h"
#include "CommandContext.h"
#include "GPUTimestampProfiler.h"
#include <chrono>
#include "AssetLoader.h"
#include "MokoLog.h"
#include "MokoTime.h"

void Renderer::Init(GraphicsDevice* device)
{
	m_width = device->GetWidth();
	m_height = device->GetHeight();

	ShaderCompiler::Reserve(50);

	InitDepthPrePass(device);
	InitGBufferPass(device);
	InitLightingPass(device);
	InitPBRLightingPass(device);
	InitSkyboxPass(device);

	InitDebugPass(device);


	// Equirect -> CubeMap
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
	float* hdrData = loader.LoadHDR("../citrus_orchard_puresky_4k.hdr", w, h, ch, 4);
	//float* hdrData = loader.LoadHDR("../venice_sunset_4k.hdr", w, h, ch, 4);

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
	// Equirect -> CubeMap

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
			ctx.SetComputeDescriptorTable(0, device->GetUAVHeapSlot(m_brdfLUTTexture));
			ctx.Dispatch(512 / 16, 512 / 16, 1);

			ctx.TransitionBarrier(m_brdfLUTTexture, RGResourceState::UnorderedAccess, RGResourceState::ShaderResource);
		}
	);
	// BRDF LUT

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
	irradianceCubeMapDesc.height = 32;
	irradianceCubeMapDesc.format = Format::R16G16B16A16_FLOAT;
	m_irradianceMapTexture = device->CreateCubemapTexture(irradianceCubeMapDesc);

	device->ExecuteImmediate(
		[&](CommandContext& ctx) {
			ctx.SetComputePipeline(m_irradianceMapCSPipeline);
			ctx.SetComputeDescriptorTable(0, device->GetSRVHeapSlot(m_cubemapTexture));
			ctx.SetComputeDescriptorTable(1, device->GetUAVHeapSlot(m_irradianceMapTexture));
			ctx.Dispatch(32 / 8, 32 / 8, 6);

			ctx.TransitionBarrier(m_irradianceMapTexture, RGResourceState::UnorderedAccess, RGResourceState::ShaderResource);
		}
	);
	// Irradiance Map

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
	prefiltredEnvironmentCubeMapDesc.format = Format::R16G16B16A16_FLOAT;
	prefiltredEnvironmentCubeMapDesc.mipLevels = mipLevels;
	m_prefilteredMapTexture = device->CreateCubemapTexture(prefiltredEnvironmentCubeMapDesc);

	device->ExecuteImmediate(
		[&](CommandContext& ctx) {
			ctx.SetComputePipeline(m_prefilteredEnvironmentMapCSPipeline);
			ctx.SetComputeDescriptorTable(0, device->GetSRVHeapSlot(m_cubemapTexture));

			for (uint32_t mip = 0; mip < mipLevels; mip++)
			{
				float roughness = (float)mip / (float(mipLevels - 1));
				uint32_t mipSize = 512 >> mip;

				ctx.SetComputeDescriptorTable(1, device->GetUAVHeapSlot(m_prefilteredMapTexture, mip));
				ctx.SetComputeRootConstants(2, &roughness, 1);
				ctx.Dispatch(
					std::max(mipSize / 8u, 1u),
					std::max(mipSize / 8u, 1u),
					6
				);
			}

			ctx.TransitionBarrier(m_prefilteredMapTexture, RGResourceState::UnorderedAccess, RGResourceState::ShaderResource);
		}
	);
	// PreFiltered Environment Map

	// Shadow Map
	std::vector<VertexAttribute> vertexAttributes;
	vertexAttributes.push_back({ Semantic::POSITION, Format::R32G32B32_FLOAT, 0 });
	vertexAttributes.push_back({ Semantic::NORMAL,   Format::R32G32B32_FLOAT, 0 });
	vertexAttributes.push_back({ Semantic::TANGENT, Format::R32G32B32A32_FLOAT, 0 });
	vertexAttributes.push_back({ Semantic::TEXCOORD, Format::R32G32_FLOAT, 0 });

	RootSignatureDesc shadowMapRSDesc = {};
	shadowMapRSDesc.allowIA = true;
	shadowMapRSDesc.rootParamDescs.push_back({ RootParamType::RootCBV, RangeType::CBV, 0, 1, ShaderVisibility::Vertex });
	shadowMapRSDesc.rootParamDescs.push_back({ RootParamType::RootCBV, RangeType::CBV, 1, 1, ShaderVisibility::Vertex });

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
	m_shadowMap = device->CreateTexture(shadowMapDesc, nullptr);
	// Shadow Map


	debugMode = DebugMode::PBR_Enabled;
}

void Renderer::Render(GraphicsDevice* device, const Scene& scene)
{
	ReloadPSO(device);

	if (m_needsResize)
		Resize(device);

	CommandContext& ctx = device->BeginFrame();

	RenderGraph graph(device);

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

	RGResourceDesc skyboxTextureDesc = { m_width, m_height, Format::R16G16B16A16_FLOAT, TextureUsage::ShaderResource };
	RGResourceHandle skyboxTexture = graph.ImportTexture(m_cubemapTexture, skyboxTextureDesc, RGResourceState::ShaderResource);

	RGResourceDesc irradiacneMapDesc = { m_width, m_height, Format::R16G16B16A16_FLOAT, TextureUsage::ShaderResource };
	RGResourceHandle irradiacneMap = graph.ImportTexture(m_irradianceMapTexture, irradiacneMapDesc, RGResourceState::ShaderResource);

	RGResourceDesc prefilteredEnvMapDesc = { m_width, m_height, Format::R16G16B16A16_FLOAT, TextureUsage::ShaderResource };
	RGResourceHandle prefilteredEnvMap = graph.ImportTexture(m_prefilteredMapTexture, prefilteredEnvMapDesc, RGResourceState::ShaderResource);

	RGResourceDesc brdfLUTDesc = { m_width, m_height, Format::R16G16B16A16_FLOAT, TextureUsage::ShaderResource };
	RGResourceHandle brdfLUT = graph.ImportTexture(m_brdfLUTTexture, brdfLUTDesc, RGResourceState::ShaderResource);

	RGResourceDesc shadowMapDesc = { 4096, 4096, Format::R32_TYPELESS, TextureUsage::DepthStencil };
	RGResourceHandle shadowMap = graph.ImportTexture(m_shadowMap, shadowMapDesc, RGResourceState::DepthWrite);

	PerFrameCB perFrame;
	XMMATRIX vp = scene.cam.GetViewMatrix() * scene.cam.GetProjectionMatrix();
	XMStoreFloat4x4(&perFrame.ViewProj, XMMatrixTranspose(vp));
	XMMATRIX inverseVP = XMMatrixInverse(nullptr, vp);
	XMStoreFloat4x4(&perFrame.InvViewProj, inverseVP);
	perFrame.CameraPos = scene.cam.GetPos();
	XMStoreFloat2(&perFrame.ScreenSize, { static_cast<float>(m_width), static_cast<float>(m_height) });
	
	static float time = 0.0f;
	time += MokoTime::GetDeltaTime();

	float sunSpeed = 0.2f;
	float angle = sinf(time * sunSpeed) * 0.5f; 
	XMVECTOR lightDir = DirectX::XMVector3Normalize(
		DirectX::XMVectorSet(
			0.0f,   
			-1.0f,   
			angle,
			0.0f
		)
	);

	//XMVECTOR lightDir = XMVector3Normalize(XMVectorSet(0.0f, -0.5f, 0.5f, 0.0f));

	LightCB light;
	XMStoreFloat3(&light.Direction, lightDir);
	XMStoreFloat3(&light.Color, XMVectorSet(2.0f, 1.8f, 1.5f, 0.0f));
	XMStoreFloat3(&light.Ambient, { 0.07f, 0.07f, 0.07f });
	light.Intensity = 1.1f;


	XMVECTOR targetPos = XMVectorSet(0, 0, 0, 0); 
	XMVECTOR lightPos = XMVectorSubtract(targetPos, XMVectorScale(lightDir, 50.0f));

	XMMATRIX lightView = XMMatrixLookAtLH(lightPos, targetPos, XMVectorSet(0, 1, 0, 0));

	XMMATRIX lightProj = XMMatrixOrthographicLH(100.0f, 100.0f, 1.0f, 300.0f);

	XMMATRIX lightVP = lightView * lightProj;

	ShadowCB shadow;

	FrameContext fc = {
		backBuffer, gbufferAlbedo, gbufferNormal, gbufferMR,
		depthTexture, skyboxTexture,
		irradiacneMap, prefilteredEnvMap, brdfLUT, shadowMap,
		{}, {}, {}, {}
	};

	// Shadow Cascade Map
	float cascadeSplits[CASCADE_COUNT + 1];
	float nearClip = 0.1f;
	float farClip = 100.0f;
	float lambda = 0.5f;

	for (int i = 0; i <= CASCADE_COUNT; i++)
	{
		float t = (float)i / CASCADE_COUNT;
		float log = nearClip * powf(farClip / nearClip, t);
		float linear = nearClip + (farClip - nearClip) * t;
		cascadeSplits[i] = lambda * log + (1.0f - lambda) * linear;
	}

	XMVECTOR ndcCorners[8] = {
		{-1, -1, 0, 1}, {-1,  1, 0, 1}, { 1,  1, 0, 1}, { 1, -1, 0, 1},
		{-1, -1, 1, 1}, {-1,  1, 1, 1}, { 1,  1, 1, 1}, { 1, -1, 1, 1},
	};

	XMMATRIX invVP = XMLoadFloat4x4(&perFrame.InvViewProj);

	for (int c = 0; c < CASCADE_COUNT; c++)
	{
		float cNear = cascadeSplits[c];
		float cFar = cascadeSplits[c + 1];

		XMVECTOR worldCorners[8];
		for (int i = 0; i < 8; i++)
			worldCorners[i] = XMVector4Transform(ndcCorners[i], invVP);
		for (int i = 0; i < 8; i++)
			worldCorners[i] = XMVectorDivide(worldCorners[i], XMVectorSplatW(worldCorners[i]));

		float nearT = (cNear - nearClip) / (farClip - nearClip);
		float farT = (cFar - nearClip) / (farClip - nearClip);

		XMVECTOR cascadeCorners[8];
		for (int i = 0; i < 4; i++)
		{
			XMVECTOR dir = XMVectorSubtract(worldCorners[i + 4], worldCorners[i]);
			cascadeCorners[i] = XMVectorAdd(worldCorners[i], XMVectorScale(dir, nearT));
			cascadeCorners[i + 4] = XMVectorAdd(worldCorners[i], XMVectorScale(dir, farT));
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
			XMFLOAT3 p; XMStoreFloat3(&p, v);
			mins.x = min(mins.x, p.x); maxs.x = std::max(maxs.x, p.x);
			mins.y = min(mins.y, p.y); maxs.y = std::max(maxs.y, p.y);
			mins.z = min(mins.z, p.z); maxs.z = std::max(maxs.z, p.z);
		}

		float sceneZMin = FLT_MAX;
		XMFLOAT3 sceneCorners[8] = {
			{ scene.sceneAABBMin.x, scene.sceneAABBMin.y, scene.sceneAABBMin.z },
			{ scene.sceneAABBMax.x, scene.sceneAABBMin.y, scene.sceneAABBMin.z },
			{ scene.sceneAABBMin.x, scene.sceneAABBMax.y, scene.sceneAABBMin.z },
			{ scene.sceneAABBMax.x, scene.sceneAABBMax.y, scene.sceneAABBMin.z },
			{ scene.sceneAABBMin.x, scene.sceneAABBMin.y, scene.sceneAABBMax.z },
			{ scene.sceneAABBMax.x, scene.sceneAABBMin.y, scene.sceneAABBMax.z },
			{ scene.sceneAABBMin.x, scene.sceneAABBMax.y, scene.sceneAABBMax.z },
			{ scene.sceneAABBMax.x, scene.sceneAABBMax.y, scene.sceneAABBMax.z },
		};
		for (int i = 0; i < 8; i++)
		{
			XMVECTOR v = XMVector4Transform(XMLoadFloat3(&sceneCorners[i]), lightView);
			XMFLOAT3 p; XMStoreFloat3(&p, v);
			sceneZMin = min(sceneZMin, p.z);
		}
		mins.z = sceneZMin;

		float cascadeWidth = maxs.x - mins.x;
		float cascadeHeight = maxs.y - mins.y;

		float texelSizeX = cascadeWidth / (float)2048;
		float texelSizeY = cascadeHeight / (float)2048;

		mins.x = floor(mins.x / texelSizeX) * texelSizeX;
		maxs.x = floor(maxs.x / texelSizeX) * texelSizeX;
		mins.y = floor(mins.y / texelSizeY) * texelSizeY;
		maxs.y = floor(maxs.y / texelSizeY) * texelSizeY;

		XMMATRIX lightProj = XMMatrixOrthographicOffCenterLH(
			mins.x, maxs.x, mins.y, maxs.y, mins.z, maxs.z);

		fc.LightViewProj[c] = lightView * lightProj;
	}
	// Shadow Cascade Map


	CBHandle perFrameCBHandle;
	CBHandle lightCBHandle;
	CBHandle shadowCBHandle;


	graph.AddPass(
		"CB UpdatePass",
		[&](RGBuilder& builder) {},
		[&](CommandContext& passCtx) {
			fc.perFrameCB = passCtx.UpdateConstantBuffer(0, &perFrame, sizeof(PerFrameCB));
			fc.lightCB = passCtx.UpdateConstantBuffer(1, &light, sizeof(LightCB));
			ShadowCB shadow;
			for (int c = 0; c < CASCADE_COUNT; c++)
			{
				XMStoreFloat4x4(&shadow.LightViewProj[c], XMMatrixTranspose(fc.LightViewProj[c]));
				shadow.cascadeSplits[c] = cascadeSplits[c + 1];
			}
			fc.shadowCB = passCtx.UpdateConstantBuffer(2, &shadow, sizeof(ShadowCB));
		}
	);

	AddDepthPrePass(device, graph, fc, scene);
	AddGBufferPass(device, graph, fc, scene);

	graph.AddPass(
		"ShadowMapPass",
		[&](RGBuilder& builder) {
			builder.Write(fc.shadowMap, RGResourceState::DepthWrite);
		},
		[this, &fc, device, &scene](CommandContext& passCtx) {
			{
				passCtx.ClearDepthStencil(m_shadowMap, 1.0f);
				passCtx.SetRenderTarget(0, {}, m_shadowMap);
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
					auto cascadeCB = passCtx.UpdateConstantBuffer(0, &transposed, sizeof(XMFLOAT4X4));
					passCtx.BindConstantBuffer(0, cascadeCB);

					for (const auto& obj : scene.renderObjects)
					{
						if (obj.material.alphaMode == AlphaMode::Opaque)
						{
							auto perObjectCB = passCtx.UpdateConstantBuffer(1, &obj.world, sizeof(obj.world));
							passCtx.BindConstantBuffer(1, perObjectCB);
							passCtx.SetVertexBuffer(obj.vertexBuffer);
							passCtx.SetIndexBuffer(obj.indexBuffer);
							passCtx.DrawIndexed(obj.indexCount, obj.indexOffset, 0);
						}
					}
				}
			}
		}
	);

	if (debugMode == DebugMode::PBR_Enabled)
	{
		AddPBRLightingPass(device, graph, fc, scene);
	}
	else if (debugMode == DebugMode::PBR_Disabled)
	{
		AddLightingPass(device, graph, fc, scene);
	}
	else
	{
		AddDebugPass(device, graph, fc, scene);
	}

	AddSkyboxPass(device, graph, fc, scene);

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

	LOG_INFO(
		"[GPU Time] | DepthPre: %6.3f ms | GBuffer: %6.3f ms | GBufferAlpha: %6.3f ms | PBRLighting: %6.3f ms | Skybox: %6.3f ms\n",
		depthPrePassTime,
		gBufferPassTime,
		gBufferAlphaPassTime,
		PBRlightingPassTime,
		skyboxPassTime);
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
void Renderer::InitLightingPass(GraphicsDevice* device)
{
	RootSignatureDesc lightingPassRSDesc = {};
	lightingPassRSDesc.rootParamDescs.push_back({ RootParamType::RootCBV, RangeType::CBV, 0, 1, ShaderVisibility::Pixel });
	lightingPassRSDesc.rootParamDescs.push_back({ RootParamType::RootCBV, RangeType::CBV, 1, 1, ShaderVisibility::Pixel });
	lightingPassRSDesc.rootParamDescs.push_back({ RootParamType::DescriptorTable, RangeType::SRV, 0, 1, ShaderVisibility::Pixel });
	lightingPassRSDesc.rootParamDescs.push_back({ RootParamType::DescriptorTable, RangeType::SRV, 1, 1, ShaderVisibility::Pixel });
	lightingPassRSDesc.rootParamDescs.push_back({ RootParamType::DescriptorTable, RangeType::SRV, 2, 1, ShaderVisibility::Pixel });
	lightingPassRSDesc.rootParamDescs.push_back({ RootParamType::DescriptorTable, RangeType::SRV, 3, 1, ShaderVisibility::Pixel });
	lightingPassRSDesc.staticSamplers.push_back({ SamplerFilter::Point, SamplerAddressMode::Clamp, 0, ShaderVisibility::Pixel });

	std::vector<VertexAttribute> vertexAttributes;
	vertexAttributes.push_back({ Semantic::POSITION, Format::R32G32B32_FLOAT, 0 });
	vertexAttributes.push_back({ Semantic::NORMAL,   Format::R32G32B32_FLOAT, 0 });
	vertexAttributes.push_back({ Semantic::TANGENT, Format::R32G32B32A32_FLOAT, 0 });
	vertexAttributes.push_back({ Semantic::TEXCOORD, Format::R32G32_FLOAT, 0 });

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


void Renderer::AddDepthPrePass(GraphicsDevice* device, RenderGraph& graph, FrameContext& fc, const Scene& scene)
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
}
void Renderer::AddGBufferPass(GraphicsDevice* device, RenderGraph& graph, FrameContext& fc, const Scene& scene)
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

						auto perObjectCBHandle = passCtx.UpdateConstantBuffer(1, &obj.world, sizeof(obj.world));
						passCtx.SetVertexBuffer(obj.vertexBuffer);
						passCtx.SetIndexBuffer(obj.indexBuffer);

						passCtx.BindConstantBuffer(1, perObjectCBHandle);
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

						auto perObjectCBHandle = passCtx.UpdateConstantBuffer(1, &obj.world, sizeof(obj.world));

						passCtx.SetVertexBuffer(obj.vertexBuffer);
						passCtx.SetIndexBuffer(obj.indexBuffer);

						passCtx.BindConstantBuffer(1, perObjectCBHandle);
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
}
void Renderer::AddLightingPass(GraphicsDevice* device, RenderGraph& graph, FrameContext& fc, const Scene& scene)
{
	graph.AddPass(
		"LightingPass",
		[&](RGBuilder& builder) {
			builder.Read(fc.depthTexture, RGResourceState::ShaderResource);
			builder.Read(fc.gbufferAlbedo, RGResourceState::ShaderResource);
			builder.Read(fc.gbufferNormal, RGResourceState::ShaderResource);
			builder.Read(fc.gbufferMR, RGResourceState::ShaderResource);
			builder.Write(fc.backBuffer, RGResourceState::RenderTarget);
		},
		[this, &fc, device](CommandContext& passCtx) {
			{
				passCtx.ClearRenderTarget(device->GetCurrentBackBuffer(), clearColor);
				passCtx.SetRenderTarget(1, device->GetCurrentBackBufferPtr(), {});
				passCtx.SetPipeline(m_lightingPassPipeline);
				passCtx.SetViewport(0, 0, (float)m_width, (float)m_height);
				passCtx.SetScissorRect(0, 0, (LONG)m_width, (LONG)m_height);

				passCtx.BindConstantBuffer(0, fc.perFrameCB);
				passCtx.BindConstantBuffer(1, fc.lightCB);
				passCtx.BindTexture(m_depthTexture, 2);
				passCtx.BindTexture(m_gbufferAlbedo, 3);
				passCtx.BindTexture(m_gbufferNormal, 4);
				passCtx.BindTexture(m_gbufferMR, 5);

				passCtx.Draw(3, 0);
			}
		}
	);
}
void Renderer::AddPBRLightingPass(GraphicsDevice* device, RenderGraph& graph, FrameContext& fc, const Scene& scene)
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

				passCtx.BindTexture(m_depthTexture, 3);
				passCtx.BindTexture(m_gbufferAlbedo, 4);
				passCtx.BindTexture(m_gbufferNormal, 5);
				passCtx.BindTexture(m_gbufferMR, 6);
				passCtx.BindTexture(m_irradianceMapTexture, 7);
				passCtx.BindTexture(m_prefilteredMapTexture, 8);
				passCtx.BindTexture(m_brdfLUTTexture, 9);
				passCtx.BindTexture(m_shadowMap, 10);

				passCtx.Draw(3, 0);
			}
			passCtx.EndTimestamp(PassID::PBRLightingPass);
		}
	);
}
void Renderer::AddSkyboxPass(GraphicsDevice* device, RenderGraph& graph, FrameContext& fc, const Scene& scene)
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

			passCtx.BindTexture(m_cubemapTexture, 1);

			passCtx.Draw(3, 0);

			passCtx.EndTimestamp(PassID::SkyboxPass);
		}
	);
}
void Renderer::AddDebugPass(GraphicsDevice* device, RenderGraph& graph, FrameContext& fc, const Scene& scene)
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
			else if (debugMode == DebugMode::BRDF_LUT)
			{
				passCtx.SetPipeline(m_debugPipeline);
				passCtx.BindTexture(m_brdfLUTTexture, 0);
			}
			else if (debugMode == DebugMode::IrradianceMap)
			{
				passCtx.SetPipeline(m_debugPipeline);
				passCtx.BindTexture(m_irradianceMapTexture, 0);
			}
			else if (debugMode == DebugMode::PreFilteredEnvrionmentMap)
			{
				passCtx.SetPipeline(m_debugPipeline);
				passCtx.BindTexture(m_prefilteredMapTexture, 0);
			}
			passCtx.SetViewport(0, 0, (float)m_width, (float)m_height);
			passCtx.SetScissorRect(0, 0, (LONG)m_width, (LONG)m_height);

			passCtx.Draw(3, 0);
		}
	);
}