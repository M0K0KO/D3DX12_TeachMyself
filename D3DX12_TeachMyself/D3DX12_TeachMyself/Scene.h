#pragma once
#include "stdafx.h"
#include "RHITypes.h"
#include "Camera.h"

using namespace DirectX;

class Scene
{
public:
	Scene() = default;
	~Scene() = default;

public:
	struct RenderObject
	{
		BufferHandle vertexBuffer;
		BufferHandle indexBuffer;
		uint32_t indexOffset;
		uint32_t indexCount;
		GPUMaterial material;
		XMFLOAT4X4 world;
	};

	std::vector<RenderObject> renderObjects;

	Camera cam;
};

