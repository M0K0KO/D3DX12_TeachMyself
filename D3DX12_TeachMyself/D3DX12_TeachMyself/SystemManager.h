#pragma once
#include "EntityScene.h"
#include "ISystem.h"

class SystemManager
{
	std::vector<std::unique_ptr<ISystem>> systems;

public:
	template<typename T, typename... Args>
	T* Add(Args&&... args)
	{
		auto sys = std::make_unique<T>(std::forward<Args>(args)...);
		T* ptr = sys.get();
		systems.push_back(std::move(sys));
		return ptr;
	}

	void InitAll(SystemContext& ctx);
	void UpdateAll(SystemContext& ctx);
	void ShutdownAll(SystemContext& ctx);
};
