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
	static void Initialize(GraphicsDevice& device, AssetManager* assets);
	static void Shutdown(GraphicsDevice& device, AssetManager* assets);

	static MeshHandle GetCubeMesh();
	static MeshHandle GetSphereMesh();
	static TextureHandle GetDefaultWhite();
	static TextureHandle GetDefaultNormal();
	static TextureHandle GetDefaultMR();
	static MaterialHandle GetDefaultMaterial();

private:
	BuiltinAssets() = delete;
	inline static MeshHandle     s_cubeMesh;
	inline static MeshHandle     s_sphereMesh;
	inline static TextureHandle  s_defaultWhite, s_defaultNormal, s_defaultMR;
	inline static MaterialHandle s_defaultMaterial;
};