#pragma once
#include <filesystem>

namespace MokoPath
{
	std::filesystem::path GetExecutableDir();
	std::filesystem::path GetAssetRoot();
	std::string ToString(const std::wstring& wstr);
	std::wstring ToWString(const std::string& str);
	bool IsLoadableGLTF(std::filesystem::path path);
}