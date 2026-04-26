#include "stdafx.h"
#include "AssetManager.h"
#include "BuiltinAssets.h"

AssetManager::AssetManager(GraphicsDevice* device)
	:
	textures(device),
	materials(&textures),
	meshes(device)
{
	Initialize(device);
}

void AssetManager::Initialize(GraphicsDevice* device)
{
	BuiltinAssets::Initialize(*device, this);
	materials.InitDefaults();
}