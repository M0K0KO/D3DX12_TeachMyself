#pragma once
#include "stdafx.h"
#include "RHITypes.h"

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
		TextureHandle texture;
		uint32_t indexCount;

		XMFLOAT4X4 world;
	};

	std::vector<RenderObject> renderObjects;
};

