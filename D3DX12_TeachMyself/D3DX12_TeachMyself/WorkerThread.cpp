#include "WorkerThread.h"
#include "JobQueue.h"
#include "JobSystem.h"
#include "Job.h"
#include "JobTime.h"

namespace MokoJob
{
	WorkerThread::WorkerThread(JobQueue& queue, int index, JobSystem* jobSystem)
		: m_queue(queue), m_index(index), m_jobSystem(jobSystem)
	{
		m_thread = std::thread([this] { Run(); });
	}

	WorkerThread::~WorkerThread()
	{
		Join();
	}

	void WorkerThread::Join()
	{
		if (m_thread.joinable()) m_thread.join();
	}

	void WorkerThread::Run()
	{
		tls_workerIndex = m_index;
		Job job;
		while (m_queue.WaitAndPop(job))
		{
			uint64_t t0 = NowNs();
			job.Execute();
			uint64_t t1 = NowNs();

			auto& stats = m_jobSystem->GetWorkerStats(m_index);
			stats.busyNs.fetch_add(t1 - t0, std::memory_order_relaxed);
			stats.executed.fetch_add(1, std::memory_order_relaxed);
		}
	}
}