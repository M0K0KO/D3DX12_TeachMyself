#pragma once
#include "RHITypes.h"
#include "GraphicsDevice.h"
#include "ShaderCompiler.h"
#include "RenderGraph.h"
#include "RenderScene.h"
#include "FrameData.h"
#include "AssetManager.h"

struct DebugLineVertex
{
	XMFLOAT3 position;
	XMFLOAT4 color;
};

enum class DebugViewMode : uint32_t
{
	SceneColor = 0,
	Albedo,
	Normal,
	MetallicRoughness,
	Emissive,
	Depth,
	AO,
	Count
};	

struct FrameContext
{
	RGTextureHandle backBuffer;
	RGTextureHandle gbufferAlbedo;
	RGTextureHandle gbufferNormal;
	RGTextureHandle gbufferMR;
	RGTextureHandle gbufferEmissive;
	RGTextureHandle depthTexture;
	RGTextureHandle skyboxTexture;
	RGTextureHandle irradianceMap;
	RGTextureHandle prefilteredEnvMap;
	RGTextureHandle brdfLutTexture;
	RGTextureHandle shadowMap;
	std::vector<RGTextureHandle> pointShadowMaps;
	int pointLightCount = 0;
	std::vector<PointLightData> pointLights;
	RGTextureHandle ssaoTexture;
	RGTextureHandle ssaoNoiseTexture;
	RGTextureHandle ssaoTempTexture;
	RGTextureHandle gtaoTexture;
	RGTextureHandle gtaoTempTexture;
	RGTextureHandle scenecolor;
	RGTextureHandle sceneColorLDR;

	RGBufferHandle depthArgBuffer;
	RGBufferHandle gbufferArgBuffer;
	RGBufferHandle directionalShadowArgBuffer;
	RGBufferHandle pointShadowArgBuffer;

	CBHandle perFrameCB;
	CBHandle lightCB;
	CBHandle shadowCB;
	CBHandle pointShadowCB;
	XMMATRIX LightViewProj[CASCADE_COUNT];

	CBHandle ssaoCB;
	CBHandle bilateralBlurCB;
	CBHandle gtaoCB;
	CBHandle gtaoBilateralBlurCB;
};

class Renderer
{
public:
	void Init(GraphicsDevice* device, AssetManager* assetManager);
	void Render(GraphicsDevice* device, CommandContext& ctx, const RenderScene& renderScene);
	void Shutdown();

	void OnViewportResize(uint32_t width, uint32_t height);

	DebugViewMode GetDebugViewMode();
	void SetDebugViewMode(DebugViewMode viewMode);

	void DrawAABB(const XMFLOAT3& mn, const XMFLOAT3& mx, const XMFLOAT4& color);
	bool GetShowAABB();
	void SetShowAABB(bool value);

	GPUTextureHandle GetSceneColorLDR() const	{ return m_sceneColorLDRTexture; };

private:
	void ReloadPSO(GraphicsDevice* device);
	void Resize(GraphicsDevice* device);

	void CreateCubeMap(GraphicsDevice* device);
	void CreateIrradianceMap(GraphicsDevice* device);
	void CreatePrefilteredEnvironmentMap(GraphicsDevice* device);
	void CreateBRDFLUT(GraphicsDevice* device);

	void CollectDebugLines(const RenderScene& scene);

	void BuildCascadeShadowMatrices(
		const XMMATRIX& invViewProj,
		FXMVECTOR lightDir,
		const XMFLOAT3& sceneAABBMin,
		const XMFLOAT3& sceneAABBMax,
		float nearClip,
		float farClip,
		float cascadeSplits[CASCADE_COUNT + 1],
		XMMATRIX outLightViewProj[CASCADE_COUNT]);

	FrameContext BuildFrameContext(GraphicsDevice* device, CommandContext& ctx, RenderGraph& graph, const RenderScene& scene);

	void UpdateIndirectArgBuffers(GraphicsDevice* device, const RenderScene& scene);

	void BuildSceneGraph(GraphicsDevice* device, RenderGraph& graph, FrameContext& fc, const RenderScene& scene);
	void BuildPresentGraph(GraphicsDevice* device, RenderGraph& graph, FrameContext& fc);
	void BuildDebugGraph(GraphicsDevice* device, RenderGraph& graph, FrameContext& fc);

