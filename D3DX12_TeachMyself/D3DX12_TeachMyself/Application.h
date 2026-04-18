#pragma once
#include "stdafx.h"
#include "Window.h"
#include "GraphicsDevice_DX12.h"
#include "Renderer.h"
#include "FrameData.h"
#include "EntityScene.h"
#include "SystemManager.h"
#include "InputState.h"
#include "EditorSystem.h"

class Application
{
public:
	Application();
	~Application();
	int Run();

private:
	void Init();
	void Update();
	void Render();

	RenderScene ExtractRenderScene();

	void HandleKeyboardEvents();
	void HandleDebugModeInput();

private:
	std::unique_ptr<GraphicsDevice> m_device;

	Window wnd;
	EntityScene m_ecsScene;
	std::unique_ptr<SystemManager> m_systemManager;
	InputState m_inputState;
	Renderer m_renderer;

	EditorSystem* m_editorSystem;

	uint32_t m_pendingWidth = 0;
	uint32_t m_pendingHeight = 0;
	bool m_needsResize = false;

	bool m_initialized = false;
};