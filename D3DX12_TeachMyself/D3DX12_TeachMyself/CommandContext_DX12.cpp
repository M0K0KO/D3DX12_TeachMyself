#include "stdafx.h"
#include "CommandContext_DX12.h"
#include "GraphicsDevice_DX12.h"

void CommandContext_DX12::Init(GraphicsDevice_DX12* pDevice, ID3D12GraphicsCommandList* pCommandList)
{
	m_pDevice = pDevice;
	m_commandList = pCommandList;
}

void CommandContext_DX12::SetPipeline(PipelineHandle handle)
{
	auto internalPSO = m_pDevice->m_pipelines[handle.id];
	m_commandList->SetPipelineState(internalPSO.pso.Get());
	m_commandList->SetGraphicsRootSignature(internalPSO.rootSignature.Get());
}

void CommandContext_DX12::SetVertexBuffer(BufferHandle handle)
{
	auto buffer = m_pDevice->m_buffers[handle.id];

	D3D12_VERTEX_BUFFER_VIEW bufferView = {};
	bufferView.BufferLocation = buffer.resource->GetGPUVirtualAddress();
	bufferView.StrideInBytes = buffer.desc.stride;
	bufferView.SizeInBytes = buffer.desc.size;

	m_commandList->IASetVertexBuffers(0, 1, &bufferView);
}

void CommandContext_DX12::SetIndexBuffer(BufferHandle handle)
{
	auto buffer = m_pDevice->m_buffers[handle.id];

	D3D12_INDEX_BUFFER_VIEW bufferView = {};
	bufferView.BufferLocation = buffer.resource->GetGPUVirtualAddress();
	bufferView.Format = DXGI_FORMAT_R16_UINT;
	bufferView.SizeInBytes = buffer.desc.size;

	m_commandList->IASetIndexBuffer(&bufferView);
}

void CommandContext_DX12::BindConstantBuffer(BufferHandle handle, uint32_t slot)
{
	auto buffer = m_pDevice->m_buffers[handle.id];

	ID3D12DescriptorHeap* ppHeaps[] = { m_pDevice->m_cbvSrvHeap.Get() };
	m_commandList->SetDescriptorHeaps(1, ppHeaps);
	UINT descriptorSize = m_pDevice->m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	CD3DX12_GPU_DESCRIPTOR_HANDLE cbvGpuHandle(m_pDevice->m_cbvSrvHeap->GetGPUDescriptorHandleForHeapStart(), buffer.heapSlot + m_pDevice->m_frameIndex, descriptorSize);
	m_commandList->SetGraphicsRootDescriptorTable(slot, cbvGpuHandle);
}

void CommandContext_DX12::DrawIndexed(uint32_t indexCount, uint32_t startIndex, uint32_t baseVertex)
{
	m_commandList->DrawIndexedInstanced(indexCount, 1, startIndex, baseVertex, 0);
}
