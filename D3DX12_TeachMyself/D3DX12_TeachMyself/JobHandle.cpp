#include "stdafx.h"
#include "JobHandle.h"
#include "SpinWait.h"

void MokoJob::JobHandle::Wait() const
{
	if (!m_counter) return;

	SpinBackoff backoff;

	while (m_counter->load(std::memory_order_acquire) > 0)
	{
		if (m_system && m_system->TryExecuteOne())
		{
			backoff.Reset();
			continue;
		}
		backoff.Step();
	}
}
