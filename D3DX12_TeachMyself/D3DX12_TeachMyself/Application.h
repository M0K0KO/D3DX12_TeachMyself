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
	Window wnd;
	std::unique_ptr<GraphicsDevice> m_device;
	Scene m_scene;
	Renderer m_renderer;

	struct CubeConstantBuffer
	{
		XMFLOAT4X4 worldViewProj;
		float padding[48];
	};
	CubeConstantBuffer m_constantBufferData;
};