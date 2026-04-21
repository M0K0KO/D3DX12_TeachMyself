#include "SceneFactory.h"
#include "TransformComponent.h"
#include "MeshRendererComponent.h"
#include "RHITypes.h"
#include "Mesh.h"


namespace SceneFactory
{
	Entity LoadGLTFToScene(EntityScene& ecsScene, GraphicsDevice& device, const std::filesystem::path& path, Entity parent)
	{
		MOKOLOG_INFO("Loading Model from [{}]", path.string());

		AssetLoader loader;
		auto loadedScene = loader.LoadGLTF(path.string());

		BufferDesc vbDesc = {
			static_cast<uint32_t>(loadedScene.vertices.size() * sizeof(Mesh::Vertex)),
			sizeof(Mesh::Vertex),
			BufferUsage::Vertex, MemoryAccess::CpuWrite };
		auto vb = device.CreateBuffer(vbDesc, loadedScene.vertices.data());

		BufferDesc ibDesc = {
			static_cast<uint32_t>(loadedScene.indices.size() * sizeof(uint32_t)),
			sizeof(uint32_t),
			BufferUsage::Index, MemoryAccess::CpuWrite };
		auto ib = device.CreateBuffer(ibDesc, loadedScene.indices.data());

		std::vector<TextureHandle> gpuTextures;

		device.BeginTextureUpload();
		for (auto& tex : loadedScene.textures)
		{
			gpuTextures.push_back(device.LoadTexture(tex.path));
		}
		device.FlushTextureUploads();


		uint8_t white[] = { 255, 255, 255, 255 };
		uint8_t normal[] = { 128, 128, 255, 255 };
		uint8_t mr[] = { 0, 128, 0, 255 };
		TextureHandle defaultWhite = device.CreateTexture({ 1, 1, Format::R8G8B8A8_UNORM, TextureUsage::ShaderResource }, white);
		TextureHandle defaultNormal = device.CreateTexture({ 1, 1, Format::R8G8B8A8_UNORM, TextureUsage::ShaderResource }, normal);
		TextureHandle defaultMR = device.CreateTexture({ 1, 1, Format::R8G8B8A8_UNORM, TextureUsage::ShaderResource }, mr);

		Entity gltfRoot =
			parent == INVALID_ENTITY ?
			ecsScene.GetRoot() : parent;

		std::vector<Entity> nodeEntities(loadedScene.nodes.size(), INVALID_ENTITY);
		for (size_t i = 0; i < loadedScene.nodes.size(); ++i)
		{
			const auto& node = loadedScene.nodes[i];
			Entity e = ecsScene.CreateSceneEntity(node.name);
			nodeEntities[i] = e;

			auto& t = ecsScene.GetRegistry().Get<TransformComponent>(e);
			t.position = node.translation;
			t.rotation = node.rotation;
			t.scale = node.scale;
			t.dirty = true;
		}

		for (size_t i = 0; i < loadedScene.nodes.size(); ++i)
		{
			const auto& node = loadedScene.nodes[i];
			Entity child = nodeEntities[i];
			if (child == INVALID_ENTITY) continue;

			if (node.parentIndex < 0)
			{
				ecsScene.SetParent(child, gltfRoot);
			}
			else
			{
				Entity parent = nodeEntities[node.parentIndex];
				if (parent != INVALID_ENTITY)
					ecsScene.SetParent(child, parent);
			}
		}

		for (const auto& subMesh : loadedScene.subMeshes)
		{
			Entity e = ecsScene.CreateSceneEntity(subMesh.name);
			if (subMesh.nodeIndex >= 0 && subMesh.nodeIndex < static_cast<int>(nodeEntities.size()))
			{
				Entity parentEntity = nodeEntities[subMesh.nodeIndex];
				if (parentEntity != INVALID_ENTITY)
				{
					ecsScene.SetParent(e, parentEntity);
				}
			}

			auto& t = ecsScene.GetRegistry().Get<TransformComponent>(e);
			t.dirty = true;

			auto& mr = ecsScene.GetRegistry().Add<MeshRendererComponent>(e);
			mr.vertexBuffer = vb;
			mr.indexBuffer = ib;
			mr.indexOffset = subMesh.indexOffset;
			mr.indexCount = subMesh.indexCount;

			auto& mat = loadedScene.materials[subMesh.materialIndex];
			mr.material.baseColor = (mat.baseColorTexture >= 0) ? gpuTextures[mat.baseColorTexture] : defaultWhite;
			mr.material.normal = (mat.normalTexture >= 0) ? gpuTextures[mat.normalTexture] : defaultNormal;
			mr.material.metallicRoughness = (mat.metallicRoughnessTexture >= 0) ? gpuTextures[mat.metallicRoughnessTexture] : defaultMR;
			mr.material.alphaMode = mat.alphaMode;
			mr.material.alphaCutoff = mat.alphaCutoff;
			mr.material.metallicFactor = mat.metallicFactor;
			mr.material.roughnessFactor = mat.roughnessFactor;
			mr.visible = true;

			mr.aabbMin = subMesh.aabbMin;
			mr.aabbMax = subMesh.aabbMax;
		}
		XMFLOAT3 scaledMin = {
			loadedScene.sceneAABBMin.x,
			loadedScene.sceneAABBMin.y,
			loadedScene.sceneAABBMin.z
		};
		XMFLOAT3 scaledMax = {
			loadedScene.sceneAABBMax.x,
			loadedScene.sceneAABBMax.y,
			loadedScene.sceneAABBMax.z
		};
		ecsScene.SetSceneAABB(scaledMin, scaledMax);

		return gltfRoot;
	}
}