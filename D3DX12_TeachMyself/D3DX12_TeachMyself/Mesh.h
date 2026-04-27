#pragma once

#include <DirectXMath.h>
#include <vector>
#include <string>
#include <cfloat>
#include <optional>

#include "RHITypes.h"

using namespace DirectX;

struct AABB
{
	XMFLOAT3 min;
	XMFLOAT3 max;
};

struct Submesh
{
	uint32_t indexOffset;
	uint32_t indexCount;
	uint32_t materialSlot;
	AABB aabb;
};

struct MeshAsset
{
	GPUBufferHandle vb;
	GPUBufferHandle ib;
	uint32_t vertexCount;
	uint32_t indexCount;
	std::vector<Submesh> submeshes;
	AABB bounds;
};

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
		int emissiveTexture = -1;
		int occlusionTexture = -1;

		XMFLOAT4 baseColorFactor = { 0.0f, 0.0f, 0.0f, 0.0f };
		float metallicFactor = 1.0f;
		float roughnessFactor = 1.0f;
		XMFLOAT3 emissiveFactor = { 0.0f, 0.0f, 0.0f };
		float occlusionStrength = 1.0f;

		AlphaMode alphaMode = AlphaMode::Opaque;
		float alphaCutoff = 0.5f;

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
		bool sRGB = false;
		int width = 0;
		int height = 0;
		int channels = 0;
		int bytesPerChannel = 1;
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