#pragma once
#include "stdafx.h"
#include "UploadStagingRingAllocator.h"

using namespace Microsoft::WRL;



class UploadQueue
{
public:
	UploadQueue(ID3D12Device* device);

	UINT64 UploadBuffer(ID3D12Resource* dst, UINT64 dstOffset, const void* data, size_t size);
	UINT64 UploadTexture(ID3D12Resource* dst, const D3D12_SUBRESOURCE_DATA* subresources, uint32_t subresourceCount, uint32_t firstSubresource = 0);

	UINT64 Flush();

	void InsertWaitOnQueue(ID3D12CommandQueue* targetQueue, UINT64 fenceValue);

	void WaitForFenceCPU(UINT64 fenceValue);
	bool IsComplete(UINT64 fenceValue) const;

	ID3D12CommandQueue* GetCopyQueue() const { return m_copyQueue.Get(); }
	ID3D12Fence* GetFence()     const { return m_fence.Get(); }

private:
	void OpenListIfNeeded();
	void FlushAndWait();

private:
	ID3D12Device* m_device = nullptr;

	ComPtr<ID3D12CommandQueue> m_copyQueue;
	ComPtr<ID3D12CommandAllocator> m_cmdAlloc;
	ComPtr<ID3D12GraphicsCommandList> m_cmdList;
	ComPtr<ID3D12Fence> m_fence;

	UINT64 m_nextFenceValue = 1;
	UINT64 m_lastSubmittedFenceValue = 0;

	std::unique_ptr<UploadStagingRingAllocator> m_ring;
	bool m_listOpen = false;

};