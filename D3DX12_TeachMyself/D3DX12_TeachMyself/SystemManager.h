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

	void InitAll(EntityScene& scene);
	void UpdateAll(EntityScene& scene, float dt, const SystemContext& ctx);
	void ShutdownAll(EntityScene& scene);
};
