#pragma once
#include "stdafx.h"
#include "Window.h"
#include "Renderer.h"
#include "FrameData.h"
#include "EntityScene.h"
#include "SystemManager.h"
#include "InputState.h"
#include "EditorSystem.h"
#include "JobSystem.h"
#include "SceneSerializer.h"
#include "AssetManager.h"

class GraphicsDevice;

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

private:
	std::unique_ptr<GraphicsDevice> m_device;

	Window wnd;
	EntityScene m_ecsScene;
	std::unique_ptr<SystemManager> m_systemManager;
	std::unique_ptr<AssetManager> m_assetManager;
	InputState m_inputState;

	Renderer m_renderer;
	RenderScene m_renderScene;

	SystemContext m_systemContext;
	MokoJob::JobSystem* m_jobSystem;
	EditorSystem* m_editorSystem;

	bool m_pendingSwapchainResize = false;
	uint32_t m_pendingWidth = 0;
	uint32_t m_pendingHeight = 0;

	bool m_initialized = false;
};