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
	MeshHandle mesh;
	std::vector<uint32_t> submeshIndices;
	std::vector<MaterialHandle> materials;
	bool visible = true;

	MeshSource source;
};