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
	PipelineHandle m_forwardPipeline;
	BufferHandle m_constantBuffer;
};