#pragma once
#include "stdafx.h"
#include "Window.h"
#include "GraphicsDevice_DX12.h"
#include "Scene.h"
#include "Renderer.h"

class Application
{
public:
	Application();
	~Application() = default;;
	int Run();

private:
	void Init();
	void Update();
	void Render();

private:
	std::unique_ptr<GraphicsDevice> m_device;

	Window wnd;
	Scene m_scene;
	Renderer m_renderer;

	uint32_t m_pendingWidth = 0;
	uint32_t m_pendingHeight = 0;
	bool m_needsResize = false;

	bool m_initialized = false;
};