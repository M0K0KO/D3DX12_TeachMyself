#pragma once

#include <limits>
#include <cstdint>
#include "d3dx12.h"


struct DescriptorHandle
{
	uint32_t index = (std::numeric_limits<uint32_t>::max)();
	D3D12_CPU_DESCRIPTOR_HANDLE cpu = {};
	D3D12_GPU_DESCRIPTOR_HANDLE gpu = {};

	bool IsValid() const
	{
		return index != (std::numeric_limits<uint32_t>::max)();
	}
};