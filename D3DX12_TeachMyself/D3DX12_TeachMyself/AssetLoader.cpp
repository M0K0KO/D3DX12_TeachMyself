// Define these only in *one* .cc file.
#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
// #define TINYGLTF_NOEXCEPTION // optional. disable exception handling.

#include "AssetLoader.h"
#include "DirectXTex/DirectXTex.h"

Mesh::Mesh AssetLoader::LoadGLTF(const std::string& path)
{
    tinygltf::Model model;
    tinygltf::TinyGLTF loader;
    std::string err, warn;

    bool ok = path.ends_with(".glb")
        ? loader.LoadBinaryFromFile(&model, &err, &warn, path)
        : loader.LoadASCIIFromFile(&model, &err, &warn, path);

    if (!ok)
        throw std::runtime_error("glTF load failed: " + err);

    Mesh::Mesh mesh;

    for (const auto& gltfMesh : model.meshes)
    {
        for (const auto& prim : gltfMesh.primitives)
        {
            if (prim.mode != TINYGLTF_MODE_TRIANGLES && prim.mode != -1)
                continue; // 삼각형 아니면 스킵

            OutputDebugStringA(
                ("mode=" + std::to_string(prim.mode)
                    + " indices=" + std::to_string(prim.indices) + "\n").c_str()
            );

            // ── 현재 서브메시의 시작 오프셋 기록 ──
            uint32_t vertexOffset = static_cast<uint32_t>(mesh.vertices.size());
            uint32_t indexOffset = static_cast<uint32_t>(mesh.indices.size());

            // ── POSITION (필수) ──
            auto posIt = prim.attributes.find("POSITION");
            if (posIt == prim.attributes.end())
                throw std::runtime_error("Primitive missing POSITION");

            const auto& posAcc = model.accessors[posIt->second];
            const auto* posPtr = reinterpret_cast<const float*>(GetBufferPointer(model, posAcc));
            size_t vertexCount = posAcc.count;

            // 정점 배열 확보
            mesh.vertices.resize(vertexOffset + vertexCount);

            for (size_t i = 0; i < vertexCount; ++i)
            {
                mesh.vertices[vertexOffset + i].position = {
                    posPtr[i * 3 + 0],
                    posPtr[i * 3 + 1],
                    posPtr[i * 3 + 2]
                };
            }

            // ── NORMAL (없으면 0,0,0) ──
            auto normIt = prim.attributes.find("NORMAL");
            if (normIt != prim.attributes.end())
            {
                const auto& normAcc = model.accessors[normIt->second];
                const auto* normPtr = reinterpret_cast<const float*>(GetBufferPointer(model, normAcc));

                for (size_t i = 0; i < vertexCount; ++i)
                {
                    mesh.vertices[vertexOffset + i].normal = {
                        normPtr[i * 3 + 0],
                        normPtr[i * 3 + 1],
                        normPtr[i * 3 + 2]
                    };
                }
            }

            // ── TEXCOORD_0 (없으면 0,0) ──
            auto uvIt = prim.attributes.find("TEXCOORD_0");
            if (uvIt != prim.attributes.end())
            {
                const auto& uvAcc = model.accessors[uvIt->second];
                const auto* uvPtr = reinterpret_cast<const float*>(GetBufferPointer(model, uvAcc));

                for (size_t i = 0; i < vertexCount; ++i)
                {
                    mesh.vertices[vertexOffset + i].uv = {
                        uvPtr[i * 2 + 0],
                        uvPtr[i * 2 + 1]
                    };
                }
            }

            // ── Indices ──
            if (prim.indices >= 0)
            {
                const auto& idxAcc = model.accessors[prim.indices];
                const uint8_t* idxPtr = GetBufferPointer(model, idxAcc);

                for (size_t i = 0; i < idxAcc.count; ++i)
                {
                    uint32_t index;
                    // componentType에 따라 읽는 크기가 다름
                    if (idxAcc.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT)
                        index = reinterpret_cast<const uint16_t*>(idxPtr)[i];
                    else
                        index = reinterpret_cast<const uint32_t*>(idxPtr)[i];

                    mesh.indices.push_back(vertexOffset + index); // 글로벌 인덱스로 변환
                }
            }
            else
            {
                for (uint32_t i = 0; i < vertexCount; ++i)
                    mesh.indices.push_back(vertexOffset + i);
            }

            // ── SubMesh 기록 ──
            mesh.subMeshes.push_back({
                indexOffset,
                static_cast<uint32_t>(mesh.indices.size() - indexOffset)
                });
        }
    }

    return mesh;
}

Mesh::TextureData AssetLoader::LoadTexture(const std::wstring& path)
{
    DirectX::ScratchImage image;
    DirectX::TexMetadata meta;

    HRESULT hr;
    if (path.ends_with(L".dds"))
        hr = DirectX::LoadFromDDSFile(path.c_str(), DirectX::DDS_FLAGS_NONE, &meta, image);
    else
        hr = DirectX::LoadFromWICFile(path.c_str(), DirectX::WIC_FLAGS_NONE, &meta, image);

    if (FAILED(hr))
        throw std::runtime_error("Texture load failed: " + std::to_string(hr));

    if (meta.format != DXGI_FORMAT_R8G8B8A8_UNORM)
    {
        DirectX::ScratchImage converted;
        DirectX::Convert(image.GetImages(), image.GetImageCount(), meta,
            DXGI_FORMAT_R8G8B8A8_UNORM, DirectX::TEX_FILTER_DEFAULT,
            DirectX::TEX_THRESHOLD_DEFAULT, converted);
        image = std::move(converted);
        meta = image.GetMetadata();
    }

    return { std::move(image), (uint32_t)meta.width, (uint32_t)meta.height, meta.format };
}

const uint8_t* AssetLoader::GetBufferPointer(const tinygltf::Model& model, const const tinygltf::Accessor& acc)
{
	const auto& bv = model.bufferViews[acc.bufferView];
	const auto& buf = model.buffers[bv.buffer];
	return buf.data.data() + bv.byteOffset + acc.byteOffset;
}
