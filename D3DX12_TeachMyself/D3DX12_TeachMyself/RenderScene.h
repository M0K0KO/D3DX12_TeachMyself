#pragma once
#include <vector>
#include <DirectXMath.h>
#include "FrameData.h"
#include "HandlePool.h"

using namespace DirectX;

struct RenderObject
{
	GPUBufferHandle vertexBuffer;
	GPUBufferHandle indexBuffer;
	uint32_t indexOffset;
	uint32_t indexCount;
	MaterialHandle material;
	XMFLOAT4X4 world;
	XMFLOAT3 aabbMin = { FLT_MAX,  FLT_MAX,  FLT_MAX };
	XMFLOAT3 aabbMax = { -FLT_MAX, -FLT_MAX, -FLT_MAX };
};

struct RenderScene
{
	std::vector<RenderObject> renderObjects;
	FrameData frameData;
    XMFLOAT3 sceneAABBMin = { FLT_MAX,  FLT_MAX,  FLT_MAX };
    XMFLOAT3 sceneAABBMax = { -FLT_MAX, -FLT_MAX, -FLT_MAX };
};