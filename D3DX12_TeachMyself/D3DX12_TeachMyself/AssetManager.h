#pragma once
#include "TextureManager.h"
#include "MaterialManager.h"
#include "MeshManager.h"


class AssetManager
{
    TextureManager  textures;
    MaterialManager materials;
    MeshManager     meshes;

public:
    AssetManager(GraphicsDevice* device);
    void Initialize(GraphicsDevice* device);

    TextureManager& Textures() { return textures; };
    MaterialManager& Materials() { return materials; };
    MeshManager& Meshes() { return meshes; };
};