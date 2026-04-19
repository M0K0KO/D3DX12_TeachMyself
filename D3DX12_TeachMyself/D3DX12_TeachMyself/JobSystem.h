#pragma once
#include "ISystem.h"
#include "JobQueue.h"
#include "JobHandle.h"
#include <vector>
#include <memory>

namespace MokoJob
{
	class WorkerThread;

	class JobSystem : public ISystem
	{
	public:
		explicit JobSystem(int workerCount = 0);
		~JobSystem() override;

		void Init(EntityScene& scene) override;
		void Update(EntityScene& scene, float dt, const SystemContext& ctx) override {}
		void Shutdown(EntityScene& scene) override;

		void Submit(std::function<void()> func);
		JobHandle SubmitTracked(std::function<void()> func);
		void SubmitInternal(std::function<void()> func, std::atomic<int>* counter);

		int WorkerCount() const { return static_cast<int>(m_workers.size()); };

	private:
		void ShutdownInternal();

	private:
		int m_requestedCount;
		JobQueue m_queue;
		std::vector<std::unique_ptr<WorkerThread>> m_workers;
		bool m_initialized = false;
	};
}