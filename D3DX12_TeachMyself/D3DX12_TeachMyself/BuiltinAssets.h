#pragma once
#include <cstdint>
#include <DirectXMath.h>
#include "RHITypes.h"
#include "HandlePool.h"

using namespace DirectX;

class GraphicsDevice;
class AssetManager;

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
	static void Initialize(GraphicsDevice& device, AssetManager* assets);
	static void Shutdown(GraphicsDevice& device, AssetManager* assets);
	static bool IsInitialized();

	static const MeshData& GetCube();
	static const MeshData& GetSphere();

	static TextureHandle GetDefaultWhite();
	static TextureHandle GetDefaultNormal();
	static TextureHandle GetDefaultMR();

	static MaterialHandle GetDefaultMaterial();

private:
	BuiltinAssets() = delete;
};