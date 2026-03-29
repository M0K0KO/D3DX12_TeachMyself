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
	m_commandList->IASetPrimitiveTopology(D3D10_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
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
	bufferView.Format = DXGI_FORMAT_R32_UINT;
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

void CommandContext_DX12::BindTexture(TextureHandle handle, uint32_t slot)
{
	auto texture = m_pDevice->m_textures[handle.id];

	UINT descriptorSize = m_pDevice->m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	CD3DX12_GPU_DESCRIPTOR_HANDLE srvGpuHandle(m_pDevice->m_cbvSrvHeap->GetGPUDescriptorHandleForHeapStart(), texture.heapSlot, descriptorSize);
	m_commandList->SetGraphicsRootDescriptorTable(slot, srvGpuHandle);
}

void CommandContext_DX12::TransitionBarrier(TextureHandle handle, RGResourceState before, RGResourceState after)
{
	auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(
		m_pDevice->m_textures[handle.id].resource.Get(),
		GetDXResourceState(before),
		GetDXResourceState(after));

	m_commandList->ResourceBarrier(1, &barrier);
}

void CommandContext_DX12::ClearRenderTarget(TextureHandle handle, const float clearValue[4])
{
	auto rtvHandle = m_pDevice->m_textures[handle.id].descriptorHandle;
	m_commandList->ClearRenderTargetView(rtvHandle, clearValue, 0, nullptr);
}

void CommandContext_DX12::ClearDepthStencil(TextureHandle handle, float depth)
{
	auto dsvHandle = m_pDevice->m_textures[handle.id].descriptorHandle;
	m_commandList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, depth, 0, 0, nullptr);
}

void CommandContext_DX12::SetRenderTarget(TextureHandle rt, TextureHandle depth)
{
	D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle{};
	UINT numRT = 0;
	if (rt.IsValid())
	{
		rtvHandle = m_pDevice->m_textures[rt.id].descriptorHandle;
		numRT = 1;
	}

	D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle{};
	if (depth.IsValid())
		dsvHandle = m_pDevice->m_textures[depth.id].descriptorHandle;
	
	m_commandList->OMSetRenderTargets(numRT, &rtvHandle, FALSE, &dsvHandle);
}

void CommandContext_DX12::DrawIndexed(uint32_t indexCount, uint32_t startIndex, uint32_t baseVertex)
{
	m_commandList->DrawIndexedInstanced(indexCount, 1, startIndex, baseVertex, 0);
}
