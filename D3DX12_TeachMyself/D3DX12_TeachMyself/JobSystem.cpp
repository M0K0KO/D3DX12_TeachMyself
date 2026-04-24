#include "stdafx.h"
#include "JobSystem.h"
#include "WorkerThread.h"
#include "JobGroup.h"
#include "JobTime.h"

namespace MokoJob
{
	thread_local int tls_workerIndex = -1;
}

namespace MokoJob
{
	JobSystem::JobSystem(int workerCount)
		: m_requestedCount(workerCount)
	{}

	JobSystem::~JobSystem()
	{
		ShutdownInternal();
	}

	void JobSystem::Init(SystemContext& ctx)
	{
		int count = m_requestedCount;
		if (count <= 0)
		{
			unsigned hw = std::thread::hardware_concurrency();
			count = hw > 1 ? static_cast<int>(hw) - 1 : 1;
		}

		m_workerStats.reserve(count);
		for (int i = 0; i < count; ++i)
		{
			m_workerStats.push_back(std::make_unique<WorkerStats>());
		}

		m_sampleState.resize(count);
		m_lastSampleWallNs = NowNs();

		m_workers.reserve(count);
		for (int i = 0; i < count; ++i)
		{
			m_workers.push_back(std::make_unique<WorkerThread>(m_queue, i, this));
		}

		m_initialized = true;
	}

	void JobSystem::Shutdown(SystemContext& ctx)
	{
		ShutdownInternal();
	}

	void JobSystem::Submit(std::function<void()> func)
	{
		MOKO_ASSERT(!m_initialized && "Submit after Shutdown is not allowed");

		OnJobSubmitted();
		auto wrapped = [this, orig = std::move(func)]() mutable {
			orig();
			OnJobCompleted();
			};
		m_queue.Push(Job{ std::move(wrapped), nullptr });
	}

	JobHandle JobSystem::SubmitTracked(std::function<void()> func)
	{
		MOKO_ASSERT(!m_initialized && "Submit after Shutdown is not allowed");

		OnJobSubmitted();
		auto counter = std::make_shared<std::atomic<int>>(1);
		auto counterCopy = counter;

		Job job;
		job.func = [this, orig = std::move(func), counterCopy]() mutable {
			orig();
			counterCopy->fetch_sub(1, std::memory_order_release);
			OnJobCompleted();
			};
		job.counter = nullptr;
		m_queue.Push(std::move(job));
		return JobHandle(counter, this);
	}

	void JobSystem::SubmitInternal(std::function<void()> func, std::atomic<int>* counter)
	{
		MOKO_ASSERT(!m_initialized && "Submit after Shutdown is not allowed");

		if (counter) counter->fetch_add(1, std::memory_order_relaxed);
		OnJobSubmitted();
		auto wrapped = [this, orig = std::move(func)]() mutable {
			orig();
			OnJobCompleted();
			};
		m_queue.Push(Job{ std::move(wrapped), counter });
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

	void JobSystem::WaitAll()
	{
		while (m_submitted.load(std::memory_order_acquire) != m_completed.load(std::memory_order_acquire))
		{
			if (!TryExecuteOne())
			{
				std::this_thread::yield();
			}
		}
	}

	JobSystemSnapshot JobSystem::GetSnapshot()
	{
		const int n = (int)m_workers.size();
		JobSystemSnapshot snap;
		snap.workerCount = n;
		snap.queueSize = m_queue.Size();
		snap.submitted = m_submitted.load(std::memory_order_relaxed);
		snap.completed = m_completed.load(std::memory_order_relaxed);
		snap.workerExecuted.resize(n);
		snap.workerBusyRatio.resize(n);

		uint64_t nowNs = NowNs();
		uint64_t deltaWall = nowNs - m_lastSampleWallNs;
		if (deltaWall == 0) deltaWall = 1;  

		for (int i = 0; i < n; ++i)
		{
			uint64_t busy = m_workerStats[i]->busyNs.load(std::memory_order_relaxed);
			uint64_t deltaBusy = busy - m_sampleState[i].prevBusyNs;
			double ratio = (double)deltaBusy / (double)deltaWall;
			if (ratio > 1.0) ratio = 1.0;  

			snap.workerBusyRatio[i] = ratio;
			snap.workerExecuted[i] = m_workerStats[i]->executed.load(std::memory_order_relaxed);

			m_sampleState[i].prevBusyNs = busy;
		}
		m_lastSampleWallNs = nowNs;

		return snap;
	}

	void JobSystem::ShutdownInternal()
	{
		if (!m_initialized) return;
		m_initialized = false;

		WaitAll();
		m_queue.Shutdown();
		m_workers.clear();
	}
}
