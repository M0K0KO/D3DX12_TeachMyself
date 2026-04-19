#pragma once
#include <thread>
#include <chrono>

#if defined(_MSC_VER)
#include <intrin.h>
#endif


namespace MokoJob
{
	inline void CpuRelax()
	{
#if defined(_MSC_VER)
		_mm_pause();
#elif defined(__GNUC__) || defined(__clang__)
		__builtin_ia32_pause();
#else
		std::this_thread::yield();
#endif
	}

	class SpinBackoff
	{
	public:
		void Step()
		{
			if (m_count < SPIN_LIMIT)
			{
				CpuRelax();
			}
			else if (m_count < SPIN_LIMIT + YIELD_LIMIT)
			{
				std::this_thread::yield();
			}
			else
			{
				std::this_thread::sleep_for(std::chrono::microseconds(100));
			}
			++m_count;
		}
	
		void Reset() { m_count = 0; };

	private:
		int m_count = 0;
		static constexpr int SPIN_LIMIT = 1000;
		static constexpr int YIELD_LIMIT = 100;
	};
}