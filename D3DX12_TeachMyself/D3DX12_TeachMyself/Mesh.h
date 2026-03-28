#pragma once

#include "stdafx.h"
#include <optional>
#include "DirectXTex/DirectXTex.h"

using namespace DirectX;

namespace Mesh
{
	struct Vertex
	{
		XMFLOAT3 position;
		XMFLOAT3 normal;
		XMFLOAT2 uv;
	};

	struct SubMesh
	{
		uint32_t indexOffset;
		uint32_t indexCount;
	};

	struct TextureData
	{
		ScratchImage image;
		uint32_t width, height;
		DXGI_FORMAT format;
	};

	struct Mesh
	{
		std::vector<Vertex> vertices;
		std::vector<uint32_t> indices;
		std::vector<SubMesh> subMeshes;

		std::optional<TextureData> baseColorTexture;
	};
}