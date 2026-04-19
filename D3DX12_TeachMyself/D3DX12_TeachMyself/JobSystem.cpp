#define NOMINMAX

#include "JobSystem.h"
#include "WorkerThread.h"
#include "JobGroup.h"

namespace MokoJob
{
	JobSystem::JobSystem(int workerCount)
		: m_requestedCount(workerCount)
	{}

	JobSystem::~JobSystem()
	{
		ShutdownInternal();
	}

	void JobSystem::Init(EntityScene& scene)
	{
		int count = m_requestedCount;
		if (count <= 0)
		{
			unsigned hw = std::thread::hardware_concurrency();
			count = hw > 1 ? static_cast<int>(hw) - 1 : 1;
		}

		m_workers.reserve(count);
		for (int i = 0; i < count; i++)
		{
			m_workers.push_back(std::make_unique<WorkerThread>(m_queue, i));
		}
		m_initialized = true;
	}

	void JobSystem::Shutdown(EntityScene & scene)
	{
		ShutdownInternal();
	}

	void JobSystem::Submit(std::function<void()> func)
	{
		Job job;
		job.func = std::move(func);
		job.counter = nullptr;
		m_queue.Push(std::move(job));
	}

	JobHandle JobSystem::SubmitTracked(std::function<void()> func)
	{
		auto counter = std::make_shared<std::atomic<int>>(1);

		Job job;
		job.func = std::move(func);
		job.counter = counter.get();

		auto counterCopy = counter;
		job.func = [orig = std::move(job.func), counterCopy]() mutable {
			orig();
			counterCopy->fetch_sub(1, std::memory_order_release);
		};
		job.counter = nullptr;

		m_queue.Push(std::move(job));
		return JobHandle(counter, this);
	}

	void JobSystem::SubmitInternal(std::function<void()> func, std::atomic<int>* counter)
	{
		Job job;
		job.func = std::move(func);
		job.counter = counter;
		m_queue.Push(std::move(job));
	}

	bool JobSystem::TryExecuteOne()
	{
		Job job;
		if (m_queue.TryPop(job))
		{
			job.Execute();
			return true;
		}
		
		return false;
	}

	void JobSystem::ParallelFor(int begin, int end, int batchSize, std::function<void(int)> func)
	{
		if (begin >= end) return;
		if (batchSize < 1) batchSize = 1;

		const int total = end - begin;
		const int jobCount = (total + batchSize - 1) / batchSize;

		// main thread fallback
		if (jobCount <= 1)
		{
			for (int i = begin; i < end; i++)
				func(i);

			return;
		}

		JobGroup group(*this);
		for (int j = 0; j < jobCount; j++)
		{
			const int lo = begin + j * batchSize;
			const int hi = std::min(lo + batchSize, end);

			group.Submit([lo, hi, &func] {
				for (int i = lo; i < hi; i++)
					func(i);
				});
		}
		group.Wait();
	}

	void JobSystem::ShutdownInternal()
	{
		if (!m_initialized) return;

		m_queue.Shutdown();
		m_workers.clear();
		m_initialized = false;
	}
}