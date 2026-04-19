#include "JobGroup.h"
#include "JobSystem.h"
#include <thread>

namespace MokoJob
{
	JobGroup::JobGroup(JobSystem& system)
		: m_system(system), m_counter(std::make_shared<std::atomic<int>>(0))
	{}

	void JobGroup::Submit(std::function<void()> func)
	{
		m_counter->fetch_add(1, std::memory_order_relaxed);
		m_system.SubmitInternal(std::move(func), m_counter.get());
	}

	void JobGroup::Wait()
	{
		while (m_counter->load(std::memory_order_acquire) > 0)
		{
			std::this_thread::yield();
		}
	}
}

