#pragma once
#include "RHITypes.h"

class CommandContext
{
public:
	virtual ~CommandContext() = default;

	virtual void SetViewport(float x, float y, float width, float height, float minDept = 0.0f, float maxDepth = 1.0f) = 0;
	virtual void SetScissorRect(long left, long top, long right, long bottom) = 0;
	virtual void SetComputePipeline(PipelineHandle handle) = 0;
	virtual void Dispatch(UINT x, UINT y, UINT z) = 0;
	virtual void SetPipeline(PipelineHandle handle) = 0;
	virtual void SetVertexBuffer(BufferHandle handle) = 0;
	virtual void SetIndexBuffer(BufferHandle handle) = 0;
	virtual void SetComputeRootConstants(UINT slot, const void* data, UINT count32Bit) = 0;
	virtual void SetRootConstants(UINT slot, const void* data, UINT count32Bit) = 0;
	virtual CBHandle UpdateConstantBuffer(UINT slot, const void* data, size_t size) = 0;
	virtual void SetComputeDescriptorTable(UINT slot, UINT heapSlot) = 0;
	virtual void BindConstantBuffer(uint32_t slot, CBHandle handle) = 0;
	virtual void BindTexture(TextureHandle handle, uint32_t slot) = 0;
	virtual void TransitionBarrier(TextureHandle handle, RGResourceState before, RGResourceState after) = 0;
	virtual void ClearRenderTarget(TextureHandle handle, const float clearValue[4]) = 0;
	virtual void ClearRenderTargets(UINT numRT, TextureHandle* renderTargets, const float clearValue[4]) = 0;
	virtual void ClearDepthStencil(TextureHandle handle, float depth) = 0;
	virtual void SetRenderTarget(UINT numRT, TextureHandle* renderTargets, TextureHandle depth) = 0;
	virtual void DrawIndexed(uint32_t indexCount, uint32_t startIndex, uint32_t baseVertex) = 0;
	virtual void Draw(uint32_t vertexCount, uint32_t startVertex) = 0;

	virtual void BeginTimestamp(uint32_t passIndex) = 0;
	virtual void EndTimestamp(uint32_t passIndex) = 0;
}; 