#pragma once
#include <queue>
#include <cstdint>
#include <d3d12.h>
#include <wrl.h>

using namespace Microsoft::WRL;

struct UploadAllocation
{
	void* cpuAddress;  // memcpy target
	D3D12_GPU_VIRTUAL_ADDRESS    gpuAddress;  // CBV binding
	UINT                         offset;
};

class UploadHeapRingAllocator
{
public:
	UploadHeapRingAllocator(ID3D12Device* device, ID3D12Fence* fence);
	~UploadHeapRingAllocator();

	void WaitForFront();
	void ReleaseCompleted();
	UploadAllocation Allocate(size_t size);
	void FinishFrame(UINT64 fenceValue);

	size_t GetEmptySpaceSize();

private:
	static constexpr UINT kCapacity = 128 * 1024 * 1024; // 128MB
	static constexpr UINT kAlignment = 256;

	UINT AlignUp(UINT value, UINT alignment);

	ComPtr<ID3D12Resource>  m_resource;
	ID3D12Fence* m_fence;   
	void* m_cpuBase;     // ptr obtained by map()
	D3D12_GPU_VIRTUAL_ADDRESS m_gpuBase;
	
	UINT64 m_virtualHead = 0;
	UINT64 m_virtualTail = 0;

	struct FrameRecord { UINT64 fenceValue; UINT64 virtualHead; };
	std::queue<FrameRecord> m_frameQueue;
};