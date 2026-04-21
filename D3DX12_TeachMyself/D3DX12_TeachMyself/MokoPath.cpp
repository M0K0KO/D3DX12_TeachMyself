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
