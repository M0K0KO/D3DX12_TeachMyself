#pragma once
#include <cstdint>
#include <chrono>

namespace MokoJob
{
	static uint64_t NowNs()
	{
		return (uint64_t)std::chrono::duration_cast<std::chrono::nanoseconds>(
			std::chrono::steady_clock::now().time_since_epoch()).count();
	}
}