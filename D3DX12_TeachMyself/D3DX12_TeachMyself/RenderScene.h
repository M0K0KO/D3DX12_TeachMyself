#pragma once
#include <vector>
#include <DirectXMath.h>
#include "FrameData.h"
#include "HandlePool.h"

using namespace DirectX;

struct GPUTransformData
{
	XMFLOAT4X4 world;
	XMFLOAT4X4 worldInvTranspose;
};

struct RenderObject
{
	GPUBufferHandle vertexBuffer;
	GPUBufferHandle indexBuffer;
	uint32_t indexOffset;
	uint32_t indexCount;
	MaterialHandle material;
	uint32_t transformIdx;
	XMFLOAT3 aabbMin = { FLT_MAX,  FLT_MAX,  FLT_MAX };
	XMFLOAT3 aabbMax = { -FLT_MAX, -FLT_MAX, -FLT_MAX };
};

struct RenderScene
{
	std::vector<RenderObject> renderObjects;
	std::vector<GPUTransformData> transforms;
	FrameData frameData;
    XMFLOAT3 sceneAABBMin = { FLT_MAX,  FLT_MAX,  FLT_MAX };
    XMFLOAT3 sceneAABBMax = { -FLT_MAX, -FLT_MAX, -FLT_MAX };
};