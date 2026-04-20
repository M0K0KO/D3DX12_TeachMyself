#include "JobQueue.h"

namespace MokoJob
{
	void JobQueue::Push(Job job)
	{
		{
			std::lock_guard lk(m_mutex);
			m_queue.push(std::move(job));
		}
		m_cv.notify_one();
	}

	void JobQueue::PushBatch(std::vector<Job> jobs)
	{
		{
			std::lock_guard lk(m_mutex);
			for (auto& job : jobs) m_queue.push(std::move(job));
		}
		m_cv.notify_all();
	}

	bool JobQueue::WaitAndPop(Job& outJob)
	{
		std::unique_lock lk(m_mutex);
		m_cv.wait(lk, [this] {return !m_queue.empty() || m_shutdown; });

		if (m_shutdown && m_queue.empty()) return false;

		outJob = std::move(m_queue.front());
		m_queue.pop();
		return true;
	}

	bool JobQueue::TryPop(Job& outJob)
	{
		std::lock_guard lk(m_mutex);
		if (m_queue.empty()) return false;
		outJob = std::move(m_queue.front());
		m_queue.pop();
		return true;
	}

	void JobQueue::Shutdown()
	{
		{
			std::lock_guard lk(m_mutex);
			m_shutdown = true;
		}
		m_cv.notify_all();
	}

	uint64_t JobQueue::Size()
	{
		std::lock_guard lk(m_mutex);
		return static_cast<uint64_t>(m_queue.size());
	}
}