	void InitDepthPrePass(GraphicsDevice* device);
	void InitGBufferPass(GraphicsDevice* device);
	void InitDirectionalShadowPass(GraphicsDevice* device);
	void InitPointShadowPass(GraphicsDevice* device);
	void InitSSAOPass(GraphicsDevice* device);
	void InitGTAOPass(GraphicsDevice* device);
	void InitSSAOBilateralBlurPass(GraphicsDevice* device);
	void InitGTAOBilateralBlurPass(GraphicsDevice* device);
	void InitPBRLightingPass(GraphicsDevice* device);
	void InitSkyboxPass(GraphicsDevice* device);
	void InitPresentPass(GraphicsDevice* device);
	void InitDebugLinePass(GraphicsDevice* device);

	void AddDepthPrePass(GraphicsDevice* device, RenderGraph& graph, FrameContext& fc, const RenderScene& scene);
	void AddGBufferPass(GraphicsDevice* device, RenderGraph& graph, FrameContext& fc, const RenderScene& scene);
	void AddDirectionalShadowPass(GraphicsDevice* device, RenderGraph& graph, FrameContext& fc, const RenderScene& scene);
	void AddPointShadowPass(GraphicsDevice* device, RenderGraph& graph, FrameContext& fc, const RenderScene& scene);
	void AddSSAOPass(GraphicsDevice* device, RenderGraph& graph, FrameContext& fc, const RenderScene& scene);
	void AddGTAOPass(GraphicsDevice* device, RenderGraph& graph, FrameContext& fc, const RenderScene& scene);
	void AddSSAOBilateralBlurPass(GraphicsDevice* device, RenderGraph& graph, FrameContext& fc, const RenderScene& scene);
	void AddGTAOBilateralBlurPass(GraphicsDevice* device, RenderGraph& graph, FrameContext& fc, const RenderScene& scene);
	void AddPBRLightingPass(GraphicsDevice* device, RenderGraph& graph, FrameContext& fc, const RenderScene& scene);
	void AddSkyboxPass(GraphicsDevice* device, RenderGraph& graph, FrameContext& fc, const RenderScene& scene);
	void AddPresentPass(GraphicsDevice* device, RenderGraph& graph, FrameContext& fc);
	void AddDebugLinePass(GraphicsDevice* device, RenderGraph& graph, FrameContext& fc);
	
private:
	AssetManager* m_assetManager;
	GPUBufferHandle m_transformBuffer;
	static constexpr uint32_t MAX_TRANSFORMS = 4096;

	DebugViewMode m_debugViewMode = DebugViewMode::SceneColor;

	GPUBufferHandle m_debugLineBuffer;
	static constexpr uint32_t MAX_DEBUG_LINES = 16384;
	std::vector<DebugLineVertex> m_debugLines;

	bool m_showAABB = false;

private:
	static constexpr float clearColor[4] = { 0.0f, 0.0f,  0.0f,  0.0f };
	std::vector<XMFLOAT4> hemisphereSamples;

	uint32_t m_viewportWidth;
	uint32_t m_viewportHeight;

	ShaderHandle m_fullscreenVS;

	ShaderHandle m_depthVS;
	PipelineDesc m_depthPrePassPipelinDesc;
	PipelineHandle m_depthPrePassPipeline;

	ShaderHandle m_gBufferVS;
	ShaderHandle m_gBufferOpaquePS;
	ShaderHandle m_gBufferAlphaPS;
	PipelineDesc m_gBufferOpaquePassPipelineDesc;
	PipelineDesc m_gBufferAlphaPassPipelineDesc;
	PipelineHandle m_gBufferOpaquePassPipeline;
	PipelineHandle m_gBufferAlphaPassPipeline;

	ShaderHandle m_PBRlightingPS;
	PipelineDesc m_PBRlightingPassPipelineDesc;
	PipelineHandle m_PBRlightingPassPipeline;

	ShaderHandle m_depthDebugPS;
	PipelineDesc m_depthDebugPipelineDesc;
	PipelineHandle m_depthDebugPipeline;

	ShaderHandle m_equirectConvertCS;
	PipelineHandle m_equirectCSPipeline;

	ShaderHandle m_skyboxPS;
	PipelineDesc m_skyboxPipelineDesc;
	PipelineHandle m_skyboxPipeline;

