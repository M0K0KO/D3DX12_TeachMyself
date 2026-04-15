#include "UploadHeapRingAllocator.h"
#include "HRException.h"
#include "MokoLog.h"

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
	assert(aligned <= kCapacity && "Ring Buffer Allocation exceeds total capacity");

	if (m_head + aligned > kCapacity)
	{
		LOG_WARN("[RingBuffer] WRAP-AROUND! Current Head: %u, AlignedSize: %u, Resetting to 0\n", m_head, aligned);

		while (!m_frameQueue.empty() && m_frameQueue.front().head >= m_head)
		{
			WaitForFront();
		}

		m_head = 0;
	}

	while (m_head < m_tail && m_head + aligned > m_tail)
	{
		LOG_WARN("[RingBuffer] STALL! Head(%u) is catching up to Tail(%u). Waiting for GPU...\n", m_head, m_tail);

		if (m_frameQueue.empty())
		{
			m_tail = m_head;
			break;
		}
		WaitForFront();
	}

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

void UploadHeapRingAllocator::WaitForFront()
{
	if (m_frameQueue.empty())
		return;

	const FrameRecord front = m_frameQueue.front();
	const UINT64 completed = m_fence->GetCompletedValue();
	if (completed < front.fenceValue)
	{
		HANDLE event = CreateEventEx(nullptr, nullptr, 0, EVENT_ALL_ACCESS);
		if (event)
		{
			m_fence->SetEventOnCompletion(front.fenceValue, event);
			WaitForSingleObject(event, INFINITE);
			CloseHandle(event);
		}
	}
	m_tail = front.head;
	m_frameQueue.pop();
}