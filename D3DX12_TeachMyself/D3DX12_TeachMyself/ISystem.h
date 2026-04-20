#pragma once
#include "EntityScene.h"
#include "Window.h"
#include "InputState.h"

struct SystemContext
{
	const float dt;
	Window* window;
	const InputState* input;
	EntityScene* scene;
};


class ISystem
{
public:
	virtual ~ISystem() = default;
	virtual void Init(SystemContext& scene) {}
	virtual void Update(SystemContext& ctx) = 0;
	virtual void Shutdown(SystemContext& scene) {}
};
