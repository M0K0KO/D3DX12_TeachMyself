#include "stdafx.h"
#include "MokoPath.h"

namespace MokoPath
{
    std::filesystem::path GetExecutableDir()
    {
        return std::filesystem::current_path();
    }

    std::filesystem::path GetAssetRoot()
    {
        std::filesystem::path assetRoot = GetExecutableDir() / "assets";
        std::filesystem::exists(assetRoot) ? 0 : std::filesystem::create_directories(assetRoot);
        return assetRoot;
    }

    std::string ToString(const std::wstring& wstr)
    {
        if (wstr.empty())
            return {};

        const int sizeNeeded = WideCharToMultiByte(
            CP_UTF8,
            0,
            wstr.c_str(),
            static_cast<int>(wstr.size()),
            nullptr,
            0,
            nullptr,
            nullptr
        );

        std::string result(sizeNeeded, 0);

        WideCharToMultiByte(
            CP_UTF8,
            0,
            wstr.c_str(),
            static_cast<int>(wstr.size()),
            result.data(),
            sizeNeeded,
            nullptr,
            nullptr
        );

        return result;
    }

    std::wstring ToWString(const std::string& str)
    {
        int len = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, nullptr, 0);
        std::wstring result(len - 1, 0);
        MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, result.data(), len);
        return result;
    }

    bool IsLoadableGLTF(std::filesystem::path path)
    {
        auto ext = path.extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
        if (ext == ".gltf" || ext == ".glb")
        {
            return true;
        }
        else
        {
            return false;
        }
    }
}
