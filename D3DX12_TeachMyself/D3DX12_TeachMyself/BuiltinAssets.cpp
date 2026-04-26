#include "stdafx.h"
#include "BuiltinAssets.h"

#include <cmath>
#include <stdexcept>
#include <cassert>

#include "GraphicsDevice.h"
#include "MokoMath.h"
#include "AssetManager.h"

using namespace DirectX;

namespace
{
    struct MeshData
    {
        std::vector<Mesh::Vertex> vertices;
        std::vector<uint32_t>     indices;
    };

    inline MeshData MakeCubeMeshData()
    {
        MeshData m;
        const float h = 0.5f;

        auto addFace = [&](XMFLOAT3 n,
            XMFLOAT3 v0, XMFLOAT3 v1, XMFLOAT3 v2, XMFLOAT3 v3) {
                uint32_t base = static_cast<uint32_t>(m.vertices.size());

                m.vertices.push_back({ v0, n, {1,1,1,1}, {0,1} });
                m.vertices.push_back({ v1, n, {1,1,1,1}, {0,0} });
                m.vertices.push_back({ v2, n, {1,1,1,1}, {1,0} });
                m.vertices.push_back({ v3, n, {1,1,1,1}, {1,1} });

                m.indices.insert(m.indices.end(), {
                    base + 0, base + 1, base + 2,
                    base + 0, base + 2, base + 3
                    });
            };

        // +X
        addFace({ +1,0,0 },
            { +h,-h,-h }, { +h,+h,-h }, { +h,+h,+h }, { +h,-h,+h });

        // -X
        addFace({ -1,0,0 },
            { -h,-h,+h }, { -h,+h,+h }, { -h,+h,-h }, { -h,-h,-h });

        // +Y
        addFace({ 0,+1,0 },
            { -h,+h,-h }, { -h,+h,+h }, { +h,+h,+h }, { +h,+h,-h });

        // -Y
        addFace({ 0,-1,0 },
            { -h,-h,+h }, { -h,-h,-h }, { +h,-h,-h }, { +h,-h,+h });

        // +Z
        addFace({ 0,0,+1 },
            { +h,-h,+h }, { +h,+h,+h }, { -h,+h,+h }, { -h,-h,+h });

        // -Z
        addFace({ 0,0,-1 },
            { -h,-h,-h }, { -h,+h,-h }, { +h,+h,-h }, { +h,-h,-h });

        return m;
    }

    inline MeshData MakeSphereMeshData()
    {
        const uint32_t sliceCount = 32;
        const uint32_t stackCount = 16;
        const float radius = 0.5f;

        MeshData m;

        const float pi = 3.1415926535f;

        // Top pole
        m.vertices.push_back({
            {0.0f, +radius, 0.0f},
            {0.0f, +1.0f, 0.0f},
            {1,1,1,1},
            {0.0f, 0.0f}
            });

        // Rings
        for (uint32_t stack = 1; stack <= stackCount - 1; ++stack)
        {
            float phi = pi * stack / stackCount;

            for (uint32_t slice = 0; slice <= sliceCount; ++slice)
            {
                float theta = 2.0f * pi * slice / sliceCount;

                float x = radius * std::sin(phi) * std::cos(theta);
                float y = radius * std::cos(phi);
                float z = radius * std::sin(phi) * std::sin(theta);

                DirectX::XMFLOAT3 pos{ x, y, z };
                DirectX::XMFLOAT3 normal{
                    x / radius,
                    y / radius,
                    z / radius
                };

                float u = theta / (2.0f * pi);
                float v = phi / pi;

                m.vertices.push_back({
                    pos,
                    normal,
                    {1,1,1,1},
                    {u, v}
                    });
            }
        }

        // Bottom pole
        uint32_t bottomPoleIndex = static_cast<uint32_t>(m.vertices.size());

        m.vertices.push_back({
            {0.0f, -radius, 0.0f},
            {0.0f, -1.0f, 0.0f},
            {1,1,1,1},
            {0.0f, 1.0f}
            });

        const uint32_t ringVertexCount = sliceCount + 1;

        // Top cap - CCW viewed from outside
        for (uint32_t slice = 0; slice < sliceCount; ++slice)
        {
            m.indices.push_back(0);
            m.indices.push_back(slice + 2);
            m.indices.push_back(slice + 1);
        }

        // Middle
        for (uint32_t stack = 0; stack < stackCount - 2; ++stack)
        {
            for (uint32_t slice = 0; slice < sliceCount; ++slice)
            {
                uint32_t a = 1 + stack * ringVertexCount + slice;
                uint32_t b = 1 + stack * ringVertexCount + slice + 1;
                uint32_t c = 1 + (stack + 1) * ringVertexCount + slice;
                uint32_t d = 1 + (stack + 1) * ringVertexCount + slice + 1;

                m.indices.push_back(a);
                m.indices.push_back(b);
                m.indices.push_back(c);

                m.indices.push_back(b);
                m.indices.push_back(d);
                m.indices.push_back(c);
            }
        }

        // Bottom cap - CCW viewed from outside
        uint32_t lastRingStart = bottomPoleIndex - ringVertexCount;

        for (uint32_t slice = 0; slice < sliceCount; ++slice)
        {
            m.indices.push_back(bottomPoleIndex);
            m.indices.push_back(lastRingStart + slice);
            m.indices.push_back(lastRingStart + slice + 1);
        }

        return m;
    }
}

