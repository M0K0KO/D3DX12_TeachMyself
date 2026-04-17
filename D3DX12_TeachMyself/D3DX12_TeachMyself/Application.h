#pragma once
#include "stdafx.h"
#include "Window.h"
#include "GraphicsDevice_DX12.h"
#include "Renderer.h"
#include "FrameData.h"
#include "EntityScene.h"
#include "Camera.h"

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

	void UpdateCameraController(float dt);

	RenderScene ExtractRenderScene();

	void HandleKeyboardEvents();
	void HandleDebugModeInput();

private:
	std::unique_ptr<GraphicsDevice> m_device;

	Window wnd;
	EntityScene m_ecsScene;
	Renderer m_renderer;

	uint32_t m_pendingWidth = 0;
	uint32_t m_pendingHeight = 0;
	bool m_needsResize = false;

	bool m_initialized = false;
};