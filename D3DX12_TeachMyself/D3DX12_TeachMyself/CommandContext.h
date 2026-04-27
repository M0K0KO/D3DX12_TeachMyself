#pragma once
#include "RHITypes.h"
#include "DescriptorHandle.h"

class CommandContext
{
public:
	virtual ~CommandContext() = default;

	virtual void SetViewport(float x, float y, float width, float height, float minDept = 0.0f, float maxDepth = 1.0f) = 0;
	virtual void SetScissorRect(long left, long top, long right, long bottom) = 0;

	virtual void SetComputePipeline(PipelineHandle handle) = 0;
	virtual void Dispatch(UINT x, UINT y, UINT z) = 0;

	virtual void SetPipeline(PipelineHandle handle) = 0;
	virtual void SetVertexBuffer(GPUBufferHandle handle) = 0;
	virtual void SetIndexBuffer(GPUBufferHandle handle) = 0;

	virtual void SetRootConstants(uint32_t slot, const void* data, uint32_t count32Bit) = 0;
	virtual void SetComputeRootConstants(uint32_t slot, const void* data, uint32_t count32Bit) = 0;

	virtual CBHandle UpdateConstantBuffer(const void* data, size_t size) = 0;

	virtual void BindConstantBuffer(uint32_t slot, CBHandle handle) = 0;
	virtual void BindComputeConstantBuffer(uint32_t slot, CBHandle handle) = 0;

	virtual void BindTexture(uint32_t slot, GPUTextureHandle handle) = 0;
	virtual void BindComputeTexture(uint32_t slot, GPUTextureHandle handle) = 0;

	virtual void BindUav(uint32_t slot, GPUTextureHandle handle, uint32_t mip = 0) = 0;
	virtual void BindComputeUav(uint32_t slot, GPUTextureHandle handle, uint32_t mip = 0) = 0;

	virtual void SetDescriptorTable(uint32_t slot, DescriptorHandle handle) = 0;
	virtual void SetComputeDescriptorTable(uint32_t slot, DescriptorHandle handle) = 0;

	virtual void BindRootSRV(uint32_t slot, GPUBufferHandle handle) = 0;

	virtual void TransitionBarrier(GPUTextureHandle handle, RGResourceState before, RGResourceState after) = 0;

	virtual void ClearRenderTarget(GPUTextureHandle handle, const float clearValue[4]) = 0;
	virtual void ClearRenderTargets(UINT numRT, GPUTextureHandle* renderTargets, const float clearValue[4]) = 0;
	virtual void ClearDepthStencil(GPUTextureHandle handle, float depth, int faceIdx = -1) = 0;

	virtual void SetRenderTarget(UINT numRT, GPUTextureHandle* renderTargets, GPUTextureHandle depth, int faceIdx = -1) = 0;

	virtual void DrawIndexed(uint32_t indexCount, uint32_t startIndex, uint32_t baseVertex) = 0;
	virtual void Draw(uint32_t vertexCount, uint32_t startVertex) = 0;

	virtual void* GetNativeHandle() = 0;

	virtual void BeginTimestamp(uint32_t passIndex) = 0;
	virtual void EndTimestamp(uint32_t passIndex) = 0;
}; 