#pragma once
#include "stdafx.h"
#include "Mesh.h"
#include "HandlePool.h"

class GraphicsDevice;

class MeshManager
{
public:
	struct CreateDesc
	{
		const void* vertices;
		uint32_t vertexCount;
		uint32_t vertexStride;
		const uint32_t* indices;
		uint32_t indexCount;
		std::vector<Submesh> submeshes;
		AABB bounds;
	};

	MeshManager(GraphicsDevice* device);
	~MeshManager();

	MeshHandle Create(const CreateDesc& desc);
	void Destroy(MeshHandle h);

	const MeshAsset* Get(MeshHandle h) const { return m_pool.Get(h); }
	MeshAsset* Get(MeshHandle h) { return m_pool.Get(h); }

private:
	GraphicsDevice* m_device;
	HandlePool<MeshAsset, MeshTag> m_pool;
};