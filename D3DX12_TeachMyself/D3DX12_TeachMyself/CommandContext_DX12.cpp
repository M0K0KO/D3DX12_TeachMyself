#include "stdafx.h"
#include "CommandContext_DX12.h"
#include "GraphicsDevice_DX12.h"
#include "MokoLog.h"

void CommandContext_DX12::Init(GraphicsDevice_DX12* pDevice, ID3D12GraphicsCommandList* pCommandList)
{
	m_pDevice = pDevice;
	m_commandList = pCommandList;
}

CBHandle CommandContext_DX12::UpdateConstantBuffer(UINT slot, const void* data, size_t size)
{
	auto alloc = m_pDevice->uploadHeapAllocator->Allocate(size);
	memcpy(alloc.cpuAddress, data, size);
	return { alloc.gpuAddress };
}

void CommandContext_DX12::SetViewport(float x, float y, float width, float height, float minDepth, float maxDepth)
{
	D3D12_VIEWPORT viewport = {};
	viewport.TopLeftX = x;
	viewport.TopLeftY = y;
	viewport.Width = width;
	viewport.Height = height;
	viewport.MinDepth = minDepth;
	viewport.MaxDepth = maxDepth;
	m_commandList->RSSetViewports(1, &viewport);
}

void CommandContext_DX12::SetScissorRect(long left, long top, long right, long bottom)
{
	D3D12_RECT rect = { left, top, right, bottom };
	m_commandList->RSSetScissorRects(1, &rect);
}

void CommandContext_DX12::SetPipeline(PipelineHandle handle)
{
	auto& internalPSO = m_pDevice->m_pipelines[handle.id];
	m_commandList->SetPipelineState(internalPSO.pso.Get());
	if (m_currentRootSignature != internalPSO.rootSignature.Get())
	{
		m_commandList->SetGraphicsRootSignature(internalPSO.rootSignature.Get());
		m_currentRootSignature = internalPSO.rootSignature.Get();
	}
	m_commandList->IASetPrimitiveTopology(D3D10_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
}

void CommandContext_DX12::SetComputePipeline(PipelineHandle handle)
{
	auto& internalPSO = m_pDevice->m_pipelines[handle.id];
	m_commandList->SetPipelineState(internalPSO.pso.Get());
	m_commandList->SetComputeRootSignature(internalPSO.rootSignature.Get());
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

void CommandContext_DX12::SetRootConstants(UINT slot, const void* data, UINT count32Bit)
{
	m_commandList->SetGraphicsRoot32BitConstants(slot, count32Bit, data, 0);
}

void CommandContext_DX12::SetComputeRootConstants(UINT slot, const void* data, UINT count32Bit)
{
	m_commandList->SetComputeRoot32BitConstants(slot, count32Bit, data, 0);
}

void CommandContext_DX12::BindConstantBuffer(uint32_t slot, CBHandle handle)
{
	m_commandList->SetGraphicsRootConstantBufferView(slot, handle.gpuAddress);
}

void CommandContext_DX12::BindTexture(TextureHandle handle, uint32_t slot)
{
	auto texture = m_pDevice->m_textures[handle.id];

	UINT descriptorSize = m_pDevice->m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	CD3DX12_GPU_DESCRIPTOR_HANDLE srvGpuHandle(m_pDevice->m_cbvSrvHeap->GetGPUDescriptorHandleForHeapStart(), texture.srvHeapSlot, descriptorSize);
	m_commandList->SetGraphicsRootDescriptorTable(slot, srvGpuHandle);
}

void CommandContext_DX12::SetComputeDescriptorTable(UINT slot, UINT heapSlot)
{
	UINT descriptorSize =
		m_pDevice->m_device->GetDescriptorHandleIncrementSize(
			D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	CD3DX12_GPU_DESCRIPTOR_HANDLE gpuHandle(
		m_pDevice->m_cbvSrvHeap->GetGPUDescriptorHandleForHeapStart(),
		heapSlot,
		descriptorSize);

	m_commandList->SetComputeRootDescriptorTable(slot, gpuHandle);
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
	auto rtvHandle = m_pDevice->m_textures[handle.id].rtvHandle;
	m_commandList->ClearRenderTargetView(rtvHandle, clearValue, 0, nullptr);
}

void CommandContext_DX12::ClearRenderTargets(UINT numRT, TextureHandle* renderTargets, const float clearValue[4])
{
	for (UINT i = 0; i < numRT; i++)
	{
		auto rtvHandle = m_pDevice->m_textures[renderTargets[i].id].rtvHandle;
		m_commandList->ClearRenderTargetView(rtvHandle, clearValue, 0, nullptr);
	}
}

void CommandContext_DX12::ClearDepthStencil(TextureHandle handle, float depth)
{
	auto dsvHandle = m_pDevice->m_textures[handle.id].dsvHandle;
	m_commandList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, depth, 0, 0, nullptr);
}

void CommandContext_DX12::SetRenderTarget(UINT numRT, TextureHandle* renderTargets, TextureHandle depth)
{
	std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> rtvHandles(numRT);

	for (UINT i = 0; i < numRT; i++)
	{
		if (renderTargets[i].IsValid())
		{
			rtvHandles[i] = m_pDevice->m_textures[renderTargets[i].id].rtvHandle;
		}
	}

	D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle{};
	if (depth.IsValid())
		dsvHandle = m_pDevice->m_textures[depth.id].dsvHandle;
	
	m_commandList->OMSetRenderTargets(
		numRT,
		rtvHandles.data(),
		FALSE,
		depth.IsValid() ? &dsvHandle : nullptr
	);
}

void CommandContext_DX12::DrawIndexed(uint32_t indexCount, uint32_t startIndex, uint32_t baseVertex)
{
	m_commandList->DrawIndexedInstanced(indexCount, 1, startIndex, baseVertex, 0);
}

void CommandContext_DX12::Draw(uint32_t vertexCount, uint32_t startVertex)
{
	m_commandList->DrawInstanced(vertexCount, 1, startVertex, 0);
}

void CommandContext_DX12::BeginTimestamp(uint32_t passIndex)
{
	m_pDevice->m_profiler.BeginTimestamp(m_pDevice->m_commandList.Get(), passIndex);
}

void CommandContext_DX12::EndTimestamp(uint32_t passIndex)
{
	m_pDevice->m_profiler.EndTimestamp(m_pDevice->m_commandList.Get(), passIndex);
}

void CommandContext_DX12::ResetState()
{
	m_currentRootSignature = nullptr;
}

void CommandContext_DX12::Dispatch(UINT x, UINT y, UINT z)
{
	m_commandList->Dispatch(x, y, z);
}
