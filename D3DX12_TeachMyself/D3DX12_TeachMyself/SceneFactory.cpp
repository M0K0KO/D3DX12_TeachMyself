#include "SceneFactory.h"
#include "TransformComponent.h"
#include "MeshRendererComponent.h"
#include "RHITypes.h"
#include "Mesh.h"
#include "DirectionalLightComponent.h"
#include "PointLightComponent.h"
#include "BuiltinAssets.h"
#include "MokoPath.h"


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

    bool LoadGLTFToScene(EntityScene& ecsScene, GraphicsDevice& device, const std::filesystem::path& path, Entity parent)
    {
        std::vector<Entity> createdEntities;
        createdEntities.reserve(256);

        auto rollback = [&]() {
            for (auto it = createdEntities.rbegin(); it != createdEntities.rend(); ++it)
            {
                const Entity e = *it;
                if (e != INVALID_ENTITY)
                {
                    ecsScene.DestroyEntity(e);
                }
            }
            };

        try
        {
            AssetLoader loader;
            auto loadedScene = loader.LoadGLTF(path.string());

            if (loadedScene.nodes.empty() && loadedScene.subMeshes.empty())
            {
                MOKOLOG_WARN("GLTF [{}] contains no nodes and no submeshes.", path.string());
                return false;
            }

            if (loadedScene.vertices.empty())
            {
                MOKOLOG_ERROR("GLTF [{}] has no vertices.", path.string());
                return false;
            }

            if (loadedScene.indices.empty())
            {
                MOKOLOG_ERROR("GLTF [{}] has no indices.", path.string());
                return false;
            }

            BufferDesc vbDesc = {
                static_cast<uint32_t>(loadedScene.vertices.size() * sizeof(Mesh::Vertex)),
                sizeof(Mesh::Vertex),
                BufferUsage::Vertex,
                MemoryAccess::CpuWrite
            };

            auto vb = device.CreateBuffer(vbDesc, loadedScene.vertices.data());
            if (!vb.IsValid())
            {
                MOKOLOG_ERROR("Failed to create vertex buffer for [{}].", path.string());
                return false;
            }

            BufferDesc ibDesc = {
                static_cast<uint32_t>(loadedScene.indices.size() * sizeof(uint32_t)),
                sizeof(uint32_t),
                BufferUsage::Index,
                MemoryAccess::CpuWrite
            };

            auto ib = device.CreateBuffer(ibDesc, loadedScene.indices.data());
            if (!ib.IsValid())
            {
                MOKOLOG_ERROR("Failed to create index buffer for [{}].", path.string());
                return false;
            }

            std::vector<TextureHandle> gpuTextures;
            gpuTextures.reserve(loadedScene.textures.size());

            TextureHandle defaultWhite = BuiltinAssets::GetDefaultWhite();
            TextureHandle defaultNormal = BuiltinAssets::GetDefaultNormal();
            TextureHandle defaultMR = BuiltinAssets::GetDefaultMR();

            device.BeginTextureUpload();
            try
            {
                for (size_t i = 0; i < loadedScene.textures.size(); ++i)
                {
                    const auto& tex = loadedScene.textures[i];

                    TextureHandle handle{};
                    try
                    {
                        handle = device.LoadTexture(tex.path);
                        if (!handle.IsValid())
                        {
                            MOKOLOG_WARN("Texture load failed [{}] in scene [{}]. Fallback will be used.", MokoPath::ToString(tex.path), path.string());
                        }
                    }
                    catch (const std::exception& e)
                    {
                        MOKOLOG_WARN("Exception while loading texture [{}] in scene [{}]: {}", MokoPath::ToString(tex.path), path.string(), e.what());
                    }
                    catch (...)
                    {
                        MOKOLOG_WARN("Unknown exception while loading texture [{}] in scene [{}].", MokoPath::ToString(tex.path), path.string());
                    }

                    gpuTextures.push_back(handle);
                }
            }
            catch (...)
            {
                device.FlushTextureUploads();
                throw;
            }
            device.FlushTextureUploads();

            const Entity gltfRoot = (parent == INVALID_ENTITY) ? ecsScene.GetRoot() : parent;

            std::vector<Entity> nodeEntities(loadedScene.nodes.size(), INVALID_ENTITY);

            for (size_t i = 0; i < loadedScene.nodes.size(); ++i)
            {
                const auto& node = loadedScene.nodes[i];

                Entity e = ecsScene.CreateSceneEntity(node.name.empty() ? "GLTF_Node" : node.name);
                if (e == INVALID_ENTITY)
                {
                    MOKOLOG_ERROR("Failed to create entity for node {} in [{}].", i, path.string());
                    rollback();
                    return false;
                }

                createdEntities.push_back(e);
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
                const Entity child = nodeEntities[i];
                if (child == INVALID_ENTITY)
                    continue;

                if (node.parentIndex < 0)
                {
                    ecsScene.SetParent(child, gltfRoot);
                }
                else
                {
                    if (node.parentIndex >= static_cast<int>(nodeEntities.size()))
                    {
                        MOKOLOG_ERROR(
                            "Invalid parent index {} for node {} in [{}].",
                            node.parentIndex,
                            i,
                            path.string()
                        );
                        rollback();
                        return false;
                    }

                    Entity parentEntity = nodeEntities[node.parentIndex];
                    if (parentEntity == INVALID_ENTITY)
                    {
                        MOKOLOG_ERROR(
                            "Parent entity is invalid for node {} in [{}].",
                            i,
                            path.string()
                        );
                        rollback();
                        return false;
                    }

                    ecsScene.SetParent(child, parentEntity);
                }
            }

            for (size_t i = 0; i < loadedScene.subMeshes.size(); ++i)
            {
                const auto& subMesh = loadedScene.subMeshes[i];

                if (subMesh.indexCount <= 0)
                {
                    MOKOLOG_WARN("Skipping submesh [{}] with zero indexCount in [{}].", subMesh.name, path.string());
                    continue;
                }

                if (subMesh.indexOffset + subMesh.indexCount > loadedScene.indices.size())
                {
                    MOKOLOG_ERROR(
                        "Submesh [{}] has invalid index range (offset={}, count={}) in [{}].",
                        subMesh.name,
                        subMesh.indexOffset,
                        subMesh.indexCount,
                        path.string()
                    );
                    rollback();
                    return false;
                }

                Entity e = ecsScene.CreateSceneEntity(subMesh.name.empty() ? "GLTF_SubMesh" : subMesh.name);
                if (e == INVALID_ENTITY)
                {
                    MOKOLOG_ERROR("Failed to create submesh entity [{}] in [{}].", subMesh.name, path.string());
                    rollback();
                    return false;
                }

                createdEntities.push_back(e);

                if (subMesh.nodeIndex >= 0)
                {
                    if (subMesh.nodeIndex >= static_cast<int>(nodeEntities.size()))
                    {
                        MOKOLOG_ERROR(
                            "Submesh [{}] has invalid nodeIndex {} in [{}].",
                            subMesh.name,
                            subMesh.nodeIndex,
                            path.string()
                        );
                        rollback();
                        return false;
                    }

                    Entity parentEntity = nodeEntities[subMesh.nodeIndex];
                    if (parentEntity == INVALID_ENTITY)
                    {
                        MOKOLOG_ERROR(
                            "Submesh [{}] references invalid parent node entity in [{}].",
                            subMesh.name,
                            path.string()
                        );
                        rollback();
                        return false;
                    }

                    ecsScene.SetParent(e, parentEntity);
                }
                else
                {
                    ecsScene.SetParent(e, gltfRoot);
                }

                auto& t = ecsScene.GetRegistry().Get<TransformComponent>(e);
                t.dirty = true;

                auto& mr = ecsScene.GetRegistry().Add<MeshRendererComponent>(e);
                mr.vertexBuffer = vb;
                mr.indexBuffer = ib;
                mr.indexOffset = subMesh.indexOffset;
                mr.indexCount = subMesh.indexCount;
                mr.visible = true;
                mr.aabbMin = subMesh.aabbMin;
                mr.aabbMax = subMesh.aabbMax;

                if (subMesh.materialIndex < 0 || subMesh.materialIndex >= static_cast<int>(loadedScene.materials.size()))
                {
                    MOKOLOG_WARN(
                        "Submesh [{}] has invalid material index {} in [{}]. Using default material.",
                        subMesh.name,
                        subMesh.materialIndex,
                        path.string()
                    );

                    mr.material.baseColor = defaultWhite;
                    mr.material.normal = defaultNormal;
                    mr.material.metallicRoughness = defaultMR;
                    mr.material.alphaMode = AlphaMode::Opaque;
                    mr.material.alphaCutoff = 0.5f;
                    mr.material.metallicFactor = 1.0f;
                    mr.material.roughnessFactor = 1.0f;
                    continue;
                }

                const auto& mat = loadedScene.materials[subMesh.materialIndex];

                auto resolveTextureOrDefault = [&](int texIndex, TextureHandle fallback, const char* slotName) -> TextureHandle {
                    if (texIndex < 0)
                        return fallback;

                    if (texIndex >= static_cast<int>(gpuTextures.size()))
                    {
                        MOKOLOG_WARN(
                            "Material texture index out of range for submesh [{}], slot [{}], texIndex={} in [{}]. Using fallback.",
                            subMesh.name,
                            slotName,
                            texIndex,
                            path.string()
                        );
                        return fallback;
                    }

                    if (!gpuTextures[texIndex].IsValid())
                    {
                        MOKOLOG_WARN(
                            "Material texture invalid for submesh [{}], slot [{}], texIndex={} in [{}]. Using fallback.",
                            subMesh.name,
                            slotName,
                            texIndex,
                            path.string()
                        );
                        return fallback;
                    }

                    return gpuTextures[texIndex];
                    };

                mr.material.baseColor = resolveTextureOrDefault(mat.baseColorTexture, defaultWhite, "BaseColor");
                mr.material.normal = resolveTextureOrDefault(mat.normalTexture, defaultNormal, "Normal");
                mr.material.metallicRoughness = resolveTextureOrDefault(mat.metallicRoughnessTexture, defaultMR, "MetallicRoughness");
                mr.material.alphaMode = mat.alphaMode;
                mr.material.alphaCutoff = mat.alphaCutoff;
                mr.material.metallicFactor = mat.metallicFactor;
                mr.material.roughnessFactor = mat.roughnessFactor;
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

            MOKOLOG_INFO("Model [{}] loaded successfully.", path.filename().string());
            return true;
        }
        catch (const std::exception& e)
        {
            rollback();
            MOKOLOG_ERROR("Failed to load GLTF [{}]: {}", path.string(), e.what());
            return false;
        }
        catch (...)
        {
            rollback();
            MOKOLOG_ERROR("Failed to load GLTF [{}]: unknown exception", path.string());
            return false;
        }
    }
}