#pragma once
#include <cstdint>
#include <DirectXMath.h>
#include "RHITypes.h"

using namespace DirectX;

class GraphicsDevice;

class BuiltinAssets
{
public:
	struct MeshData
	{
		BufferHandle vb;
		BufferHandle ib;
		uint32_t indexOffset = 0;
		uint32_t indexCount = 0;
		XMFLOAT3 aabbMin{};
		XMFLOAT3 aabbMax{};
	};

public:
	static void Initialize(GraphicsDevice& device);
	static void Shutdown(GraphicsDevice& device);
	static bool IsInitialized();

	static const MeshData& GetCube();
	static const MeshData& GetSphere();

	static TextureHandle GetDefaultWhite();
	static TextureHandle GetDefaultNormal();
	static TextureHandle GetDefaultMR();

private:
	BuiltinAssets() = delete;
};