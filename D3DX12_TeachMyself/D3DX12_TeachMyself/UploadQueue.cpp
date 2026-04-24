#include "stdafx.h"
#include "UploadQueue.h"
#include "HRException.h"


UploadQueue::UploadQueue(ID3D12Device* device)
	: m_device(device)
{
	D3D12_COMMAND_QUEUE_DESC qd{};
	qd.Type = D3D12_COMMAND_LIST_TYPE_COPY;
	qd.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
	HR_CHECK(device->CreateCommandQueue(&qd, IID_PPV_ARGS(&m_copyQueue)));

	HR_CHECK(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_COPY, IID_PPV_ARGS(&m_cmdAlloc)));

	HR_CHECK(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_COPY, m_cmdAlloc.Get(), nullptr, IID_PPV_ARGS(&m_cmdList)));
	m_cmdList->Close();

	HR_CHECK(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence)));

	m_ring = std::make_unique<UploadStagingRingAllocator>(device, m_fence.Get());
}

UINT64 UploadQueue::UploadBuffer(ID3D12Resource * dst, UINT64 dstOffset, const void* data, size_t size)
{
	MOKO_ASSERT(dst && data && size > 0);
	OpenListIfNeeded();

	auto alloc = m_ring->TryAllocate(size);
	if (!alloc)
	{
		FlushAndWait();
		alloc = m_ring->TryAllocate(size);
		MOKO_ASSERT(alloc && "Even after flush, allocation failed - single request > capacity?");
	}

	memcpy(alloc->cpuAddress, data, size);

	m_cmdList->CopyBufferRegion(
		dst, dstOffset,
		alloc->stagingResource, alloc->offset,
		size
	);

	return m_nextFenceValue;
}

UINT64 UploadQueue::UploadTexture(ID3D12Resource* dst, const D3D12_SUBRESOURCE_DATA* subresources, uint32_t subresourceCount, uint32_t firstSubresource)
{
	MOKO_ASSERT(dst && subresources && subresourceCount > 0);
	OpenListIfNeeded();

	auto desc = dst->GetDesc();
	std::vector<D3D12_PLACED_SUBRESOURCE_FOOTPRINT> layouts(subresourceCount);
	std::vector<UINT> numRows(subresourceCount);
	std::vector<UINT64> rowSizes(subresourceCount);
	UINT64 totalBytes = 0;

	m_device->GetCopyableFootprints(
		&desc, firstSubresource, subresourceCount, 0,
		layouts.data(), numRows.data(), rowSizes.data(), &totalBytes);

	auto alloc = m_ring->TryAllocate(totalBytes);
	MOKO_ASSERT(alloc && "Texture size exceeds staging ring capacity");
	if (!alloc)
	{
		FlushAndWait();
		alloc = m_ring->TryAllocate(totalBytes);
		MOKO_ASSERT(alloc && "Even after flush, allocation failed - single request > capacity?");
	}

	for (UINT i = 0; i < subresourceCount; i++)
	{
		D3D12_MEMCPY_DEST destInfo{};
		destInfo.pData = static_cast<uint8_t*>(alloc->cpuAddress) + layouts[i].Offset;
		destInfo.RowPitch = layouts[i].Footprint.RowPitch;
		destInfo.SlicePitch = SIZE_T(layouts[i].Footprint.RowPitch) * numRows[i];

		MemcpySubresource(&destInfo, &subresources[i], static_cast<SIZE_T>(rowSizes[i]), numRows[i], layouts[i].Footprint.Depth);
	
		layouts[i].Offset += alloc->offset;

		CD3DX12_TEXTURE_COPY_LOCATION srcLoc(alloc->stagingResource, layouts[i]);
		CD3DX12_TEXTURE_COPY_LOCATION dstLoc(dst, firstSubresource + i);

		m_cmdList->CopyTextureRegion(&dstLoc, 0, 0, 0, &srcLoc, nullptr);
	}

	return m_nextFenceValue;
}

UINT64 UploadQueue::Flush()
{
	if (!m_listOpen) return 0;

	HR_CHECK(m_cmdList->Close());
	ID3D12CommandList* lists[] = { m_cmdList.Get() };
	m_copyQueue->ExecuteCommandLists(1, lists);

	const UINT64 fv = m_nextFenceValue;
	HR_CHECK(m_copyQueue->Signal(m_fence.Get(), fv));
	m_ring->Submit(fv);

	m_lastSubmittedFenceValue = fv;
	m_nextFenceValue++;
	m_listOpen = false;
	return fv;
}

void UploadQueue::InsertWaitOnQueue(ID3D12CommandQueue* targetQueue, UINT64 fenceValue)
{
	if (fenceValue == 0) return;

	if (m_listOpen && fenceValue == m_nextFenceValue)
	{
		Flush();
	}

	HR_CHECK(targetQueue->Wait(m_fence.Get(), fenceValue));
}

void UploadQueue::WaitForFenceCPU(UINT64 fenceValue)
{
	if (m_fence->GetCompletedValue() >= fenceValue) return;
	HANDLE e = CreateEventEx(nullptr, nullptr, 0, EVENT_ALL_ACCESS);
	m_fence->SetEventOnCompletion(fenceValue, e);
	WaitForSingleObject(e, INFINITE);
	CloseHandle(e);
}

bool UploadQueue::IsComplete(UINT64 fenceValue) const
{
	return m_fence->GetCompletedValue() >= fenceValue;
}

void UploadQueue::OpenListIfNeeded()
{
	if (m_listOpen) return;

	if (m_lastSubmittedFenceValue > 0 && m_fence->GetCompletedValue() < m_lastSubmittedFenceValue)
	{
		WaitForFenceCPU(m_lastSubmittedFenceValue);
	}

	HR_CHECK(m_cmdAlloc->Reset());
	HR_CHECK(m_cmdList->Reset(m_cmdAlloc.Get(), nullptr));
	m_listOpen = true;
}

void UploadQueue::FlushAndWait()
{
	if (!m_listOpen) return;

	HR_CHECK(m_cmdList->Close());
	ID3D12CommandList* lists[] = { m_cmdList.Get() };
	m_copyQueue->ExecuteCommandLists(1, lists);

	const UINT64 fv = m_nextFenceValue;
	HR_CHECK(m_copyQueue->Signal(m_fence.Get(), fv));
	m_ring->Submit(fv);

	m_lastSubmittedFenceValue = fv;
	m_nextFenceValue++;
	m_listOpen = false;

	WaitForFenceCPU(fv);

	m_ring->ReleaseCompleted();

	OpenListIfNeeded();
}
