#pragma once
#include <thread>
#include <string>

namespace MokoJob
{
	class JobQueue;
	class JobSystem;

	class WorkerThread
	{
	public:
		WorkerThread(JobQueue& queue, int index, JobSystem* jobSystem);
		~WorkerThread();

		WorkerThread(const WorkerThread&) = delete;
		WorkerThread& operator=(const WorkerThread&) = delete;

		void Join();

	private:
		void Run();

		JobQueue& m_queue;
		JobSystem* m_jobSystem;
		std::thread m_thread;
		int m_index;
	};
}