#include "stdafx.h"
#include "DescriptorAllocator.h"
#include "HRException.h"

void DescriptorAllocator::Init(ID3D12Device* device, D3D12_DESCRIPTOR_HEAP_TYPE type, uint32_t capacity, bool shaderVisible)
{
	m_capacity = capacity;
	m_shaderVisible = shaderVisible;

	D3D12_DESCRIPTOR_HEAP_DESC desc = {};
	desc.NumDescriptors = capacity;
	desc.Type = type;
	desc.Flags = shaderVisible ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE : D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	
	HR_CHECK(device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&m_heap)));

	m_incrementSize = device->GetDescriptorHandleIncrementSize(type);
	m_cpuStart = m_heap->GetCPUDescriptorHandleForHeapStart();

	if (shaderVisible)
		m_gpuStart = m_heap->GetGPUDescriptorHandleForHeapStart();
	else
		m_gpuStart = { 0 };

	m_freeList.reserve(256);
}

DescriptorHandle DescriptorAllocator::Allocate()
{
	uint32_t index;

	if (!m_freeList.empty())
	{
		index = m_freeList.back();
		m_freeList.pop_back();
	}
	else
	{
		MOKO_ASSERT(m_nextFreshIndex < m_capacity && "DescriptorAllocator out of space");
		index = m_nextFreshIndex++;
	}

	DescriptorHandle handle;
	handle.index = index;
	handle.cpu.ptr = m_cpuStart.ptr + static_cast<SIZE_T>(index) * m_incrementSize;

	if (m_shaderVisible)
		handle.gpu.ptr = m_gpuStart.ptr + static_cast<UINT64>(index) * m_incrementSize;
	else
		handle.gpu.ptr = 0;

	return handle;
}

void DescriptorAllocator::Free(const DescriptorHandle& handle)
{
	if (!handle.IsValid())
		return;

	m_pendingFrees.push({ handle.index, m_currentFence });
}

void DescriptorAllocator::FreeByCpuHandle(D3D12_CPU_DESCRIPTOR_HANDLE cpu)
{
	MOKO_ASSERT(cpu.ptr >= m_cpuStart.ptr &&
		cpu.ptr < m_cpuStart.ptr + m_capacity * m_incrementSize);

	uint32_t index = static_cast<uint32_t>((cpu.ptr - m_cpuStart.ptr) / m_incrementSize);

	m_pendingFrees.push({ index, m_currentFence });
}

void DescriptorAllocator::FinishFrame(uint64_t completedFenceValue)
{
	while (!m_pendingFrees.empty() &&
		m_pendingFrees.front().fenceValue <= completedFenceValue)
	{
		m_freeList.push_back(m_pendingFrees.front().index);
		m_pendingFrees.pop();
	}
}
