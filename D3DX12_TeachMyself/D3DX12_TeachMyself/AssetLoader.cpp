// Define these only in *one* .cc file.
#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
// #define TINYGLTF_NOEXCEPTION // optional. disable exception handling.

#include "AssetLoader.h"
#include "DirectXTex/DirectXTex.h"
#include "JobSystem.h"

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
        auto& sceneNode = scene.nodes[nodeIndex];

        DirectX::XMMATRIX local = DirectX::XMMatrixIdentity();

        if (node.matrix.size() == 16)
        {
            local = DirectX::XMMATRIX(
                (float)node.matrix[0], (float)node.matrix[1], (float)node.matrix[2], (float)node.matrix[3],
                (float)node.matrix[4], (float)node.matrix[5], (float)node.matrix[6], (float)node.matrix[7],
                (float)node.matrix[8], (float)node.matrix[9], (float)node.matrix[10], (float)node.matrix[11],
                (float)node.matrix[12], (float)node.matrix[13], (float)node.matrix[14], (float)node.matrix[15]
            );

            XMVECTOR s, r, t;
            XMMatrixDecompose(&s, &r, &t, local);
            XMStoreFloat3(&sceneNode.scale, s);
            XMStoreFloat4(&sceneNode.rotation, r);
            XMStoreFloat3(&sceneNode.translation, t);
        }
        else
        {
            if (node.translation.size() == 3)
            {
                sceneNode.translation = {
                    (float)node.translation[0],
                    (float)node.translation[1],
                    (float)node.translation[2]
                };
            }
            if (node.rotation.size() == 4)
            {
                sceneNode.rotation = {
                    (float)node.rotation[0],
                    (float)node.rotation[1],
                    (float)node.rotation[2],
                    (float)node.rotation[3]
                };
            }
            if (node.scale.size() == 3)
            {
                sceneNode.scale = {
                    (float)node.scale[0],
                    (float)node.scale[1],
                    (float)node.scale[2]
                };
            }

            XMVECTOR s = XMLoadFloat3(&sceneNode.scale);
            XMVECTOR r = XMLoadFloat4(&sceneNode.rotation);
            XMVECTOR t = XMLoadFloat3(&sceneNode.translation);

            local =
                DirectX::XMMatrixScalingFromVector(s) *
                DirectX::XMMatrixRotationQuaternion(r) *
                DirectX::XMMatrixTranslationFromVector(t);
        }

        XMMATRIX world = local * parentMatrix;

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

                    scene.vertices[vertexOffset + i].position = { p[0], p[1], p[2] };

                    XMVECTOR localPos = XMVectorSet(p[0], p[1], p[2], 1.0f);
                    XMVECTOR worldPos = XMVector3TransformCoord(localPos, world);
                    XMFLOAT3 wp; XMStoreFloat3(&wp, worldPos);

                    primAABBMin.x = std::min(primAABBMin.x, wp.x);
                    primAABBMin.y = std::min(primAABBMin.y, wp.y);
                    primAABBMin.z = std::min(primAABBMin.z, wp.z);
                    primAABBMax.x = std::max(primAABBMax.x, wp.x);
                    primAABBMax.y = std::max(primAABBMax.y, wp.y);
                    primAABBMax.z = std::max(primAABBMax.z, wp.z);
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

                    for (size_t i = 0; i < vertexCount; ++i)
                    {
                        const float* n = reinterpret_cast<const float*>(base + i * stride);
                        scene.vertices[vertexOffset + i].normal = { n[0], n[1], n[2] };
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
                        scene.vertices[vertexOffset + i].tangent = { t[0], t[1], t[2], t[3] };
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
            ProcessNode(nodeIndex, XMMatrixIdentity());
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

Mesh::Scene AssetLoader::LoadGLTFParallel(const std::string& path, MokoJob::JobSystem& jobSys)
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

    // ---------------- Textures (serial, 가벼움) ----------------
    scene.textures.resize(model.images.size());
    std::filesystem::path baseDir = std::filesystem::path(path).parent_path();

    for (size_t i = 0; i < model.images.size(); i++)
    {
        const auto& img = model.images[i];
        if (!img.uri.empty())
        {
            auto p = baseDir / img.uri;
            p.replace_extension(".dds");
            scene.textures[i].path = p.wstring();
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

    // ---------------- Materials (serial, 가벼움) ----------------
    scene.materials.reserve(model.materials.size());
    for (const auto& gltfMat : model.materials)
    {
        Mesh::Material mat{};
        if (gltfMat.pbrMetallicRoughness.baseColorTexture.index >= 0)
            mat.baseColorTexture = model.textures[gltfMat.pbrMetallicRoughness.baseColorTexture.index].source;
        if (gltfMat.normalTexture.index >= 0)
            mat.normalTexture = model.textures[gltfMat.normalTexture.index].source;
        if (gltfMat.pbrMetallicRoughness.metallicRoughnessTexture.index >= 0)
            mat.metallicRoughnessTexture = model.textures[gltfMat.pbrMetallicRoughness.metallicRoughnessTexture.index].source;

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

    // ---------------- Nodes (serial, TRS + hierarchy) ----------------
    scene.nodes.resize(model.nodes.size());
    for (size_t i = 0; i < model.nodes.size(); i++)
    {
        const auto& gltfNode = model.nodes[i];
        auto& node = scene.nodes[i];
        node.name = gltfNode.name.empty() ? ("NodeA_" + std::to_string(i)) : gltfNode.name;
        node.children.reserve(gltfNode.children.size());
        for (int childIndex : gltfNode.children)
            node.children.push_back(childIndex);
    }
    for (size_t i = 0; i < scene.nodes.size(); i++)
    {
        for (int childIndex : scene.nodes[i].children)
        {
            if (childIndex >= 0 && childIndex < static_cast<int>(scene.nodes.size()))
                scene.nodes[childIndex].parentIndex = static_cast<int>(i);
        }
    }

    // =================================================================
    // PASS 1 (Serial DFS): TRS 계산 + primitive 수집 + offset 확정
    // =================================================================

    // 병렬 처리를 위해 primitive들을 flat list로 모음
    struct PrimitiveJob
    {
        int             nodeIndex;
        int             meshIndex;
        int             primIndex;           // gltfMesh.primitives 내 인덱스
        DirectX::XMMATRIX world;             // 사전 계산된 world matrix
        uint32_t        vertexOffset;        // 최종 scene.vertices 내 위치
        uint32_t        indexOffset;         // 최종 scene.indices 내 위치
        uint32_t        vertexCount;
        uint32_t        indexCount;          // prim.indices < 0이면 vertexCount
        int             subMeshIndex;        // scene.subMeshes 내 위치
    };

    std::vector<PrimitiveJob> primJobs;
    primJobs.reserve(model.meshes.size() * 2);  // 추정

    // 누적 offset
    uint32_t cumVertex = 0;
    uint32_t cumIndex = 0;

    std::function<void(int, DirectX::XMMATRIX)> CollectNode;
    CollectNode = [&](int nodeIndex, DirectX::XMMATRIX parentMatrix) {
        const auto& node = model.nodes[nodeIndex];
        auto& sceneNode = scene.nodes[nodeIndex];

        DirectX::XMMATRIX local = DirectX::XMMatrixIdentity();
        if (node.matrix.size() == 16)
        {
            local = DirectX::XMMATRIX(
                (float)node.matrix[0], (float)node.matrix[1], (float)node.matrix[2], (float)node.matrix[3],
                (float)node.matrix[4], (float)node.matrix[5], (float)node.matrix[6], (float)node.matrix[7],
                (float)node.matrix[8], (float)node.matrix[9], (float)node.matrix[10], (float)node.matrix[11],
                (float)node.matrix[12], (float)node.matrix[13], (float)node.matrix[14], (float)node.matrix[15]
            );
            XMVECTOR s, r, t;
            XMMatrixDecompose(&s, &r, &t, local);
            XMStoreFloat3(&sceneNode.scale, s);
            XMStoreFloat4(&sceneNode.rotation, r);
            XMStoreFloat3(&sceneNode.translation, t);
        }
        else
        {
            if (node.translation.size() == 3)
                sceneNode.translation = { (float)node.translation[0], (float)node.translation[1], (float)node.translation[2] };
            if (node.rotation.size() == 4)
                sceneNode.rotation = { (float)node.rotation[0], (float)node.rotation[1], (float)node.rotation[2], (float)node.rotation[3] };
            if (node.scale.size() == 3)
                sceneNode.scale = { (float)node.scale[0], (float)node.scale[1], (float)node.scale[2] };

            XMVECTOR s = XMLoadFloat3(&sceneNode.scale);
            XMVECTOR r = XMLoadFloat4(&sceneNode.rotation);
            XMVECTOR t = XMLoadFloat3(&sceneNode.translation);
            local = DirectX::XMMatrixScalingFromVector(s) *
                DirectX::XMMatrixRotationQuaternion(r) *
                DirectX::XMMatrixTranslationFromVector(t);
        }

        XMMATRIX world = local * parentMatrix;

        if (node.mesh >= 0)
        {
            const auto& gltfMesh = model.meshes[node.mesh];
            for (size_t p = 0; p < gltfMesh.primitives.size(); ++p)
            {
                const auto& prim = gltfMesh.primitives[p];
                if (prim.mode != TINYGLTF_MODE_TRIANGLES && prim.mode != -1)
                    continue;

                auto posIt = prim.attributes.find("POSITION");
                if (posIt == prim.attributes.end())
                    continue;

                uint32_t vCount = (uint32_t)model.accessors[posIt->second].count;
                uint32_t iCount = (prim.indices >= 0)
                    ? (uint32_t)model.accessors[prim.indices].count
                    : vCount;

                PrimitiveJob job{};
                job.nodeIndex = nodeIndex;
                job.meshIndex = node.mesh;
                job.primIndex = (int)p;
                job.world = world;
                job.vertexOffset = cumVertex;
                job.indexOffset = cumIndex;
                job.vertexCount = vCount;
                job.indexCount = iCount;

                // SubMesh 먼저 만들어두기 (이름/offset만, AABB는 Pass 2에서)
                Mesh::SubMesh subMesh{};
                std::string subMeshName = !node.name.empty() ? node.name
                    : (!gltfMesh.name.empty() ? gltfMesh.name
                        : ("SubMesh_" + std::to_string(scene.subMeshes.size())));
                subMesh.name = subMeshName;
                subMesh.indexOffset = cumIndex;
                subMesh.indexCount = iCount;
                subMesh.materialIndex = prim.material >= 0 ? prim.material : 0;
                subMesh.nodeIndex = nodeIndex;
                // aabbMin/Max는 Pass 2에서 채움

                job.subMeshIndex = (int)scene.subMeshes.size();
                scene.subMeshes.push_back(subMesh);
                scene.nodes[nodeIndex].subMeshIndices.push_back(job.subMeshIndex);

                primJobs.push_back(job);

                cumVertex += vCount;
                cumIndex += iCount;
            }
        }
        for (int child : node.children)
            CollectNode(child, world);
        };

    int sceneIndex = model.defaultScene >= 0 ? model.defaultScene : 0;
    if (sceneIndex >= 0 && sceneIndex < (int)model.scenes.size())
    {
        for (int nodeIndex : model.scenes[sceneIndex].nodes)
            CollectNode(nodeIndex, XMMatrixIdentity());
    }

    // 전체 size 미리 확정
    scene.vertices.resize(cumVertex);
    scene.indices.resize(cumIndex);

    // =================================================================
    // PASS 2 (Parallel): 각 primitive가 자기 영역에 독립적으로 write
    // =================================================================

    // per-primitive AABB 결과 저장 (병렬 write)
    struct PrimAABB { DirectX::XMFLOAT3 mn, mx; };
    std::vector<PrimAABB> primAABBs(primJobs.size());

    jobSys.ParallelFor(0, (int)primJobs.size(), 1, [&](int ji) {
        const PrimitiveJob& job = primJobs[ji];
        const auto& gltfMesh = model.meshes[job.meshIndex];
        const auto& prim = gltfMesh.primitives[job.primIndex];

        const uint32_t vOff = job.vertexOffset;
        const uint32_t iOff = job.indexOffset;
        const uint32_t vCount = job.vertexCount;

        // ---- POSITION + AABB ----
        auto posIt = prim.attributes.find("POSITION");
        const auto& posAcc = model.accessors[posIt->second];
        const auto& posView = model.bufferViews[posAcc.bufferView];
        const uint8_t* posBase = GetBufferPointer(model, posAcc);
        size_t posStride = posAcc.ByteStride(posView);

        DirectX::XMFLOAT3 aMin = { FLT_MAX,  FLT_MAX,  FLT_MAX };
        DirectX::XMFLOAT3 aMax = { -FLT_MAX, -FLT_MAX, -FLT_MAX };

        for (uint32_t i = 0; i < vCount; ++i)
        {
            const float* p = reinterpret_cast<const float*>(posBase + i * posStride);
            scene.vertices[vOff + i].position = { p[0], p[1], p[2] };

            XMVECTOR localPos = XMVectorSet(p[0], p[1], p[2], 1.0f);
            XMVECTOR worldPos = XMVector3TransformCoord(localPos, job.world);
            XMFLOAT3 wp; XMStoreFloat3(&wp, worldPos);

            aMin.x = std::min(aMin.x, wp.x); aMin.y = std::min(aMin.y, wp.y); aMin.z = std::min(aMin.z, wp.z);
            aMax.x = std::max(aMax.x, wp.x); aMax.y = std::max(aMax.y, wp.y); aMax.z = std::max(aMax.z, wp.z);
        }
        primAABBs[ji] = { aMin, aMax };

        // ---- NORMAL ----
        auto normIt = prim.attributes.find("NORMAL");
        if (normIt != prim.attributes.end())
        {
            const auto& acc = model.accessors[normIt->second];
            const auto& view = model.bufferViews[acc.bufferView];
            const uint8_t* base = GetBufferPointer(model, acc);
            size_t stride = acc.ByteStride(view);
            for (uint32_t i = 0; i < vCount; ++i)
            {
                const float* n = reinterpret_cast<const float*>(base + i * stride);
                scene.vertices[vOff + i].normal = { n[0], n[1], n[2] };
            }
        }

        // ---- TANGENT ----
        auto tanIt = prim.attributes.find("TANGENT");
        if (tanIt != prim.attributes.end())
        {
            const auto& acc = model.accessors[tanIt->second];
            const auto& view = model.bufferViews[acc.bufferView];
            const uint8_t* base = GetBufferPointer(model, acc);
            size_t stride = acc.ByteStride(view);
            for (uint32_t i = 0; i < vCount; ++i)
            {
                const float* t = reinterpret_cast<const float*>(base + i * stride);
                scene.vertices[vOff + i].tangent = { t[0], t[1], t[2], t[3] };
            }
        }

        // ---- UV ----
        auto uvIt = prim.attributes.find("TEXCOORD_0");
        if (uvIt != prim.attributes.end())
        {
            const auto& acc = model.accessors[uvIt->second];
            const auto& view = model.bufferViews[acc.bufferView];
            const uint8_t* base = GetBufferPointer(model, acc);
            size_t stride = acc.ByteStride(view);
            for (uint32_t i = 0; i < vCount; ++i)
            {
                const float* uv = reinterpret_cast<const float*>(base + i * stride);
                scene.vertices[vOff + i].uv = { uv[0], uv[1] };
            }
        }

        // ---- INDICES ----
        if (prim.indices >= 0)
        {
            const auto& idxAcc = model.accessors[prim.indices];
            const uint8_t* idxBase = GetBufferPointer(model, idxAcc);
            for (uint32_t i = 0; i < job.indexCount; ++i)
            {
                uint32_t index = 0;
                switch (idxAcc.componentType)
                {
                case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
                    index = reinterpret_cast<const uint8_t*>(idxBase)[i]; break;
                case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
                    index = reinterpret_cast<const uint16_t*>(idxBase)[i]; break;
                case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
                    index = reinterpret_cast<const uint32_t*>(idxBase)[i]; break;
                default:
                    // 병렬 컨텍스트에선 throw 대신 0으로 대체 (또는 에러 플래그)
                    index = 0; break;
                }
                scene.indices[iOff + i] = vOff + index;
            }
        }
        else
        {
            for (uint32_t i = 0; i < vCount; ++i)
                scene.indices[iOff + i] = vOff + i;
        }
        });

    // =================================================================
    // Pass 3 (Serial): SubMesh AABB 채우기 + scene AABB reduce
    // =================================================================
    scene.sceneAABBMin = { FLT_MAX,  FLT_MAX,  FLT_MAX };
    scene.sceneAABBMax = { -FLT_MAX, -FLT_MAX, -FLT_MAX };

    for (size_t ji = 0; ji < primJobs.size(); ++ji)
    {
        const auto& job = primJobs[ji];
        const auto& ab = primAABBs[ji];

        auto& sm = scene.subMeshes[job.subMeshIndex];
        sm.aabbMin = ab.mn;
        sm.aabbMax = ab.mx;

        scene.sceneAABBMin.x = std::min(scene.sceneAABBMin.x, ab.mn.x);
        scene.sceneAABBMin.y = std::min(scene.sceneAABBMin.y, ab.mn.y);
        scene.sceneAABBMin.z = std::min(scene.sceneAABBMin.z, ab.mn.z);
        scene.sceneAABBMax.x = std::max(scene.sceneAABBMax.x, ab.mx.x);
        scene.sceneAABBMax.y = std::max(scene.sceneAABBMax.y, ab.mx.y);
        scene.sceneAABBMax.z = std::max(scene.sceneAABBMax.z, ab.mx.z);
    }

    return scene;
}

void AssetLoader::FreeImage(float* data)
{
    stbi_image_free(data);
}
