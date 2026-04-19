#pragma once
#include "ISystem.h"
#include "JobQueue.h"
#include "JobHandle.h"
#include "JobGroup.h"
#include <vector>
#include <memory>
#include <functional>
#include <optional>
#include <mutex>
#include <atomic>
#include <cstdint>


namespace MokoJob
{
	extern thread_local int tls_workerIndex;
}

namespace MokoJob
{
	class WorkerThread;
	class JobHandle;

	struct WorkerStats
	{
		std::atomic<uint64_t> executed{ 0 };
		std::atomic<uint64_t> busyNs{ 0 };
	};
	struct JobSystemSnapshot
	{
		int                 workerCount;
		uint64_t            queueSize;
		uint64_t            submitted;
		uint64_t            completed;
		std::vector<uint64_t> workerExecuted;
		std::vector<double>   workerBusyRatio;  // 0.0 ~ 1.0
	};

	class JobSystem : public ISystem
	{
	public:
		explicit JobSystem(int workerCount = 0);
		~JobSystem() override;

		void Init(EntityScene& scene) override;
		void Update(EntityScene& scene, float dt, const SystemContext& ctx) override {}
		void Shutdown(EntityScene& scene) override;

		void	  Submit(std::function<void()> func);
		JobHandle SubmitTracked(std::function<void()> func);
		void	  SubmitInternal(std::function<void()> func, std::atomic<int>* counter);

		bool	  TryExecuteOne();

		void	  ParallelFor(int begin, int end, int batchSize, std::function<void(int)> func);

		template<typename TLocal>
		void	  ParallelForWithLocal(size_t count, std::function<TLocal()> initLocal, std::function<void(size_t, TLocal&)> body, std::function<void(TLocal&)> finalize, size_t batchSize = 0)
		{
			if (count == 0) return;

			const int workers = GetWorkerCount();
			const int slotCount = workers + 1;

			if (batchSize == 0)
			{
				batchSize = std::max<size_t>(1, count / (workers * 4));
			}

			// main thread fallback
			const size_t jobCount = (count + batchSize - 1) / batchSize;
			if (jobCount <= 1)
			{
				TLocal local = initLocal();
				for (size_t i = 0; i < count; ++i) body(i, local);
				finalize(local);
				return;
			}

			std::vector<std::optional<TLocal>> locals(slotCount);
			auto initFlags = std::make_unique<std::once_flag[]>(slotCount);

			{
				JobGroup group(*this);
				for (size_t start = 0; start < count; start += batchSize)
				{
					size_t batchEnd = std::min(start + batchSize, count);
					group.Submit([=, &locals, &initFlags, &initLocal, &body]() {
						int idx = GetCurrentSlotIndex();
						std::call_once(initFlags[idx], [&]() {
							locals[idx] = initLocal();
							});
						for (size_t i = start; i < batchEnd; ++i)
						{
							body(i, *locals[idx]);
						}
						});
				}
				group.Wait();
			}

			for (auto& opt : locals)
			{
				if (opt.has_value()) finalize(*opt);
			}
		}
	public:
		int GetWorkerCount() const { return static_cast<int>(m_workers.size()); };
		int GetCurrentWorkerIndex() const { return tls_workerIndex; };
		int GetCurrentSlotIndex() const { return (tls_workerIndex >= 0) ? tls_workerIndex : (int)m_workers.size(); };

		JobSystemSnapshot GetSnapshot();
		WorkerStats& GetWorkerStats(int index) { return *m_workerStats[index]; }
		void OnJobSubmitted() { m_submitted.fetch_add(1, std::memory_order_relaxed); }
		void OnJobCompleted() { m_completed.fetch_add(1, std::memory_order_relaxed); }

	private:
		void ShutdownInternal();

	private:
		int m_requestedCount;
		JobQueue m_queue;
		std::vector<std::unique_ptr<WorkerThread>> m_workers;
		bool m_initialized = false;

		std::atomic<uint64_t> m_submitted{ 0 };
		std::atomic<uint64_t> m_completed{ 0 };

		std::vector<std::unique_ptr<WorkerStats>> m_workerStats;

		struct SampleState
		{
			uint64_t prevBusyNs = 0;
			uint64_t prevWallNs = 0;
		};
		std::vector<SampleState> m_sampleState;
		uint64_t m_lastSampleWallNs = 0;
	};
}