#pragma once
#include "RHITypes.h"

class CommandContext
{
public:
	virtual ~CommandContext() = default;

	virtual void SetPipeline(PipelineHandle handle) = 0;
	virtual void SetVertexBuffer(BufferHandle handle) = 0;
	virtual void SetIndexBuffer(BufferHandle handle) = 0;
	virtual void BindConstantBuffer(BufferHandle handle, uint32_t slot) = 0;
	virtual void BindTexture(TextureHandle handle, uint32_t slot) = 0;
	virtual void TransitionBarrier(TextureHandle handle, RGResourceState before, RGResourceState after) = 0;
	virtual void ClearRenderTarget(TextureHandle handle, const float clearValue[4]) = 0;
	virtual void ClearRenderTargets(UINT numRT, TextureHandle* renderTargets, const float clearValue[4]) = 0;
	virtual void ClearDepthStencil(TextureHandle handle, float depth) = 0;
	virtual void SetRenderTarget(UINT numRT, TextureHandle* renderTargets, TextureHandle depth) = 0;
	virtual void DrawIndexed(uint32_t indexCount, uint32_t startIndex, uint32_t baseVertex) = 0;
	virtual void Draw(uint32_t vertexCount, uint32_t startVertex) = 0;
};