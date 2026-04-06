#pragma once
#include "RHITypes.h"
#include "GraphicsDevice.h"
#include "ShaderCompiler.h"
#include "Scene.h"
#include "RenderGraph.h"

enum class DebugMode
{
	None,
	PBR_Enabled,
	PBR_Disabled,
	DepthTexture,
	Albedo,
	Normal,
	MR,
	BRDF_LUT,
	IrradianceMap,
	PreFilteredEnvrionmentMap
};

class FrameContext;

class Renderer
{
public:
	void Init(GraphicsDevice* device);
	void Render(GraphicsDevice* device, const Scene& scene);
	void Shutdown();

	void ChangeDebugMode(DebugMode mode) { debugMode = mode; };

	void OnResize(uint32_t width, uint32_t height);

public:
	static constexpr int CASCADE_COUNT = 4;
	static constexpr int MAX_POINT_LIGHTS = 8;

	struct FrameContext
	{
		RGResourceHandle backBuffer;
		RGResourceHandle gbufferAlbedo;
		RGResourceHandle gbufferNormal;
		RGResourceHandle gbufferMR;
		RGResourceHandle depthTexture;
		RGResourceHandle skyboxTexture;
		RGResourceHandle irradianceMap;
		RGResourceHandle prefilteredEnvMap;
		RGResourceHandle brdfLutTexture;
		RGResourceHandle shadowMap;

		CBHandle perFrameCB;
		CBHandle lightCB;
		CBHandle shadowCB;
		XMMATRIX LightViewProj[CASCADE_COUNT];
	};

private:
	void ReloadPSO(GraphicsDevice* device);
	void Resize(GraphicsDevice* device);
	
	void InitDepthPrePass(GraphicsDevice* device);
	void InitGBufferPass(GraphicsDevice* device);
	void InitLightingPass(GraphicsDevice* device);
	void InitPBRLightingPass(GraphicsDevice* device);
	void InitSkyboxPass(GraphicsDevice* device);
	void InitDebugPass(GraphicsDevice* device);

	void AddDepthPrePass(GraphicsDevice* device, RenderGraph& graph, FrameContext& fc, const Scene& scene);
	void AddGBufferPass(GraphicsDevice* device, RenderGraph& graph, FrameContext& fc, const Scene& scene);
	void AddLightingPass(GraphicsDevice* device, RenderGraph& graph, FrameContext& fc, const Scene& scene);
	void AddPBRLightingPass(GraphicsDevice* device, RenderGraph& graph, FrameContext& fc, const Scene& scene);
	void AddSkyboxPass(GraphicsDevice* device, RenderGraph& graph, FrameContext& fc, const Scene& scene);
	void AddDebugPass(GraphicsDevice* device, RenderGraph& graph, FrameContext& fc, const Scene& scene);

	void PrintGPUTimes(GraphicsDevice* device);

private:
	static constexpr float clearColor[4] = { 0.0f, 0.0f,  0.0f,  0.0f };

	UINT m_width;
	UINT m_height;

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

	ShaderHandle m_lightingPS;
	PipelineDesc m_lightingPassPipelineDesc;
	PipelineHandle m_lightingPassPipeline;

	ShaderHandle m_PBRlightingPS;
	PipelineDesc m_PBRlightingPassPipelineDesc;
	PipelineHandle m_PBRlightingPassPipeline;

	ShaderHandle m_debugPS;
	PipelineDesc m_debugPipelineDesc;
	PipelineHandle m_debugPipeline;

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
	PipelineHandle m_shadowMapPipeline;

	BufferHandle m_perFrameCB;
	BufferHandle m_perObjectCB;
	BufferHandle m_lightingDataCB;

	TextureHandle m_depthTexture;

	TextureHandle m_gbufferAlbedo;
	TextureHandle m_gbufferNormal;
	TextureHandle m_gbufferMR;

	TextureHandle m_equirectTexture;
	TextureHandle m_cubemapTexture;

	TextureHandle m_brdfLUTTexture;

	TextureHandle m_irradianceMapTexture;

	TextureHandle m_prefilteredMapTexture;
	

	TextureHandle m_shadowMap;

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
		XMFLOAT3 Ambient;
		int PointLightCount;

		PointLight PointLights[MAX_POINT_LIGHTS];
	};

	struct ShadowCB
	{
		XMFLOAT4X4 LightViewProj[CASCADE_COUNT];
		float cascadeSplits[CASCADE_COUNT];
	};
	ShadowCB m_shadowCB;

	struct MaterialConstants
	{
		float alphaCutoff;
		float metallicFactor;
		float roughnessFactor;
		float padding0;
	};

	DebugMode debugMode;

	uint32_t m_resizeWidth = 0;
	uint32_t m_resizeHeight = 0;
	bool m_needsResize = false;
};