#pragma once
#include "RHITypes.h"
#include "GraphicsDevice.h"
#include "Scene.h"

class Renderer
{
public:
	void Init(GraphicsDevice* device);
	void Render(GraphicsDevice* device, const Scene& scene);
	void Shutdown();

private:
	const float clearColor[4] = { 0.0f, 0.2f, 0.4f, 1.0f };

	PipelineHandle m_forwardPipeline;
	PipelineHandle m_depthPrePassPipeline;
	BufferHandle m_perFrameCB;
	BufferHandle m_perObjectCB;
	TextureHandle m_depthTexture;

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
};