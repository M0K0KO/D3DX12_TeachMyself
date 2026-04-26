#pragma once
#include "RHITypes.h"
#include <DirectXMath.h>
#include "Material.h"

using namespace DirectX;

struct MeshSource
{
	enum class Type { None, GLTF, Builtin };
	Type type = Type::None;
	std::string path;
	int submeshIndex;
};

struct MeshRendererComponent
{
	// TODO
	//MeshHandle mesh;
	//MaterialHandle material;

	GPUBufferHandle vertexBuffer;
	GPUBufferHandle indexBuffer;
	uint32_t indexOffset = 0;
	uint32_t indexCount = 0;
	MaterialHandle material;
	XMFLOAT3 aabbMin = { FLT_MAX,  FLT_MAX,  FLT_MAX };
	XMFLOAT3 aabbMax = { -FLT_MAX, -FLT_MAX, -FLT_MAX };
	bool visible = true;

	MeshSource source;
};