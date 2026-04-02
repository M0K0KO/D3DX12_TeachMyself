#pragma once
#include "CommandContext.h"

class GraphicsDevice_DX12;

class CommandContext_DX12 : public CommandContext
{
public:
	CommandContext_DX12() = default;

	void Init(GraphicsDevice_DX12* pDevice, ID3D12GraphicsCommandList* pCommandList);

	void SetPipeline(PipelineHandle handle) override;
	void SetVertexBuffer(BufferHandle handle) override;
	void SetIndexBuffer(BufferHandle handle) override;
	void SetRootConstants(UINT slot, const void* data, UINT count32Bit) override;
	CBHandle UpdateConstantBuffer(UINT slot, const void* data, size_t size) override;
	void BindConstantBuffer(uint32_t slot, CBHandle handle) override;
	void BindTexture(TextureHandle handle, uint32_t slot) override;
	void TransitionBarrier(TextureHandle handle, RGResourceState before, RGResourceState after) override;
	void ClearRenderTarget(TextureHandle handle, const float clearValue[4]) override;
	void ClearDepthStencil(TextureHandle handle, float depth) override;
	void ClearRenderTargets(UINT numRT, TextureHandle* renderTargets, const float clearValue[4]) override;
	void SetRenderTarget(UINT numRT, TextureHandle* renderTargets, TextureHandle depth) override;
	void DrawIndexed(uint32_t indexCount, uint32_t startIndex, uint32_t baseVertex) override;
	void Draw(uint32_t vertexCount, uint32_t startVertex) override;

	void BeginTimestamp(uint32_t passIndex) override;
	void EndTimestamp(uint32_t passIndex) override;

private:
	inline D3D12_RESOURCE_STATES GetDXResourceState(RGResourceState state)
	{
		switch (state)
		{
		case RGResourceState::RenderTarget:		return D3D12_RESOURCE_STATE_RENDER_TARGET;
		case RGResourceState::DepthWrite:		return D3D12_RESOURCE_STATE_DEPTH_WRITE;
		case RGResourceState::DepthRead:		return D3D12_RESOURCE_STATE_DEPTH_READ;
		case RGResourceState::ShaderResource:	return D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE;
		case RGResourceState::Present:			return D3D12_RESOURCE_STATE_PRESENT;
		case RGResourceState::CopyDest:			return D3D12_RESOURCE_STATE_COPY_DEST;
		}
	}

private:
	GraphicsDevice_DX12* m_pDevice;
	ID3D12GraphicsCommandList* m_commandList;

	ID3D12RootSignature* m_currentRootSignature = nullptr;
};