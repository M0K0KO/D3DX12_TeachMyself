#pragma once
#include "DescriptorHandle.h"
#include <wrl/client.h>
#include <vector>
#include <queue>
#include "d3dx12.h"

class DescriptorAllocator
{
public:
	void Init(ID3D12Device* device, D3D12_DESCRIPTOR_HEAP_TYPE type, uint32_t capacity, bool shaderVisible);

	DescriptorHandle Allocate();

	void Free(const DescriptorHandle& handle);
	void FreeByCpuHandle(D3D12_CPU_DESCRIPTOR_HANDLE cpu);

	void FinishFrame(uint64_t completedFenceValue);

	void SetCurrentFence(uint64_t fenceValue) { m_currentFence = fenceValue; };

	ID3D12DescriptorHeap* GetHeap() const { return m_heap.Get(); }
	uint32_t GetCapacity() const { return m_capacity; }

private:
	struct PendingFree
	{
		uint32_t index;
		uint64_t fenceValue;
	};

	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_heap;
	uint32_t m_incrementSize = 0;
	D3D12_CPU_DESCRIPTOR_HANDLE m_cpuStart = {};
	D3D12_GPU_DESCRIPTOR_HANDLE m_gpuStart = {};
	bool m_shaderVisible = false;

	std::vector<uint32_t> m_freeList;
	std::queue<PendingFree> m_pendingFrees;

	uint64_t m_currentFence = 0;

	uint32_t m_nextFreshIndex = 0;
	uint32_t m_capacity = 0;
};