	ShaderHandle m_brdfLUTCS;
	PipelineHandle m_brdfLUTCSPipeline;

	ShaderHandle m_irradianceMapCS;
	PipelineHandle m_irradianceMapCSPipeline;

	ShaderHandle m_prefilteredEnvironmentMapCS;
	PipelineHandle m_prefilteredEnvironmentMapCSPipeline;

	ShaderHandle m_shadowMapVS;
	PipelineDesc m_shadowMapPipelineDesc;
	PipelineHandle m_shadowMapPipeline;

	ShaderHandle m_pointShadowMapVS;
	PipelineDesc m_pointShadowMapPipelineDesc;
	PipelineHandle m_pointShadowMapPipeline;

	ShaderHandle m_ssaoVS;
	ShaderHandle m_ssaoPS;
	PipelineDesc m_SSAOPipelineDesc;
	PipelineHandle m_SSAOPipeline;

	ShaderHandle m_GTAOCS;
	ComputePipelineDesc m_GTAOComputePipelineDesc;
	PipelineHandle m_GTAOComputePipeline;

	ShaderHandle m_bilateralBlurVS;
	ShaderHandle m_bilateralBlurPS_Vertical;
	ShaderHandle m_bilateralBlurPS_Horizontal;
	PipelineDesc m_bilateralBlurPipelineDesc;
	PipelineHandle m_bilateralBlurPipeline_Vertical;
	PipelineHandle m_bilateralBlurPipeline_Horizontal;

	ShaderHandle m_bilateralBlurCS;
	ComputePipelineDesc m_bilateralBlurComputePipelineDesc;
	PipelineHandle m_bilateralBlurComputePipeline;

	ShaderHandle m_presentPS;
	PipelineDesc m_presentPipelineDesc;
	PipelineHandle m_presentPipeline;

	ShaderHandle m_debugLineVS;
	ShaderHandle m_debugLinePS;
	PipelineHandle m_debugLinePipeline;




	GPUBufferHandle m_perFrameCB;
	GPUBufferHandle m_perObjectCB;
	GPUBufferHandle m_lightingDataCB;

	GPUTextureHandle m_depthTexture;

	GPUTextureHandle m_gbufferAlbedo;
	GPUTextureHandle m_gbufferNormal;
	GPUTextureHandle m_gbufferMR;
	GPUTextureHandle m_gbufferEmissive;

	GPUTextureHandle m_equirectTexture;
	GPUTextureHandle m_cubemapTexture;

	GPUTextureHandle m_brdfLUTTexture;

	GPUTextureHandle m_irradianceMapTexture;

	GPUTextureHandle m_prefilteredEnvMapTexture;

	GPUTextureHandle m_shadowMapTexture;

	GPUTextureHandle m_defaultCubemapTexture;

	std::vector<GPUTextureHandle> m_pointShadowMapTextures;

	GPUTextureHandle m_ssaoTexture;
	GPUTextureHandle m_ssaoNoiseTexture;
	GPUTextureHandle m_ssaoTempTexture;

	GPUTextureHandle m_gtaoTexture;
	GPUTextureHandle m_gtaoTempTexture;

	GPUTextureHandle m_sceneColorTexture;
	GPUTextureHandle m_sceneColorLDRTexture;

	struct PerFrameCB
	{
		XMFLOAT4X4 ViewProj;
		XMFLOAT4X4 InvViewProj;
		XMFLOAT3 CameraPos;
		float padding0;

		XMFLOAT2 ScreenSize;
		XMFLOAT2 padding1;
	};
	PerFrameCB m_perFrameCBData;

	struct PerObjectCB
	{
		XMFLOAT4X4 World;
		XMFLOAT4X4 WorldInvTranspose;
	};
	PerObjectCB m_perObjectCBData;


	struct PointLight
	{
		XMFLOAT3 Position;
		float Radius;
		XMFLOAT3 Color;
		float Intensity;
	};

	struct LightCB
	{
		XMFLOAT3 Direction;
		float padding0;

		XMFLOAT3 Color;
		float Intensity;

		float Ambient;
		int PointLightCount;
		float padding1[2];

		PointLight PointLights[MAX_POINT_LIGHTS];
	};

	struct ShadowCB
	{
		XMFLOAT4X4 LightViewProj[CASCADE_COUNT];
		float cascadeSplits[CASCADE_COUNT];
	};
	ShadowCB m_shadowCB;


