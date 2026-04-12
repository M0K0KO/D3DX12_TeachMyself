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
		XMFLOAT3 aabbMin = { FLT_MAX, FLT_MAX, FLT_MAX };
		XMFLOAT3 aabbMax = { -FLT_MAX, -FLT_MAX, -FLT_MAX };
	};

	std::vector<RenderObject> renderObjects;

	Camera cam;

	XMFLOAT3 sceneAABBMin = { FLT_MAX, FLT_MAX, FLT_MAX };
	XMFLOAT3 sceneAABBMax = { -FLT_MAX, -FLT_MAX, -FLT_MAX };
};

