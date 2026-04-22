#pragma once
#include <filesystem>

namespace MokoPath
{
	std::filesystem::path GetExecutableDir();
	std::filesystem::path GetAssetRoot();
	std::string ToString(const std::wstring& wstr);
	bool IsLoadableGLTF(std::filesystem::path path);
}