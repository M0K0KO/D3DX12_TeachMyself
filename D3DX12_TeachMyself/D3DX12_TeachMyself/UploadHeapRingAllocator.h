#pragma once
#include <queue>
#include "stdafx.h"

using namespace Microsoft::WRL;

struct UploadAllocation
{
	void* cpuAddress;  // memcpy 대상
	D3D12_GPU_VIRTUAL_ADDRESS    gpuAddress;  // CBV 바인딩용
	UINT                         offset;
};

class UploadHeapRingAllocator
{
public:
	UploadHeapRingAllocator(ID3D12Device* device, ID3D12Fence* fence);
	~UploadHeapRingAllocator();


	void ReleaseCompleted();
	UploadAllocation Allocate(size_t size);
	void FinishFrame(UINT64 fenceValue);

private:
	static constexpr UINT kCapacity = 64 * 1024 * 1024; // 64MB
	static constexpr UINT kAlignment = 256;

	UINT AlignUp(UINT value, UINT alignment);

	ComPtr<ID3D12Resource>  m_resource;
	ID3D12Fence* m_fence;       // 비소유, 외부 수명 보장
	void* m_cpuBase;     // Map으로 얻은 포인터
	D3D12_GPU_VIRTUAL_ADDRESS m_gpuBase;
	
	UINT m_head = 0;
	UINT m_tail = 0;

	struct FrameRecord { UINT64 fenceValue; UINT head; };
	std::queue<FrameRecord> m_frameQueue;
};