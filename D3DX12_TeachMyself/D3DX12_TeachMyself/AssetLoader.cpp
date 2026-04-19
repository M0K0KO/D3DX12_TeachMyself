// Define these only in *one* .cc file.
#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
// #define TINYGLTF_NOEXCEPTION // optional. disable exception handling.

#include "AssetLoader.h"
#include "DirectXTex/DirectXTex.h"

#undef min
#undef max

using std::min;

Mesh::Scene AssetLoader::LoadGLTF(const std::string& path)
{
    tinygltf::Model model;
    tinygltf::TinyGLTF loader;
    std::string err, warn;

    bool ok = path.ends_with(".glb")
        ? loader.LoadBinaryFromFile(&model, &err, &warn, path)
        : loader.LoadASCIIFromFile(&model, &err, &warn, path);

    if (!warn.empty())
        OutputDebugStringA((warn + "\n").c_str());

    if (!ok)
        throw std::runtime_error("glTF load failed: " + err);

    Mesh::Scene scene;

    scene.textures.resize(model.images.size());

    std::filesystem::path baseDir = std::filesystem::path(path).parent_path();

    for (size_t i = 0; i < model.images.size(); i++)
    {
        const auto& img = model.images[i];
        
        if (!img.uri.empty())
        {
            auto path = baseDir / img.uri;
            path.replace_extension(".dds");

            scene.textures[i].path = path.wstring();
        }
        else
        {
            scene.textures[i].embedded = true;
            scene.textures[i].width = img.width;
            scene.textures[i].height = img.height;
            scene.textures[i].channels = img.component;
            scene.textures[i].data = img.image;
        }
    }

    scene.materials.reserve(model.materials.size());
    for (const auto& gltfMat : model.materials)
    {
        Mesh::Material mat{};

        if (gltfMat.pbrMetallicRoughness.baseColorTexture.index >= 0)
        {
            int texIndex = gltfMat.pbrMetallicRoughness.baseColorTexture.index;
            mat.baseColorTexture = model.textures[texIndex].source;
        }

        if (gltfMat.normalTexture.index >= 0)
        {
            int texIndex = gltfMat.normalTexture.index;
            mat.normalTexture = model.textures[texIndex].source;
        }

        if (gltfMat.pbrMetallicRoughness.metallicRoughnessTexture.index >= 0)
        {
            int texIndex = gltfMat.pbrMetallicRoughness.metallicRoughnessTexture.index;
            mat.metallicRoughnessTexture = model.textures[texIndex].source;
        }

        if (gltfMat.alphaMode == "MASK")
        {
            mat.alphaMode = AlphaMode::Mask;
            mat.alphaCutoff = static_cast<float>(gltfMat.alphaCutoff);
        }
        else if (gltfMat.alphaMode == "BLEND")
        {
            mat.alphaMode = AlphaMode::Blend;
        }

        mat.metallicFactor = static_cast<float>(gltfMat.pbrMetallicRoughness.metallicFactor);
        mat.roughnessFactor = static_cast<float>(gltfMat.pbrMetallicRoughness.roughnessFactor);

        scene.materials.push_back(mat);
    }

    if (scene.materials.empty())
        scene.materials.push_back({});

    scene.nodes.resize(model.nodes.size());
    for (size_t i = 0; i < model.nodes.size(); i++)
    {
        const auto& gltfNode = model.nodes[i];
        auto& node = scene.nodes[i];
        node.name = gltfNode.name.empty() ? ("NodeA_" + std::to_string(i)) : gltfNode.name;

        node.children.reserve(gltfNode.children.size());
        for (int childIndex : gltfNode.children)
        {
            node.children.push_back(childIndex);
        }
    }

    for (size_t i = 0; i < scene.nodes.size(); i++)
    {
        for (int childIndex : scene.nodes[i].children)
        {
            if (childIndex >= 0 && childIndex < static_cast<int>(scene.nodes.size()))
            {
                scene.nodes[childIndex].parentIndex = static_cast<int>(i);
            }
        }
    }

    std::function<void(int, DirectX::XMMATRIX)> ProcessNode;

    ProcessNode = [&](int nodeIndex, DirectX::XMMATRIX parentMatrix) {
        const auto& node = model.nodes[nodeIndex];

        DirectX::XMMATRIX local = DirectX::XMMatrixIdentity();

        if (node.matrix.size() == 16)
        {
            local = DirectX::XMMATRIX(
                (float)node.matrix[0], (float)node.matrix[1], (float)node.matrix[2], (float)node.matrix[3],
                (float)node.matrix[4], (float)node.matrix[5], (float)node.matrix[6], (float)node.matrix[7],
                (float)node.matrix[8], (float)node.matrix[9], (float)node.matrix[10], (float)node.matrix[11],
                (float)node.matrix[12], (float)node.matrix[13], (float)node.matrix[14], (float)node.matrix[15]
            );
        }
        else
        {
            DirectX::XMVECTOR scale = DirectX::XMVectorSet(1, 1, 1, 0);
            DirectX::XMVECTOR rotation = DirectX::XMQuaternionIdentity();
            DirectX::XMVECTOR translation = DirectX::XMVectorZero();

            if (node.scale.size() == 3)
            {
                scale = DirectX::XMVectorSet(
                    (float)node.scale[0],
                    (float)node.scale[1],
                    (float)node.scale[2],
                    0.0f
                );
            }

            if (node.rotation.size() == 4)
            {
                rotation = DirectX::XMVectorSet(
                    (float)node.rotation[0],
                    (float)node.rotation[1],
                    (float)node.rotation[2],
                    (float)node.rotation[3]
                );
            }

            if (node.translation.size() == 3)
            {
                translation = DirectX::XMVectorSet(
                    (float)node.translation[0],
                    (float)node.translation[1],
                    (float)node.translation[2],
                    1.0f
                );
            }

            local =
                DirectX::XMMatrixScalingFromVector(scale) *
                DirectX::XMMatrixRotationQuaternion(rotation) *
                DirectX::XMMatrixTranslationFromVector(translation);
        }

        DirectX::XMMATRIX world = local * parentMatrix;

        if (node.mesh >= 0)
        {
            const auto& gltfMesh = model.meshes[node.mesh];

            for (const auto& prim : gltfMesh.primitives)
            {
                if (prim.mode != TINYGLTF_MODE_TRIANGLES && prim.mode != -1)
                    continue;

                uint32_t vertexOffset = (uint32_t)scene.vertices.size();
                uint32_t indexOffset = (uint32_t)scene.indices.size();

                // ---------------- POSITION ----------------
                auto posIt = prim.attributes.find("POSITION");
                if (posIt == prim.attributes.end())
                    continue;

                const auto& posAcc = model.accessors[posIt->second];
                const auto& posView = model.bufferViews[posAcc.bufferView];
                const uint8_t* posBase = GetBufferPointer(model, posAcc);
                size_t posStride = posAcc.ByteStride(posView);

                size_t vertexCount = posAcc.count;
                scene.vertices.resize(vertexOffset + vertexCount);

                DirectX::XMFLOAT3 primAABBMin = { FLT_MAX, FLT_MAX, FLT_MAX };
                DirectX::XMFLOAT3 primAABBMax = { -FLT_MAX, -FLT_MAX, -FLT_MAX };

                for (size_t i = 0; i < vertexCount; ++i)
                {
                    const float* p = reinterpret_cast<const float*>(posBase + i * posStride);

                    DirectX::XMVECTOR pos = DirectX::XMVectorSet(p[0], p[1], p[2], 1.0f);
                    pos = DirectX::XMVector3TransformCoord(pos, world);

                    DirectX::XMStoreFloat3(&scene.vertices[vertexOffset + i].position, pos);
                    const auto& v = scene.vertices[vertexOffset + i].position;

                    primAABBMin.x = std::min(primAABBMin.x, v.x);
                    primAABBMin.y = std::min(primAABBMin.y, v.y);
                    primAABBMin.z = std::min(primAABBMin.z, v.z);
                    primAABBMax.x = std::max(primAABBMax.x, v.x);
                    primAABBMax.y = std::max(primAABBMax.y, v.y);
                    primAABBMax.z = std::max(primAABBMax.z, v.z);
                }

                scene.sceneAABBMin.x = std::min(scene.sceneAABBMin.x, primAABBMin.x);
                scene.sceneAABBMin.y = std::min(scene.sceneAABBMin.y, primAABBMin.y);
                scene.sceneAABBMin.z = std::min(scene.sceneAABBMin.z, primAABBMin.z);
                scene.sceneAABBMax.x = std::max(scene.sceneAABBMax.x, primAABBMax.x);
                scene.sceneAABBMax.y = std::max(scene.sceneAABBMax.y, primAABBMax.y);
                scene.sceneAABBMax.z = std::max(scene.sceneAABBMax.z, primAABBMax.z);

                // ---------------- NORMAL ----------------
                auto normIt = prim.attributes.find("NORMAL");
                if (normIt != prim.attributes.end())
                {
                    const auto& acc = model.accessors[normIt->second];
                    const auto& view = model.bufferViews[acc.bufferView];

                    const uint8_t* base = GetBufferPointer(model, acc);
                    size_t stride = acc.ByteStride(view);

                    DirectX::XMMATRIX normalMatrix =
                        DirectX::XMMatrixTranspose(
                            DirectX::XMMatrixInverse(nullptr, world));

                    for (size_t i = 0; i < vertexCount; ++i)
                    {
                        const float* n = reinterpret_cast<const float*>(base + i * stride);

                        DirectX::XMVECTOR normal =
                            DirectX::XMVectorSet(n[0], n[1], n[2], 0.0f);

                        normal = DirectX::XMVector3TransformNormal(normal, normalMatrix);
                        normal = DirectX::XMVector3Normalize(normal);

                        DirectX::XMStoreFloat3(
                            &scene.vertices[vertexOffset + i].normal,
                            normal);
                    }
                }

                // ---------------- TANGENT ----------------
                auto tanIt = prim.attributes.find("TANGENT");
                if (tanIt != prim.attributes.end())
                {
                    const auto& acc = model.accessors[tanIt->second];
                    const auto& view = model.bufferViews[acc.bufferView];

                    const uint8_t* base = GetBufferPointer(model, acc);
                    size_t stride = acc.ByteStride(view);

                    for (size_t i = 0; i < vertexCount; ++i)
                    {
                        const float* t = reinterpret_cast<const float*>(base + i * stride);
                        DirectX::XMVECTOR tangent = DirectX::XMVectorSet(t[0], t[1], t[2], 0.0f);
                        tangent = DirectX::XMVector3TransformNormal(tangent, world);
                        tangent = DirectX::XMVector3Normalize(tangent);

                        DirectX::XMStoreFloat3(
                            reinterpret_cast<DirectX::XMFLOAT3*>(
                                &scene.vertices[vertexOffset + i].tangent), tangent);
                        scene.vertices[vertexOffset + i].tangent.w = t[3]; 
                    }
                }

                // ---------------- UV ----------------
                auto uvIt = prim.attributes.find("TEXCOORD_0");
                if (uvIt != prim.attributes.end())
                {
                    const auto& acc = model.accessors[uvIt->second];
                    const auto& view = model.bufferViews[acc.bufferView];

                    const uint8_t* base = GetBufferPointer(model, acc);
                    size_t stride = acc.ByteStride(view);

                    for (size_t i = 0; i < vertexCount; ++i)
                    {
                        const float* uv = reinterpret_cast<const float*>(base + i * stride);

                        scene.vertices[vertexOffset + i].uv = {
                            uv[0], uv[1]
                        };
                    }
                }

                // ---------------- INDICES ----------------
                if (prim.indices >= 0)
                {
                    const auto& idxAcc = model.accessors[prim.indices];
                    const uint8_t* idxBase = GetBufferPointer(model, idxAcc);

                    for (size_t i = 0; i < idxAcc.count; ++i)
                    {
                        uint32_t index = 0;

                        switch (idxAcc.componentType)
                        {
                        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
                            index = reinterpret_cast<const uint8_t*>(idxBase)[i];
                            break;

                        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
                            index = reinterpret_cast<const uint16_t*>(idxBase)[i];
                            break;

                        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
                            index = reinterpret_cast<const uint32_t*>(idxBase)[i];
                            break;

                        default:
                            throw std::runtime_error("Unsupported index type");
                        }

                        scene.indices.push_back(vertexOffset + index);
                    }
                }
                else
                {
                    for (uint32_t i = 0; i < vertexCount; ++i)
                        scene.indices.push_back(vertexOffset + i);
                }

                Mesh::SubMesh subMesh{};

                std::string subMeshName;
                if (!node.name.empty())
                    subMeshName = node.name;
                else if (!gltfMesh.name.empty())
                    subMeshName = gltfMesh.name;
                else
                    subMeshName = "SubMesh_" + std::to_string(scene.subMeshes.size());
                subMesh.name = subMeshName;
                subMesh.indexOffset = indexOffset;
                subMesh.indexCount = (uint32_t)scene.indices.size() - indexOffset;
                subMesh.materialIndex = prim.material >= 0 ? prim.material : 0;
                subMesh.nodeIndex = nodeIndex;
                subMesh.aabbMin = primAABBMin;
                subMesh.aabbMax = primAABBMax;

                const int subMeshIndex = static_cast<int>(scene.subMeshes.size());
                scene.subMeshes.push_back(subMesh);
                scene.nodes[nodeIndex].subMeshIndices.push_back(subMeshIndex);
            }
        }
        for (int child : node.children)
            ProcessNode(child, world);
    };

    int sceneIndex = model.defaultScene >= 0 ? model.defaultScene : 0;

    if (sceneIndex >= 0 && sceneIndex < model.scenes.size())
    {
        for (int nodeIndex : model.scenes[sceneIndex].nodes)
            ProcessNode(nodeIndex, DirectX::XMMatrixIdentity());
    }

    return scene;
}

const uint8_t* AssetLoader::GetBufferPointer(const tinygltf::Model& model, const tinygltf::Accessor& acc)
{
    const auto& view = model.bufferViews[acc.bufferView];
    const auto& buffer = model.buffers[view.buffer];

    return buffer.data.data()
        + view.byteOffset
        + acc.byteOffset;
}

float* AssetLoader::LoadHDR(
    const std::string& path,
    int& width,
    int& height,
    int& channels,
    int desiredChannels)
{
    stbi_set_flip_vertically_on_load(true);

    float* data = stbi_loadf(
        path.c_str(),
        &width,
        &height,
        &channels,
        desiredChannels
    );

    if (!data)
    {
        throw std::runtime_error(
            "Failed to load HDR image: " + path
        );
    }

    if (desiredChannels != 0)
        channels = desiredChannels;

    return data;
}

void AssetLoader::FreeImage(float* data)
{
    stbi_image_free(data);
}
