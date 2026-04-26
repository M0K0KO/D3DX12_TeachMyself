#pragma once
#include "EntityScene.h"
#include "Window.h"
#include "InputState.h"
#include "AssetManager.h"

struct SystemContext
{
	Window* window;
	const InputState* input;
	AssetManager* assetManager;
};


class ISystem
{
public:
	virtual ~ISystem() = default;
	virtual void Init(SystemContext& ctx) {}
	virtual void Update(EntityScene& scene, float dt, SystemContext& ctx) = 0;
	virtual void Shutdown(SystemContext& ctx) {}
};
