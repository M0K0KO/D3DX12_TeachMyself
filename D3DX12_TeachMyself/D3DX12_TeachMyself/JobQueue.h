#pragma once
#include "Job.h"
#include <queue>
#include <mutex>
#include <condition_variable>

namespace MokoJob
{
	class JobQueue
	{
	public:
		void Push(Job job);
		void PushBatch(std::vector<Job> jobs);

		bool WaitAndPop(Job& outJob);

		bool TryPop(Job& outJob);

		void Shutdown();

	private:
		std::queue<Job> m_queue;
		std::mutex m_mutex;
		std::condition_variable m_cv;
		bool m_shutdown = false;
	};
}