#include "stdafx.h"
#include "SceneFactory.h"
#include "TransformComponent.h"
#include "MeshRendererComponent.h"
#include "RHITypes.h"
#include "Mesh.h"
#include "DirectionalLightComponent.h"
#include "PointLightComponent.h"
#include "BuiltinAssets.h"
#include "MokoPath.h"
#include "AssetManager.h"


namespace SceneFactory
{
    namespace
    {
        GPUTextureHandle CreateTextureFromEmbedded(const Mesh::Texture& tex, GraphicsDevice& device)
        {
            if (!tex.embedded || tex.width <= 0 || tex.height <= 0 || tex.data.empty())
                return {};

            const size_t pixelCount = static_cast<size_t>(tex.width) * static_cast<size_t>(tex.height);
            if (pixelCount == 0)
                return {};
            if (tex.channels <= 0 || tex.channels > 4)
                return {};
            if (tex.bytesPerChannel <= 0)
                return {};

            constexpr int dstChannels = 4;
            std::vector<uint8_t> rgba(pixelCount * dstChannels);
            const size_t srcStride = static_cast<size_t>(tex.channels) * static_cast<size_t>(tex.bytesPerChannel);
            const size_t expectedSize = pixelCount * srcStride;
            if (tex.data.size() < expectedSize)
                return {};

            auto readAsUNorm8 = [&](const uint8_t* srcChannel) -> uint8_t {
                if (tex.bytesPerChannel == 1)
                    return srcChannel[0];

                if (tex.bytesPerChannel == 2)
                {
                    const uint16_t v16 = static_cast<uint16_t>(srcChannel[0]) |
                        (static_cast<uint16_t>(srcChannel[1]) << 8);
                    return static_cast<uint8_t>(v16 >> 8);
                }

                return srcChannel[0];
                };

            for (int y = 0; y < tex.height; y++)
            {
                const int srcY = (tex.height - 1) - y;

                for (int x = 0; x < tex.width; ++x)
                {
                    const size_t srcIndex = static_cast<size_t>(srcY) * static_cast<size_t>(tex.width) + static_cast<size_t>(x);
                    const uint8_t* src = tex.data.data() + (srcIndex * srcStride);

                    const size_t dstIndex = static_cast<size_t>(y) * static_cast<size_t>(tex.width) + static_cast<size_t>(x);
                    const size_t dstBase = dstIndex * dstChannels;

                    const uint8_t r = readAsUNorm8(src + (0 * tex.bytesPerChannel));
                    const uint8_t g = (tex.channels >= 2) ? readAsUNorm8(src + (1 * tex.bytesPerChannel)) : r;
                    const uint8_t b = (tex.channels >= 3) ? readAsUNorm8(src + (2 * tex.bytesPerChannel)) : r;
                    const uint8_t a = (tex.channels >= 4) ? readAsUNorm8(src + (3 * tex.bytesPerChannel)) : 255;

                    rgba[dstBase + 0] = r;
                    rgba[dstBase + 1] = g;
                    rgba[dstBase + 2] = b;
                    rgba[dstBase + 3] = a;
                }
            }

            SubresourceData sub{ rgba.data(), tex.width * dstChannels, tex.width * tex.height * dstChannels };
            TextureInitDesc desc = {
                {
                    static_cast<uint32_t>(tex.width),
                    static_cast<uint32_t>(tex.height),
                    1, 1,
                    Format::R8G8B8A8_UNORM,
                    TextureUsage::ShaderResource,
                    false
                },
                std::span<const SubresourceData>(&sub, 1)
            };

            return device.CreateTexture(desc);
        }
    }

	Entity CreateEmpty(EntityScene& scene, const std::string& name, Entity parent)
	{
		Entity e = scene.CreateSceneEntity(name);
		scene.SetParent(e, parent);
		
		MOKOLOG_INFO("Created Empty Entity");
		return e;
	}

    void AttachCubeMesh(Registry& reg, Entity e)
    {
        auto& mr = reg.Add<MeshRendererComponent>(e);
        const auto& cube = BuiltinAssets::GetCube();
        mr.vertexBuffer = cube.vb;
        mr.indexBuffer = cube.ib;
        mr.indexOffset = cube.indexOffset;
        mr.indexCount = cube.indexCount;
        mr.aabbMin = cube.aabbMin;
        mr.aabbMax = cube.aabbMax;

        mr.material = BuiltinAssets::GetDefaultMaterial();
        mr.visible = true;

        mr.source = { MeshSource::Type::Builtin, "Cube", 0 };
    }
	Entity CreateCube(EntityScene& scene, const std::string& name, Entity parent)
	{
		Entity e = scene.CreateSceneEntity(name);
		if (parent != INVALID_ENTITY)
		{
			scene.SetParent(e, parent);
		}
        AttachCubeMesh(scene.GetRegistry(), e);
		MOKOLOG_INFO("Created Cube");
		return e;
	}

    void AttachSphereMesh(Registry& reg, Entity e)
    {
        auto& mr = reg.Add<MeshRendererComponent>(e);
        const auto& sphere = BuiltinAssets::GetSphere();
        mr.vertexBuffer = sphere.vb;
        mr.indexBuffer = sphere.ib;
        mr.indexOffset = sphere.indexOffset;
        mr.indexCount = sphere.indexCount;
        mr.aabbMin = sphere.aabbMin;
        mr.aabbMax = sphere.aabbMax;

        mr.material = BuiltinAssets::GetDefaultMaterial();
        mr.visible = true;

        mr.source = { MeshSource::Type::Builtin, "Sphere", 0 };
    }
	Entity CreateSphere(EntityScene& scene, const std::string& name, Entity parent)
	{
		Entity e = scene.CreateSceneEntity(name);
		if (parent != INVALID_ENTITY)
		{
			scene.SetParent(e, parent);
		}
        AttachSphereMesh(scene.GetRegistry(), e);
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

    bool LoadGLTFToScene(EntityScene& ecsScene, AssetManager& assets, GraphicsDevice& device, const std::filesystem::path& path, Entity parent)
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
                MemoryAccess::GpuOnly
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
                MemoryAccess::GpuOnly
            };

