#pragma once
#include "RHITypes.h"
#include "GraphicsDevice.h"
#include "ShaderCompiler.h"
#include "Scene.h"

enum class DebugMode
{
	None,
	PBR_Enabled,
	PBR_Disabled,
	DepthTexture,
	Albedo,
	Normal,
	MR
};

class Renderer
{
public:
	void Init(GraphicsDevice* device);
	void Render(GraphicsDevice* device, const Scene& scene);
	void Shutdown();

	void ChangeDebugMode(DebugMode mode) { debugMode = mode; };

	void OnResize(uint32_t width, uint32_t height);

private:
	void Resize(GraphicsDevice* device);

private:
	static constexpr float clearColor[4] = { 0.0f, 0.0f,  0.0f,  0.0f };

	UINT m_width;
	UINT m_height;

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

	ShaderHandle m_lightingVS;
	ShaderHandle m_lightingPS;
	PipelineDesc m_lightingPassPipelineDesc;
	PipelineHandle m_lightingPassPipeline;

	ShaderHandle m_PBRlightingVS;
	ShaderHandle m_PBRlightingPS;
	PipelineDesc m_PBRlightingPassPipelineDesc;
	PipelineHandle m_PBRlightingPassPipeline;

	ShaderHandle m_debugVS;
	ShaderHandle m_debugPS;
	PipelineDesc m_debugPipelineDesc;
	PipelineHandle m_debugPipeline;

	ShaderHandle m_depthDebugPS;
	PipelineDesc m_depthDebugPipelineDesc;
	PipelineHandle m_depthDebugPipeline;

	BufferHandle m_perFrameCB;
	BufferHandle m_perObjectCB;
	BufferHandle m_lightingDataCB;

	TextureHandle m_depthTexture;

	TextureHandle m_gbufferAlbedo;
	TextureHandle m_gbufferNormal;
	TextureHandle m_gbufferMR;

	struct PerFrameData
	{
		XMFLOAT4X4 ViewProj;
		XMFLOAT3 CameraPos;
		float padding;
	};
	PerFrameData m_perFrameCBData;

	struct PerObjectData
	{
		XMFLOAT4X4 World;
	};
	PerObjectData m_perObjectCBData;

	struct LightingData
	{
		XMFLOAT4X4 inverseVP;
		XMFLOAT3   cameraPos;
		float      pad0;
		XMFLOAT3   direction;
		float      pad1;
		XMFLOAT3   color;
		float      pad2;
		XMFLOAT3   ambient;
		float      intensity;
	};

	struct PBRLightingData
	{
		XMFLOAT4X4 gInvViewProj;
		XMFLOAT3 gCameraPos;
		float pad0;
		XMFLOAT3 gLightDir;
		float pad1;
		XMFLOAT3 gLightColor;
		float pad2;
	};

	struct MaterialConstants
	{
		float alphaCutoff;
		float metallicFactor;
		float roughnessFactor;
		float _pad;
	};

	DebugMode debugMode;

	uint32_t m_resizeWidth = 0;
	uint32_t m_resizeHeight = 0;
	bool m_needsResize = false;
};