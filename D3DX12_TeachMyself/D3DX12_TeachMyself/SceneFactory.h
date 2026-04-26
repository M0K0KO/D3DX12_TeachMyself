#pragma once
#include "AssetLoader.h"
#include "Entity.h"
#include "GraphicsDevice.h"
#include "EntityScene.h"
#include "AssetManager.h"
#include <filesystem>

namespace SceneFactory
{
	Entity CreateEmpty(EntityScene&, const std::string& name, Entity parent = INVALID_ENTITY);
	Entity CreateCube(EntityScene&, const std::string& name = "Cube", Entity parent = INVALID_ENTITY);
	Entity CreateSphere(EntityScene&, const std::string& name = "Sphere", Entity parent = INVALID_ENTITY);
	Entity CreateDirLight(EntityScene&, const std::string& name = "Directional Light", Entity parent = INVALID_ENTITY);
	Entity CreatePointLight(EntityScene&, const std::string& name = "Point Light", Entity parent = INVALID_ENTITY);

	bool LoadGLTFToScene(EntityScene& ecsScene, AssetManager& assets, GraphicsDevice& device, const std::filesystem::path& path, Entity parent = INVALID_ENTITY);
};