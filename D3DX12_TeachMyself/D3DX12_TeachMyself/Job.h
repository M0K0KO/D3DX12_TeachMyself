#pragma once
#include <functional>
#include <atomic>

namespace MokoJob
{
	struct Job
	{
		std::function<void()> func;
		std::atomic<int>* counter = nullptr;

		void Execute()
		{
			if (func) func();
			if (counter) counter->fetch_sub(1, std::memory_order_release);
		}
	};
}