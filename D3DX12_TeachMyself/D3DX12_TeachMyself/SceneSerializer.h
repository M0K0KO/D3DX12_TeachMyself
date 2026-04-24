#pragma once
#include <filesystem>

#include "json.hpp"

using json = nlohmann::json;

class EntityScene;

class SceneSerializer
{
public:
	static bool Save(EntityScene& scene, const std::filesystem::path& path);
	static bool Load(EntityScene& scene, const std::filesystem::path& path);

private:
	static constexpr int kVersion = 1;
};