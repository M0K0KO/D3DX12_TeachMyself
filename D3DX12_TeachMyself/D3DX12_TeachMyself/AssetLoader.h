#pragma once

#include "Mesh.h"
#include "tiny_gltf.h"

class AssetLoader
{
public:
	AssetLoader() = default;
	Mesh::Scene LoadGLTF(const std::string& path);
	float* LoadHDR(
		const std::string& path,
		int& width,
		int& height,
		int& channels,
		int desiredChannels = 0);
	void FreeImage(float* data);

private:
	const uint8_t* GetBufferPointer(const tinygltf::Model& model, const tinygltf::Accessor& acc);
};

