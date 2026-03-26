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
	void BindConstantBuffer(BufferHandle handle, uint32_t slot) override;
	void BindTexture(TextureHandle handle, uint32_t slot) override;
	void DrawIndexed(uint32_t indexCount, uint32_t startIndex, uint32_t baseVertex) override;

private:
	GraphicsDevice_DX12* m_pDevice;
	ID3D12GraphicsCommandList* m_commandList;
};