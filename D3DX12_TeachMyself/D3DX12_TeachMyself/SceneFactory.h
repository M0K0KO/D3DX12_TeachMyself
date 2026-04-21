#pragma once
#include "AssetLoader.h"
#include "Entity.h"
#include "GraphicsDevice.h"
#include "EntityScene.h"
#include <filesystem>

namespace SceneFactory
{
	Entity LoadGLTFToScene(EntityScene& ecsScene, GraphicsDevice& device, const std::filesystem::path& path, Entity parent = INVALID_ENTITY);
};