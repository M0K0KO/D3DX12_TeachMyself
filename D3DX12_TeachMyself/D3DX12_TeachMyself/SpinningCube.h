#pragma once

#include "DXSample.h"
#include "GraphicsDevice.h"
#include "CommandContext.h"

using namespace DirectX;

class SpinningCube : public DXSample
{
public:
	SpinningCube(UINT width, UINT height, std::wstring name);

	virtual void OnInit();
	virtual void OnUpdate();
	virtual void OnRender();
	virtual void OnDestroy();

private:
	static const UINT FrameCount = 3;
	static const UINT TextureWidth = 256;
	static const UINT TextureHeight = 256;
	static const UINT TexturePixelSize = 4;

	struct Vertex
	{
		XMFLOAT3 position;
		XMFLOAT2 uv;
	};

	struct CubeConstantBuffer
	{
		XMFLOAT4X4 worldViewProj;
		float padding[48];
	};

	GraphicsDevice* m_device;
	BufferHandle m_vertexBuffer, m_indexBuffer, m_constantBuffer;
	TextureHandle m_texture;
	PipelineHandle m_pipeline;
	CubeConstantBuffer m_constantBufferData;

	std::vector<UINT8> GenerateTextureData();
};