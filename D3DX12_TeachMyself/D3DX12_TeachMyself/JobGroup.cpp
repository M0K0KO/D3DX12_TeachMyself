#include "JobGroup.h"
#include "JobSystem.h"
#include "JobHandle.h"
#include <thread>
#include "SpinWait.h"

namespace MokoJob
{
	JobGroup::JobGroup(JobSystem& system)
		: m_system(system), m_counter(std::make_shared<std::atomic<int>>(0))
	{}

	void JobGroup::Submit(std::function<void()> func)
	{
		m_system.SubmitInternal(std::move(func), m_counter.get());
	}

	void JobGroup::Wait()
	{
		SpinBackoff backoff;

		while (m_counter->load(std::memory_order_acquire) > 0)
		{
			if (m_system.TryExecuteOne())
			{
				backoff.Reset();
				continue;
			}
		}
		backoff.Step();
	}
	JobHandle JobGroup::GetHandle() const
	{
		return JobHandle(m_counter, &m_system);
	}
}