	struct PointShadowData
	{
		XMFLOAT4X4 FaceVP[6];
		XMFLOAT3 LightPos;
		float LightRadius;
	};
	struct PointShadowCB
	{
		PointShadowData pointShadowData[MAX_POINT_LIGHTS];
	};

	struct PointShadowConstants
	{
		uint32_t lightIdx;
		uint32_t faceIdx;
	};

	struct SSAOCB
	{
		XMFLOAT4X4 ViewMatrix;
		XMFLOAT4X4 ProjMatrix;
		XMFLOAT4X4 InvProjMatrix;

		float SampleRadius;
		float Bias;
		float Power;
		int KernelSize;

		XMFLOAT2 NoiseScale;
		XMFLOAT2 padding;

		XMFLOAT4 Samples[32];
	};

	struct GTAOCB
	{
		XMFLOAT4X4 View;
		XMFLOAT4X4 InvProj;
		XMFLOAT2 InvRes;
		float Radius;
		float FalloffStart;
		float FalloffEnd;
		UINT NumSlices;
		UINT NumSteps;
		UINT FrameIndex;
	};

	struct BilateralBlurCB
	{
		XMFLOAT2 TexelSize; 
		float DepthSigma;
		float NormalSigma;
	};

	struct GTAOBilateralBlurCB
	{
		XMFLOAT2 InvRes;
		XMINT2 Direction;
		float DepthSigma;
		float NormalSigma;
		int Radius;
	};

	struct MaterialConstants
	{
		XMFLOAT4 baseColorFactor;
		XMFLOAT3 emissiveFactor;
		float  occlusionStrength;

		float  metallicFactor;
		float  roughnessFactor;
		float  alphaCutoff;
		UINT   alphaMode;
	};

	uint32_t m_resizeWidth = 0;
	uint32_t m_resizeHeight = 0;
	bool m_needsResize = false;

	struct DepthIndirectCommand
	{
		uint32_t transformIdx;
		uint32_t vertexBufferIdx;
		D3D12_INDEX_BUFFER_VIEW ibv;
		D3D12_DRAW_INDEXED_ARGUMENTS drawArgs;
	};
	GPUCommandSignatureHandle m_depthCmdSig;
	GPUBufferHandle m_depthArgBuffer;
	static constexpr uint32_t MAX_DEPTH_DRAWS = 4096;
	uint32_t m_depthDrawCount = 0;


	struct GBufferIndirectCommand
	{
		uint32_t transformIdx;
		uint32_t matIdx;
		uint32_t vertexBufferIdx;
		uint32_t pad;
		D3D12_INDEX_BUFFER_VIEW ibv;
		D3D12_DRAW_INDEXED_ARGUMENTS drawArgs;
	};
	GPUCommandSignatureHandle m_gBufferCmdSig;
	GPUCommandSignatureHandle m_gBufferAlphaCmdSig;
	GPUBufferHandle m_gbufferArgBuffer;
	static constexpr uint32_t MAX_GBUFFER_DRAWS = 4096;
	uint32_t m_gbufferOpaqueDrawCount = 0;
	uint32_t m_gbufferAlphaDrawCount = 0;
	uint32_t m_gbufferAlphaOffset = 0;


	struct DirectionalShadowIndirectCommand
	{
		uint32_t objIdx;
		uint32_t vertexBufferIdx;
		D3D12_INDEX_BUFFER_VIEW ibv;
		D3D12_DRAW_INDEXED_ARGUMENTS drawArgs;
	};
	GPUCommandSignatureHandle m_shadowCmdSig;
	GPUBufferHandle m_directionalShadowArgBuffer;
	uint32_t m_directionalShadowDrawCount = 0;

	struct PointShadowIndirectCommand
	{
		uint32_t transformIdx;
		uint32_t vertexBufferIdx;
		D3D12_INDEX_BUFFER_VIEW ibv;
		D3D12_DRAW_INDEXED_ARGUMENTS drawArgs;
	};
	GPUCommandSignatureHandle m_pointShadowCmdSig;
	GPUBufferHandle m_pointShadowArgBuffer;
	static constexpr uint32_t MAX_SHADOW_DRAWS = 4096;
	uint32_t m_pointShadowDrawCount = 0;

};
