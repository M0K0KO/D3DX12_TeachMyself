#include "SystemManager.h"

void SystemManager::InitAll(SystemContext& ctx)
{
	for (auto& sys : systems)
	{
		sys->Init(ctx);
	}
}

void SystemManager::UpdateAll(SystemContext& ctx)
{
	for (auto& sys : systems)
	{
		sys->Update(ctx);
	}
}

void SystemManager::ShutdownAll(SystemContext& ctx)
{
	for (auto it = systems.rbegin(); it != systems.rend(); it++)
	{
		(*it)->Shutdown(ctx);
	}
}