            auto ib = device.CreateBuffer(ibDesc, loadedScene.indices.data());
            if (!ib.IsValid())
            {
                MOKOLOG_ERROR("Failed to create index buffer for [{}].", path.string());
                return false;
            }

            std::unordered_map<int, bool> texSrgb;
            for (const auto& mat : loadedScene.materials)
            {
                if (mat.baseColorTexture >= 0 && !texSrgb.count(mat.baseColorTexture))
                    texSrgb[mat.baseColorTexture] = true;
                if (mat.normalTexture >= 0 && !texSrgb.count(mat.normalTexture))
                    texSrgb[mat.normalTexture] = false;
                if (mat.metallicRoughnessTexture >= 0 && !texSrgb.count(mat.metallicRoughnessTexture))
                    texSrgb[mat.metallicRoughnessTexture] = false;
            }

            std::vector<TextureHandle> textureHandles;
            textureHandles.reserve(loadedScene.textures.size());

            for (size_t i = 0; i < loadedScene.textures.size(); ++i)
            {
                const auto& tex = loadedScene.textures[i];
                const bool sRGB = texSrgb.count((int)i) ? texSrgb[(int)i] : false;

                TextureHandle handle{};
                try
                {
                    if (tex.embedded)
                    {
                        auto* texMgr = &assets.Textures();
                        handle = assets.Textures().CreateFromEmbedded(tex, sRGB);
                        if (!handle.IsValid())
                        {
                            MOKOLOG_WARN("Embedded texture load failed (idx={}) in [{}]. Fallback will apply.",
                                i, path.string());
                        }
                    }
                    else
                    {
                        handle = assets.Textures().GetOrLoad(MokoPath::ToString(tex.path), sRGB);
                        if (!handle.IsValid())
                        {
                            MOKOLOG_WARN("Texture load failed [{}] in [{}]. Fallback will apply.",
                                MokoPath::ToString(tex.path), path.string());
                        }
                    }
                }
                catch (const std::exception& e)
                {
                    MOKOLOG_WARN("Texture load exception (idx={}) in [{}]: {}", i, path.string(), e.what());
                }
                catch (...)
                {
                    MOKOLOG_WARN("Unknown texture load exception (idx={}) in [{}].", i, path.string());
                }

                textureHandles.push_back(handle);
            }

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

            auto resolveTexture = [&](int texIndex) -> TextureHandle {
                if (texIndex < 0) return {};
                if (texIndex >= (int)textureHandles.size()) return {};
                return textureHandles[texIndex];  
                };

            for (size_t i = 0; i < loadedScene.subMeshes.size(); ++i)
            {
                const auto& subMesh = loadedScene.subMeshes[i];

                if (subMesh.indexCount <= 0)
                {
                    MOKOLOG_WARN("Skipping submesh [{}] with zero indexCount in [{}].",
                        subMesh.name, path.string());
                    continue;
                }
                if (subMesh.indexOffset + subMesh.indexCount > loadedScene.indices.size())
                {
                    MOKOLOG_ERROR("Submesh [{}] invalid index range in [{}].",
                        subMesh.name, path.string());
                    rollback(); return false;
                }

                Entity e = ecsScene.CreateSceneEntity(subMesh.name.empty() ? "GLTF_SubMesh" : subMesh.name);
                if (e == INVALID_ENTITY)
                {
                    MOKOLOG_ERROR("Failed to create submesh entity [{}] in [{}].",
                        subMesh.name, path.string());
                    rollback(); return false;
                }
                createdEntities.push_back(e);

                if (subMesh.nodeIndex >= 0)
                {
                    if (subMesh.nodeIndex >= (int)nodeEntities.size())
                    {
                        MOKOLOG_ERROR("Submesh [{}] invalid nodeIndex {} in [{}].",
                            subMesh.name, subMesh.nodeIndex, path.string());
                        rollback(); return false;
                    }
                    Entity parentEntity = nodeEntities[subMesh.nodeIndex];
                    if (parentEntity == INVALID_ENTITY)
                    {
                        MOKOLOG_ERROR("Submesh [{}] invalid parent node entity in [{}].",
                            subMesh.name, path.string());
                        rollback(); return false;
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
                mr.source = { MeshSource::Type::GLTF, path.generic_string(), (int)i };

                // Material Á¶¸³
                if (subMesh.materialIndex < 0 ||
                    subMesh.materialIndex >= (int)loadedScene.materials.size())
                {
                    MOKOLOG_WARN("Submesh [{}] invalid material index. Using default.", subMesh.name);
                    mr.material = BuiltinAssets::GetDefaultMaterial();
                    continue;
                }

                const auto& mat = loadedScene.materials[subMesh.materialIndex];

                MaterialManager::CreateDesc desc{};
                desc.baseColor = resolveTexture(mat.baseColorTexture);
                desc.normal = resolveTexture(mat.normalTexture);
                desc.metallicRoughness = resolveTexture(mat.metallicRoughnessTexture);
                desc.factors.metallicFactor = mat.metallicFactor;
                desc.factors.roughnessFactor = mat.roughnessFactor;
                desc.alphaMode = mat.alphaMode;
                desc.alphaCutoff = mat.alphaCutoff;

                mr.material = assets.Materials().Create(desc);
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