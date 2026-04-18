#include "SystemManager.h"

void SystemManager::InitAll(EntityScene& scene)
{
	for (auto& sys : systems)
	{
		sys->Init(scene);
	}
}

void SystemManager::UpdateAll(EntityScene& scene, float dt, const SystemContext& ctx)
{
	for (auto& sys : systems)
	{
		sys->Update(scene, dt, ctx);
	}
}

void SystemManager::ShutdownAll(EntityScene & scene)
{
	for (auto it = systems.rbegin(); it != systems.rend(); it++)
	{
		(*it)->Shutdown(scene);
	}
}
