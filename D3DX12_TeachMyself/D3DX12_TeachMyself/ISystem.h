#pragma once
#include "EntityScene.h"
#include "Window.h"
#include "InputState.h"

struct SystemContext
{
	Window* window;
	const InputState* input;
};


class ISystem
{
public:
	virtual ~ISystem() = default;
	virtual void Init(EntityScene& scene) {}
	virtual void Update(EntityScene& scene, float dt, const SystemContext& ctx) = 0;
	virtual void Shutdown(EntityScene& scene) {}
};
