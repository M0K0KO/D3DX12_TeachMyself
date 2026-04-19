#pragma once
#include <thread>
#include <string>


namespace MokoJob
{
	class JobQueue;

	class WorkerThread
	{
	public:
		WorkerThread(JobQueue& queue, int index);
		~WorkerThread();

		WorkerThread(const WorkerThread&) = delete;
		WorkerThread& operator=(const WorkerThread&) = delete;

		void Join();

	private:
		void Run();

		JobQueue& m_queue;
		std::thread m_thread;
		int m_index;
	};
}