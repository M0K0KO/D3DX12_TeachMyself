#pragma once
#include <atomic>
#include <memory>
#include <thread>

namespace MokoJob
{
	class JobHandle
	{
	public:
		JobHandle() = default;
		explicit JobHandle(std::shared_ptr<std::atomic<int>> counter)
			: m_counter(std::move(counter))
		{}

		bool IsValid() const { return m_counter != nullptr; };
		bool IsDone() const
		{
			return !m_counter || m_counter->load(std::memory_order_acquire) == 0;
		}

		void Wait() const
		{
			if (!m_counter) return;
			while (m_counter->load(std::memory_order_acquire) > 0)
			{
				std::this_thread::yield();
			}
		}

	private:
		std::shared_ptr<std::atomic<int>> m_counter;
	};
}