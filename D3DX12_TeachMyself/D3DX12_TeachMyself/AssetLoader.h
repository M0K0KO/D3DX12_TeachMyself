#pragma once

#include "Mesh.h"
#include "tiny_gltf.h"
#include "Mesh.h"

class AssetLoader
{
public:
	AssetLoader() = default;
	Mesh::Mesh LoadGLTF(const std::string& path);
	Mesh::TextureData LoadTexture(const std::wstring& path);

private:
	const uint8_t* GetBufferPointer(const tinygltf::Model& model, const const tinygltf::Accessor& acc);
};