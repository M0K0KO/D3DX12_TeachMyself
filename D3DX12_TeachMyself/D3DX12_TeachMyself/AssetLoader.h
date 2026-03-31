#pragma once

#include "Mesh.h"
#include "tiny_gltf.h"

class AssetLoader
{
public:
	AssetLoader() = default;
	Mesh::Scene LoadGLTF(const std::string& path);

private:
	const uint8_t* GetBufferPointer(const tinygltf::Model& model, const const tinygltf::Accessor& acc);
};

