#pragma once
#include <atomic>
#include <memory>
#include <thread>
#include "JobSystem.h"

namespace MokoJob
{
	class JobSystem;

	class JobHandle
	{
	public:
		JobHandle() = default;
		explicit JobHandle(std::shared_ptr<std::atomic<int>> counter, JobSystem* system)
			: m_counter(std::move(counter)), m_system(system)
		{}

		bool IsValid() const { return m_counter != nullptr; };
		bool IsDone() const
		{
			return !m_counter || m_counter->load(std::memory_order_acquire) == 0;
		}

		void Wait() const;

	private:
		std::shared_ptr<std::atomic<int>> m_counter;
		JobSystem* m_system = nullptr;
	};
}