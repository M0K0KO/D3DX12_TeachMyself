#include "UploadHeapRingAllocator.h"
#include "HRException.h"
#include "MokoLogger.h"
#include "MokoAssert.h"

UploadHeapRingAllocator::UploadHeapRingAllocator(ID3D12Device* device, ID3D12Fence* fence)
	:
	m_fence(fence)
{
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
		m_virtualTail = m_frameQueue.front().virtualHead;
		m_frameQueue.pop();
	}
}

UploadAllocation UploadHeapRingAllocator::Allocate(size_t size)
{
	UINT aligned = AlignUp(static_cast<UINT>(size), kAlignment);
	MOKO_ASSERT(aligned <= kCapacity && "Ring Buffer Allocation exceeds total capacity");

	UINT offset = static_cast<UINT>(m_virtualHead % kCapacity);

	if (offset + aligned > kCapacity)
	{
		MOKOLOG_WARN("[RingBuffer] WRAP-AROUND! offset={} aligned={} padding={}",
			offset, aligned, kCapacity - offset);
		m_virtualHead += (kCapacity - offset);
		offset = 0;
	}

	while (m_virtualHead + aligned > m_virtualTail + kCapacity)
	{
		if (m_frameQueue.empty())
		{
			MOKOLOG_WARN("[RingBuffer] STALL with empty queue - forcing tail advance");
			m_virtualTail = m_virtualHead;
			break;
		}

		MOKOLOG_WARN("[RingBuffer] STALL! Waiting for GPU. vHead={} vTail={} inFlight={}",
			m_virtualHead, m_virtualTail, m_virtualHead - m_virtualTail);
		WaitForFront();
	}

	UploadAllocation alloc{};
	alloc.offset = offset;
	alloc.cpuAddress = static_cast<uint8_t*>(m_cpuBase) + offset;
	alloc.gpuAddress = m_gpuBase + offset;

	m_virtualHead += aligned;
	return alloc;
}

void UploadHeapRingAllocator::FinishFrame(UINT64 fenceValue)
{
	m_frameQueue.push({ fenceValue, m_virtualHead });
}


UINT UploadHeapRingAllocator::AlignUp(UINT value, UINT alignment)
{
	return (value + alignment - 1) & ~(alignment - 1);
}

size_t UploadHeapRingAllocator::GetEmptySpaceSize()
{
	const UINT64 inFlight = m_virtualHead - m_virtualTail;
	return (inFlight >= kCapacity) ? 0 : static_cast<size_t>(kCapacity - inFlight);
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
	m_virtualTail = front.virtualHead;
	m_frameQueue.pop();
}