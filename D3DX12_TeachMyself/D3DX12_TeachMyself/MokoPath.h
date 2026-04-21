#pragma once
#include <filesystem>

namespace MokoPath
{
	std::filesystem::path GetExecutableDir();
	std::filesystem::path GetAssetRoot();
	bool IsLoadableGLTF(std::filesystem::path path);
}