#include "UploadHeapRingAllocator.h"
#include "HRException.h"

UploadHeapRingAllocator::UploadHeapRingAllocator(ID3D12Device* device, ID3D12Fence* fence)
	:
	m_fence(fence)
{
	// make a huuuuge Upload Heap (64MB)
	auto heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
	auto desc = CD3DX12_RESOURCE_DESC::Buffer(kCapacity);
	HR_CHECK(device->CreateCommittedResource(
		&heapProps,
		D3D12_HEAP_FLAG_NONE,
		&desc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&m_resource)));

	HR_CHECK(m_resource->Map(0, nullptr, &m_cpuBase));
	m_gpuBase = m_resource->GetGPUVirtualAddress();
}

UploadHeapRingAllocator::~UploadHeapRingAllocator()
{
	if (m_resource)
		m_resource->Unmap(0, nullptr);
}


void UploadHeapRingAllocator::ReleaseCompleted()
{
	UINT64 completed = m_fence->GetCompletedValue();

	while (!m_frameQueue.empty() && m_frameQueue.front().fenceValue <= completed)
	{
		m_tail = m_frameQueue.front().head;
		m_frameQueue.pop();
	}
}

UploadAllocation UploadHeapRingAllocator::Allocate(size_t size)
{
	UINT aligned = AlignUp(static_cast<UINT>(size), kAlignment);

	if (m_head + aligned > kCapacity)
	{
		m_head = 0;
	}

	assert(m_head + aligned <= m_tail || m_head >= m_tail && "Ring Buffer Overflow");

	UploadAllocation alloc{};
	alloc.offset = m_head;
	alloc.cpuAddress = static_cast<uint8_t*>(m_cpuBase) + m_head;
	alloc.gpuAddress = m_gpuBase + m_head;

	m_head += aligned;
	return alloc;
}

void UploadHeapRingAllocator::FinishFrame(UINT64 fenceValue)
{
	m_frameQueue.push({ fenceValue, m_head });
}


UINT UploadHeapRingAllocator::AlignUp(UINT value, UINT alignment)
{
	return (value + alignment - 1) & ~(alignment - 1);
}

size_t UploadHeapRingAllocator::GetEmptySpaceSize()
{
	if (m_head >= m_tail)
		return (kCapacity - m_head) + m_tail - 1;
	else
		return (m_tail - m_head) - 1;
}