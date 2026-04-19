#include "WorkerThread.h"
#include "JobQueue.h"
#include "Job.h"

namespace MokoJob
{
	WorkerThread::WorkerThread(JobQueue& queue, int index)
		: m_queue(queue), m_index(index)
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
		Job job;
		while (m_queue.WaitAndPop(job))
		{
			job.Execute();
		}
	}
}