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
		GPUBufferHandle vb;
		GPUBufferHandle ib;
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

	static GPUTextureHandle GetDefaultWhite();
	static GPUTextureHandle GetDefaultNormal();
	static GPUTextureHandle GetDefaultMR();

private:
	BuiltinAssets() = delete;
};