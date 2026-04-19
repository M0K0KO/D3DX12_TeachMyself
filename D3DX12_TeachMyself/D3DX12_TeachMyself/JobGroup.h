#pragma once
#include <functional>
#include <memory>
#include <atomic>

namespace MokoJob
{
	class JobSystem;
	class JobHandle;

	class JobGroup
	{
	public:
		explicit JobGroup(JobSystem& system);

		void Submit(std::function<void()> func);
		void Wait();
		JobHandle GetHandle() const;

		int PendingCount() const
		{
			return m_counter->load(std::memory_order_acquire);
		}

	private:
		JobSystem& m_system;
		std::shared_ptr<std::atomic<int>> m_counter;
	};
}