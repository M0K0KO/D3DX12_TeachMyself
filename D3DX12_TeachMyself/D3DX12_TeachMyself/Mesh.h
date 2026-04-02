#pragma once

#include "stdafx.h"
#include "RHITypes.h"
#include <optional>
#include "DirectXTex/DirectXTex.h"

using namespace DirectX;

namespace Mesh
{
	struct Vertex
	{
		XMFLOAT3 position;
		XMFLOAT3 normal;
		XMFLOAT4 tangent;
		XMFLOAT2 uv;
	};

	struct Material
	{
		int baseColorTexture = -1;
		int normalTexture = -1;
		int metallicRoughnessTexture = -1;
		AlphaMode alphaMode = AlphaMode::Opaque;
		float alphaCutoff = 0.5f;
	};

	struct SubMesh
	{
		uint32_t indexOffset;
		uint32_t indexCount;
		int materialIndex;
	};

	struct Texture
	{
		std::wstring path;
		
		bool embedded = false;
		int width = 0;
		int height = 0;
		int channels = 0;
		std::vector<unsigned char> data;
	};

	struct Scene
	{
		std::vector<Vertex> vertices;
		std::vector<uint32_t> indices;

		std::vector<SubMesh> subMeshes;
		std::vector<Material> materials;
		std::vector<Texture> textures;
	};
}