#include "stdafx.h"
#include "UploadStagingRingAllocator.h"
#include "HRException.h"

UploadStagingRingAllocator::UploadStagingRingAllocator(ID3D12Device* device, ID3D12Fence* fence)
	: m_fence(fence)
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

UploadStagingRingAllocator::~UploadStagingRingAllocator()
{
	if (m_resource)
		m_resource->Unmap(0, nullptr);
}


void UploadStagingRingAllocator::WaitForFront()
{
	if (m_submitQueue.empty())
		return;

	const SubmitRecord front = m_submitQueue.front();
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
	m_submitQueue.pop();
}

void UploadStagingRingAllocator::ReleaseCompleted()
{
	UINT64 completed = m_fence->GetCompletedValue();

	while (!m_submitQueue.empty() && m_submitQueue.front().fenceValue <= completed)
	{
		m_virtualTail = m_submitQueue.front().virtualHead;
		m_submitQueue.pop();
	}
}

StagingAllocation UploadStagingRingAllocator::Allocate(size_t size)
{
	ReleaseCompleted();
	auto result = TryAllocate(size);
	MOKO_ASSERT(result.has_value() && "Ring exhausted - caller must Flush first");
	return *result;
}

std::optional<StagingAllocation> UploadStagingRingAllocator::TryAllocate(size_t size)
{
	UINT64 aligned = AlignUp(static_cast<UINT>(size), kAlignment);
	MOKO_ASSERT(aligned <= kCapacity && "Allocation exceeds total capacity");

	UINT64 offset = m_virtualHead % kCapacity;
	UINT64 virtualHeadAfterPad = m_virtualHead;

	if (offset + aligned > kCapacity)
	{
		virtualHeadAfterPad += (kCapacity - offset);
		offset = 0;
	}

	while (virtualHeadAfterPad + aligned > m_virtualTail + kCapacity)
	{
		if (m_submitQueue.empty())
		{
			return std::nullopt;
		}

		const UINT64 completed = m_fence->GetCompletedValue();
		if (m_submitQueue.front().fenceValue <= completed)
		{
			m_virtualTail = m_submitQueue.front().virtualHead;
			m_submitQueue.pop();
			continue;
		}

		MOKOLOG_WARN("[RingBuffer] STALL! Waiting for GPU. vHead={} vTail={} inFlight={}",
			virtualHeadAfterPad, m_virtualTail, m_virtualHead - m_virtualTail);
		WaitForFront();
	}

	m_virtualHead = virtualHeadAfterPad;

	StagingAllocation alloc{};
	alloc.offset = offset;
	alloc.cpuAddress = static_cast<uint8_t*>(m_cpuBase) + offset;
	alloc.stagingResource = m_resource.Get();

	m_virtualHead += aligned;
	return alloc;
}

void UploadStagingRingAllocator::Submit(UINT64 fenceValue)
{
	m_submitQueue.push({ fenceValue, m_virtualHead });
}

UINT64 UploadStagingRingAllocator::AlignUp(UINT64 value, UINT64 alignment)
{
	return (value + alignment - 1) & ~(alignment - 1);
}
