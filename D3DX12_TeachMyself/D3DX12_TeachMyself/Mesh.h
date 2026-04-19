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
		float metallicFactor = 1.0f;
		float roughnessFactor = 1.0f;
	};

	struct Node
	{
		std::string name;
		XMFLOAT3 translation = { 0,0,0 };
		XMFLOAT4 rotation = { 0,0,0,1 };
		XMFLOAT3 scale = { 1,1,1 };
		int parentIndex = -1;
		std::vector<int> children;
		std::vector<int> subMeshIndices;
	};

	struct SubMesh
	{
		std::string name;
		uint32_t indexOffset;
		uint32_t indexCount;
		int materialIndex;
		int nodeIndex = -1;
		XMFLOAT3 aabbMin = { FLT_MAX, FLT_MAX, FLT_MAX };
		XMFLOAT3 aabbMax = { -FLT_MAX, -FLT_MAX, -FLT_MAX };
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
		std::vector<Node> nodes;
		std::vector<Material> materials;
		std::vector<Texture> textures;

		XMFLOAT3 sceneAABBMin = { FLT_MAX, FLT_MAX, FLT_MAX };
		XMFLOAT3 sceneAABBMax = { -FLT_MAX, -FLT_MAX, -FLT_MAX };
	};
}