void BuiltinAssets::Initialize(GraphicsDevice& device, AssetManager* assets)
{
    auto& textures = assets->Textures();
    auto& materials = assets->Materials();
    auto& meshes = assets->Meshes();

    uint8_t white[4] = { 255, 255, 255, 255 };
    uint8_t normalUp[4] = { 128, 128, 255, 255 };
    uint8_t mr[4] = { 255, 255, 0, 255 };

    s_defaultWhite = textures.CreateRaw({
        .width = 1, .height = 1,
        .format = Format::R8G8B8A8_UNORM_SRGB,
        .data = white, .dataSize = 4, .sRGB = true
        });
    s_defaultNormal = textures.CreateRaw({
        .width = 1, .height = 1,
        .format = Format::R8G8B8A8_UNORM,
        .data = normalUp, .dataSize = 4, .sRGB = false
        });
    s_defaultMR = textures.CreateRaw({
        .width = 1, .height = 1,
        .format = Format::R8G8B8A8_UNORM,
        .data = mr, .dataSize = 4, .sRGB = false
        });


    MaterialManager::CreateDesc matDesc{};
    matDesc.baseColor = s_defaultWhite;
    matDesc.normal = s_defaultNormal;
    matDesc.metallicRoughness = s_defaultMR;
    matDesc.factors.metallicFactor = 0.0f;
    matDesc.factors.roughnessFactor = 1.0f;
    matDesc.alphaMode = AlphaMode::Opaque;
    s_defaultMaterial = materials.Create(matDesc);

    {
        auto cubeData = MakeCubeMeshData();
        std::vector<Submesh> submeshes = { Submesh{
            .indexOffset = 0,
            .indexCount = (uint32_t)cubeData.indices.size(),
            .materialSlot = 0,
            .aabb = AABB{ {-0.5f, -0.5f, -0.5f}, {0.5f, 0.5f, 0.5f} },
        } };

        MeshManager::CreateDesc cubeDesc{
            .vertices = cubeData.vertices.data(),
            .vertexCount = (uint32_t)cubeData.vertices.size(),
            .vertexStride = sizeof(Mesh::Vertex),
            .indices = cubeData.indices.data(),
            .indexCount = (uint32_t)cubeData.indices.size(),
            .submeshes = std::move(submeshes),
        };
        s_cubeMesh = meshes.Create(cubeDesc);
    }

    {
        auto sphereData = MakeSphereMeshData();
        std::vector<Submesh> submeshes = { Submesh{
            .indexOffset = 0,
            .indexCount = (uint32_t)sphereData.indices.size(),
            .materialSlot = 0,
            .aabb = AABB{ {-1, -1, -1}, {1, 1, 1} },
        } };

        MeshManager::CreateDesc sphereDesc{
            .vertices = sphereData.vertices.data(),
            .vertexCount = (uint32_t)sphereData.vertices.size(),
            .vertexStride = sizeof(Mesh::Vertex),
            .indices = sphereData.indices.data(),
            .indexCount = (uint32_t)sphereData.indices.size(),
            .submeshes = std::move(submeshes),
        };
        s_sphereMesh = meshes.Create(sphereDesc);
    }
}

void BuiltinAssets::Shutdown(GraphicsDevice& device, AssetManager* assets)
{
    auto& textures = assets->Textures();
    textures.Destroy(s_defaultWhite);
    textures.Destroy(s_defaultNormal);
    textures.Destroy(s_defaultMR);
}

MeshHandle BuiltinAssets::GetCubeMesh()
{
    return s_cubeMesh;
}

MeshHandle BuiltinAssets::GetSphereMesh()
{
    return s_sphereMesh;
}

TextureHandle BuiltinAssets::GetDefaultWhite()
{
    return s_defaultWhite;
}

TextureHandle BuiltinAssets::GetDefaultNormal()
{
    return s_defaultNormal;
}

TextureHandle BuiltinAssets::GetDefaultMR()
{
    return s_defaultMR;
}

MaterialHandle BuiltinAssets::GetDefaultMaterial()
{
    return s_defaultMaterial;
}
