#include "stdafx.h"
#include "CommandContext_DX12.h"
#include "GraphicsDevice_DX12.h"
#include "MokoLog.h"
#include <cassert>

void CommandContext_DX12::Init(GraphicsDevice_DX12* pDevice, ID3D12GraphicsCommandList* pCommandList)
{
	m_pDevice = pDevice;
	m_commandList = pCommandList;
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
	assert(handle.IsValid() && handle.id < m_pDevice->m_pipelines.size());
	auto& internalPSO = m_pDevice->m_pipelines[handle.id];
	assert(internalPSO.pso.Get() && "SetPipeline: PSO is null!");

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
	const auto& internalPSO = m_pDevice->m_pipelines[handle.id];
	m_commandList->SetPipelineState(internalPSO.pso.Get());
	m_commandList->SetComputeRootSignature(internalPSO.rootSignature.Get());
}

void CommandContext_DX12::SetVertexBuffer(BufferHandle handle)
{
	const auto& buffer = m_pDevice->m_buffers[handle.id];

	D3D12_VERTEX_BUFFER_VIEW bufferView = {};
	bufferView.BufferLocation = buffer.resource->GetGPUVirtualAddress();
	bufferView.StrideInBytes = buffer.desc.stride;
	bufferView.SizeInBytes = buffer.desc.size;

	m_commandList->IASetVertexBuffers(0, 1, &bufferView);
}

void CommandContext_DX12::SetIndexBuffer(BufferHandle handle)
{
	const auto& buffer = m_pDevice->m_buffers[handle.id];

	D3D12_INDEX_BUFFER_VIEW bufferView = {};
	bufferView.BufferLocation = buffer.resource->GetGPUVirtualAddress();
	bufferView.Format = DXGI_FORMAT_R32_UINT;
	bufferView.SizeInBytes = buffer.desc.size;

	m_commandList->IASetIndexBuffer(&bufferView);
}

CBHandle CommandContext_DX12::UpdateConstantBuffer(const void* data, size_t size)
{
	auto alloc = m_pDevice->uploadHeapAllocator->Allocate(size);
	memcpy(alloc.cpuAddress, data, size);
	return { alloc.gpuAddress };
}

void CommandContext_DX12::SetRootConstants(uint32_t slot, const void* data, uint32_t count32Bit)
{
	m_commandList->SetGraphicsRoot32BitConstants(slot, count32Bit, data, 0);
}

void CommandContext_DX12::SetComputeRootConstants(uint32_t slot, const void* data, uint32_t count32Bit)
{
	m_commandList->SetComputeRoot32BitConstants(slot, count32Bit, data, 0);
}

void CommandContext_DX12::BindConstantBuffer(uint32_t slot, CBHandle handle)
{
	m_commandList->SetGraphicsRootConstantBufferView(slot, handle.gpuAddress);
}

void CommandContext_DX12::BindComputeConstantBuffer(uint32_t slot, CBHandle handle)
{
	m_commandList->SetComputeRootConstantBufferView(slot, handle.gpuAddress);
}

void CommandContext_DX12::BindTexture(uint32_t slot, TextureHandle handle)
{
	const auto& texture = m_pDevice->m_textures[handle.id];

	m_commandList->SetGraphicsRootDescriptorTable(slot, texture.srv.gpu);
}

void CommandContext_DX12::BindComputeTexture(uint32_t slot, TextureHandle handle)
{
	const auto& texture = m_pDevice->m_textures[handle.id];

	m_commandList->SetComputeRootDescriptorTable(slot, texture.srv.gpu);
}

void CommandContext_DX12::BindUav(uint32_t slot, TextureHandle handle, uint32_t mip)
{
	const auto& texture = m_pDevice->m_textures[handle.id];
	if (!texture.uavMips.empty())
	{
		assert(mip < texture.uavMips.size() && "BindUav: mip out of range");
		m_commandList->SetGraphicsRootDescriptorTable(slot, texture.uavMips[mip].gpu);
	}
	else
	{
		assert(texture.uav.IsValid() && "BindUav: texture has no UAV");
		assert(mip == 0 && "BindUav: non-mip UAV, mip must be 0");
		m_commandList->SetGraphicsRootDescriptorTable(slot, texture.uav.gpu);
	}
}

void CommandContext_DX12::BindComputeUav(uint32_t slot, TextureHandle handle, uint32_t mip)
{
	const auto& texture = m_pDevice->m_textures[handle.id];
	if (!texture.uavMips.empty())
	{
		assert(mip < texture.uavMips.size() && "BindUav: mip out of range");
		m_commandList->SetComputeRootDescriptorTable(slot, texture.uavMips[mip].gpu);
	}
	else
	{
		assert(texture.uav.IsValid() && "BindUav: texture has no UAV");
		assert(mip == 0 && "BindUav: non-mip UAV, mip must be 0");
		m_commandList->SetComputeRootDescriptorTable(slot, texture.uav.gpu);
	}
}

void CommandContext_DX12::SetDescriptorTable(uint32_t slot, DescriptorHandle handle)
{
	m_commandList->SetGraphicsRootDescriptorTable(slot, handle.gpu);
}

void CommandContext_DX12::SetComputeDescriptorTable(uint32_t slot, DescriptorHandle handle)
{
	m_commandList->SetComputeRootDescriptorTable(slot, handle.gpu);
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
	const auto& rtvHandle = m_pDevice->m_textures[handle.id].rtv;
	m_commandList->ClearRenderTargetView(rtvHandle.cpu, clearValue, 0, nullptr);
}

void CommandContext_DX12::ClearRenderTargets(UINT numRT, TextureHandle* renderTargets, const float clearValue[4])
{
	for (UINT i = 0; i < numRT; i++)
	{
		auto rtvHandle = m_pDevice->m_textures[renderTargets[i].id].rtv;
		m_commandList->ClearRenderTargetView(rtvHandle.cpu, clearValue, 0, nullptr);
	}
}

void CommandContext_DX12::ClearDepthStencil(TextureHandle handle, float depth, int faceIdx)
{
	D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle{};
	if (handle.IsValid())
		dsvHandle = faceIdx == -1 ?
		m_pDevice->m_textures[handle.id].dsv.cpu :
		m_pDevice->m_textures[handle.id].faceDsvs[faceIdx].cpu;
	m_commandList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, depth, 0, 0, nullptr);
}

void CommandContext_DX12::SetRenderTarget(UINT numRT, TextureHandle* renderTargets, TextureHandle depth, int faceIdx)
{
	D3D12_CPU_DESCRIPTOR_HANDLE rtvHandles[D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT] = {};
	for (UINT i = 0; i < numRT; i++)
	{
		assert(renderTargets[i].IsValid() && "SetRenderTarget: invalid RT handle");
		rtvHandles[i] = m_pDevice->m_textures[renderTargets[i].id].rtv.cpu;
	}

	D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle{};
	if (depth.IsValid())
		dsvHandle = faceIdx == -1 ?
			m_pDevice->m_textures[depth.id].dsv.cpu :
			m_pDevice->m_textures[depth.id].faceDsvs[faceIdx].cpu;

	
	m_commandList->OMSetRenderTargets(
		numRT,
		rtvHandles,
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

void* CommandContext_DX12::GetNativeHandle()
{
	return m_commandList;
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

void CommandContext_DX12::SetInternalCommandList(ID3D12GraphicsCommandList* pCmdList)
{
	m_commandList = pCmdList;
}
