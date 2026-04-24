#pragma once
#include <queue>
#include <optional>
#include <cstdint>
#include <d3d12.h>
#include <wrl.h>

using namespace Microsoft::WRL;

struct StagingAllocation
{
	void* cpuAddress;
	ID3D12Resource* stagingResource;
	UINT64 offset;
};

class UploadStagingRingAllocator
{
public:
	UploadStagingRingAllocator(ID3D12Device* device, ID3D12Fence* fence);
	~UploadStagingRingAllocator();

	void WaitForFront();
	void ReleaseCompleted();
	
	StagingAllocation Allocate(size_t size);
	std::optional<StagingAllocation> TryAllocate(size_t size);

	void Submit(UINT64 fenceValue);

private:
	static constexpr UINT64 kCapacity = 256 * 1024 * 1024; // 256MB
	static constexpr UINT64 kAlignment = 512;

	UINT64 AlignUp(UINT64 value, UINT64 alignment);

	ComPtr<ID3D12Resource> m_resource;
	ID3D12Fence* m_fence = nullptr;

	void* m_cpuBase;
	D3D12_GPU_VIRTUAL_ADDRESS m_gpuBase = 0;

	UINT64 m_virtualHead = 0;
	UINT64 m_virtualTail = 0;

	struct SubmitRecord { UINT64 fenceValue; UINT64 virtualHead; };
	std::queue<SubmitRecord> m_submitQueue;
};