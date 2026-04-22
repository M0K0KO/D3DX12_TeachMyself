#include "SceneFactory.h"
#include "TransformComponent.h"
#include "MeshRendererComponent.h"
#include "RHITypes.h"
#include "Mesh.h"
#include "DirectionalLightComponent.h"
#include "PointLightComponent.h"
#include "BuiltinAssets.h"


namespace SceneFactory
{
	Entity CreateEmpty(EntityScene& scene, const std::string& name, Entity parent)
	{
		Entity e = scene.CreateSceneEntity(name);
		scene.SetParent(e, parent);
		
		MOKOLOG_INFO("Created Empty Entity");
		return e;
	}
	Entity CreateCube(EntityScene& scene, const std::string& name, Entity parent)
	{
		Entity e = scene.CreateSceneEntity(name);
		if (parent != INVALID_ENTITY)
		{
			scene.SetParent(e, parent);
		}

		auto& mr = scene.GetRegistry().Add<MeshRendererComponent>(e);
		const auto& cube = BuiltinAssets::GetCube();
		mr.vertexBuffer = cube.vb;
		mr.indexBuffer = cube.ib;
		mr.indexOffset = cube.indexOffset;
		mr.indexCount = cube.indexCount;
		mr.aabbMin = cube.aabbMin;
		mr.aabbMax = cube.aabbMax;

		mr.material.baseColor = BuiltinAssets::GetDefaultWhite();
		mr.material.normal = BuiltinAssets::GetDefaultNormal();
		mr.material.metallicRoughness = BuiltinAssets::GetDefaultMR();
		mr.material.metallicFactor = 0.0f;
		mr.material.roughnessFactor = 1.0f;
		mr.material.alphaMode = AlphaMode::Opaque;
		mr.material.alphaCutoff = 0.5f;
		mr.visible = true;

		MOKOLOG_INFO("Created Cube");
		return e;
	}
	Entity CreateSphere(EntityScene& scene, const std::string& name, Entity parent)
	{
		Entity e = scene.CreateSceneEntity(name);
		if (parent != INVALID_ENTITY)
		{
			scene.SetParent(e, parent);
		}

		auto& mr = scene.GetRegistry().Add<MeshRendererComponent>(e);
		const auto& sphere = BuiltinAssets::GetSphere();
		mr.vertexBuffer = sphere.vb;
		mr.indexBuffer = sphere.ib;
		mr.indexOffset = sphere.indexOffset;
		mr.indexCount = sphere.indexCount;
		mr.aabbMin = sphere.aabbMin;
		mr.aabbMax = sphere.aabbMax;

		mr.material.baseColor = BuiltinAssets::GetDefaultWhite();
		mr.material.normal = BuiltinAssets::GetDefaultNormal();
		mr.material.metallicRoughness = BuiltinAssets::GetDefaultMR();
		mr.material.metallicFactor = 0.0f;
		mr.material.roughnessFactor = 1.0f;
		mr.material.alphaMode = AlphaMode::Opaque;  
		mr.material.alphaCutoff = 0.5f;
		mr.visible = true;

		MOKOLOG_INFO("Created Sphere");
		return e;
	}
	Entity CreateDirLight(EntityScene& scene, const std::string& name, Entity parent)
	{
		Entity e = scene.CreateSceneEntity(name);
		auto& dl = scene.GetRegistry().Add<DirectionalLightComponent>(e);
		dl.direction = { 0.0f, -1.0f, 0.0f };
		dl.color = { 1,1,1 };
		dl.intensity = 1.0f;

		scene.SetParent(e, parent);
		MOKOLOG_INFO("Created Directional Light");
		return e;
	}
	Entity CreatePointLight(EntityScene& scene, const std::string& name, Entity parent)
	{
		Entity e = scene.CreateSceneEntity(name);
		auto& t = scene.GetRegistry().Get<TransformComponent>(e);
		t.position = { 0.0f, 2.0f, 0.0f };

		auto& pl = scene.GetRegistry().Add<PointLightComponent>(e);
		pl.color = { 0.7f, 0.7f, 0.7f };
		pl.radius = 10.0f;
		pl.intensity = 2.0f;

		scene.SetParent(e, parent);

		MOKOLOG_INFO("Created Point Light");
		return e;
	}

	Entity LoadGLTFToScene(EntityScene& ecsScene, GraphicsDevice& device, const std::filesystem::path& path, Entity parent)
	{
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


		TextureHandle defaultWhite = BuiltinAssets::GetDefaultWhite();
		TextureHandle defaultNormal = BuiltinAssets::GetDefaultNormal();
		TextureHandle defaultMR = BuiltinAssets::GetDefaultMR();

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

		MOKOLOG_INFO("Model {} has loaded Successfully", path.filename().string());
		return gltfRoot;
	}
}