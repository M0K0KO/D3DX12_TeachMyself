#pragma once
#include "CommandContext.h"

class GraphicsDevice_DX12;

class CommandContext_DX12 : public CommandContext
{
public:
	CommandContext_DX12() = default;

	void Init(GraphicsDevice_DX12* pDevice, ID3D12GraphicsCommandList* pCommandList);

	void SetViewport(float x, float y, float width, float height, float minDepth = 0.0f, float maxDepth = 1.0f) override;
	void SetScissorRect(long left, long top, long right, long bottom) override;
	void SetComputePipeline(PipelineHandle handle) override;
	void Dispatch(UINT x, UINT y, UINT z) override;
	void SetPipeline(PipelineHandle handle) override;
	void SetVertexBuffer(GPUBufferHandle handle) override;
	void SetIndexBuffer(GPUBufferHandle handle) override;
	
	void SetRootConstants(uint32_t slot, const void* data, uint32_t count32Bit) override;
	void SetComputeRootConstants(uint32_t slot, const void* data, uint32_t count32Bit) override;

	CBHandle UpdateConstantBuffer(const void* data, size_t size) override;

	void BindConstantBuffer(uint32_t slot, CBHandle handle) override;
	void BindComputeConstantBuffer(uint32_t slot, CBHandle handle) override;

	void BindTexture(uint32_t slot, GPUTextureHandle handle) override;
	void BindComputeTexture(uint32_t slot, GPUTextureHandle handle) override;

	void BindUav(uint32_t slot, GPUTextureHandle handle, uint32_t mip = 0) override;
	void BindComputeUav(uint32_t slot, GPUTextureHandle handle, uint32_t mip = 0) override;

	void SetDescriptorTable(uint32_t slot, DescriptorHandle handle) override;
	void SetComputeDescriptorTable(uint32_t slot, DescriptorHandle handle) override;

	void BindRootSRV(uint32_t slot, GPUBufferHandle handle) override;

	void TransitionBarrier(GPUTextureHandle handle, RGResourceState before, RGResourceState after) override;
	void TransitionBarrier(GPUBufferHandle handle, RGResourceState before, RGResourceState after) override;


	void ClearRenderTarget(GPUTextureHandle handle, const float clearValue[4]) override;
	void ClearDepthStencil(GPUTextureHandle handle, float depth, int faceIdx = -1) override;
	void ClearRenderTargets(UINT numRT, GPUTextureHandle* renderTargets, const float clearValue[4]) override;
	void SetRenderTarget(UINT numRT, GPUTextureHandle* renderTargets, GPUTextureHandle depth, int faceIdx = -1) override;

	void ExecuteIndirect(GPUCommandSignatureHandle cmdSigHandle, UINT drawCount, GPUBufferHandle argBufferHandle, UINT argBufferOffset) override;
	void DrawIndexed(uint32_t indexCount, uint32_t startIndex, uint32_t baseVertex) override;
	void Draw(uint32_t vertexCount, uint32_t startVertex) override;

	void* GetNativeHandle() override;

	void BeginTimestamp(uint32_t passIndex) override;
	void EndTimestamp(uint32_t passIndex) override;

	void ResetState();

	void SetInternalCommandList(ID3D12GraphicsCommandList* pCmdList);
private:
	inline D3D12_RESOURCE_STATES GetDXResourceState(RGResourceState state)
	{
		switch (state)
		{
		case RGResourceState::RenderTarget:		return D3D12_RESOURCE_STATE_RENDER_TARGET;
		case RGResourceState::DepthWrite:		return D3D12_RESOURCE_STATE_DEPTH_WRITE;
		case RGResourceState::DepthRead:		return D3D12_RESOURCE_STATE_DEPTH_READ;
		case RGResourceState::UnorderedAccess:  return D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
		case RGResourceState::ShaderResource:	return D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
		case RGResourceState::Present:			return D3D12_RESOURCE_STATE_PRESENT;
		case RGResourceState::CopyDest:			return D3D12_RESOURCE_STATE_COPY_DEST;
		case RGResourceState::IndirectArgument: return D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT;
		}
	}

private:
	GraphicsDevice_DX12* m_pDevice;
	ID3D12GraphicsCommandList* m_commandList;

	ID3D12RootSignature* m_currentRootSignature = nullptr;
};