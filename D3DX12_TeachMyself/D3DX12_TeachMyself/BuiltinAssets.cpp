#include "BuiltinAssets.h"

#include <vector>
#include <cmath>
#include <numbers>
#include <stdexcept>
#include <cassert>
#include <cstdint>

#include "GraphicsDevice.h"
#include "MokoMath.h"

#include "MokoAssert.h"

using namespace DirectX;

namespace
{
    struct Vertex
    {
        XMFLOAT3 position;
        XMFLOAT3 normal;
        XMFLOAT4 tangent; 
        XMFLOAT2 uv;
    };

    struct CPUGeneratedMesh
    {
        std::vector<Vertex> vertices;
        std::vector<uint32_t> indices;
        XMFLOAT3 aabbMin{};
        XMFLOAT3 aabbMax{};
    };

    struct BuiltinStorage
    {
        bool initialized = false;

        BuiltinAssets::MeshData cube{};
        BuiltinAssets::MeshData sphere{};

        TextureHandle defaultWhite{};
        TextureHandle defaultNormal{};
        TextureHandle defaultMR{};
    };

    BuiltinStorage g_builtin;



    static CPUGeneratedMesh GenerateCube()
    {
        CPUGeneratedMesh mesh;
        mesh.aabbMin = XMFLOAT3(-0.5f, -0.5f, -0.5f);
        mesh.aabbMax = XMFLOAT3(0.5f, 0.5f, 0.5f);

        mesh.vertices.reserve(24);
        mesh.indices.reserve(36);

        auto addFace =
            [&](const XMFLOAT3& normal,
                const XMFLOAT4& tangent,
                const XMFLOAT3& v0,
                const XMFLOAT3& v1,
                const XMFLOAT3& v2,
                const XMFLOAT3& v3) {
                    const uint32_t base = static_cast<uint32_t>(mesh.vertices.size());

                    // UV:
                    // v0 = (0,1), v1 = (1,1), v2 = (1,0), v3 = (0,0)
                    mesh.vertices.push_back({ v0, normal, tangent, XMFLOAT2(0.0f, 1.0f) });
                    mesh.vertices.push_back({ v1, normal, tangent, XMFLOAT2(1.0f, 1.0f) });
                    mesh.vertices.push_back({ v2, normal, tangent, XMFLOAT2(1.0f, 0.0f) });
                    mesh.vertices.push_back({ v3, normal, tangent, XMFLOAT2(0.0f, 0.0f) });

                    mesh.indices.push_back(base + 0);
                    mesh.indices.push_back(base + 2);
                    mesh.indices.push_back(base + 1);

                    mesh.indices.push_back(base + 0);
                    mesh.indices.push_back(base + 3);
                    mesh.indices.push_back(base + 2);
            };

        // +X
        addFace(
            XMFLOAT3(1, 0, 0),
            XMFLOAT4(0, 0, -1, 1),
            XMFLOAT3(0.5f, -0.5f, -0.5f),
            XMFLOAT3(0.5f, -0.5f, 0.5f),
            XMFLOAT3(0.5f, 0.5f, 0.5f),
            XMFLOAT3(0.5f, 0.5f, -0.5f));

        // -X
        addFace(
            XMFLOAT3(-1, 0, 0),
            XMFLOAT4(0, 0, 1, 1),
            XMFLOAT3(-0.5f, -0.5f, 0.5f),
            XMFLOAT3(-0.5f, -0.5f, -0.5f),
            XMFLOAT3(-0.5f, 0.5f, -0.5f),
            XMFLOAT3(-0.5f, 0.5f, 0.5f));

        // +Y
        addFace(
            XMFLOAT3(0, 1, 0),
            XMFLOAT4(1, 0, 0, 1),
            XMFLOAT3(-0.5f, 0.5f, -0.5f),
            XMFLOAT3(0.5f, 0.5f, -0.5f),
            XMFLOAT3(0.5f, 0.5f, 0.5f),
            XMFLOAT3(-0.5f, 0.5f, 0.5f));

        // -Y
        addFace(
            XMFLOAT3(0, -1, 0),
            XMFLOAT4(1, 0, 0, 1),
            XMFLOAT3(-0.5f, -0.5f, 0.5f),
            XMFLOAT3(0.5f, -0.5f, 0.5f),
            XMFLOAT3(0.5f, -0.5f, -0.5f),
            XMFLOAT3(-0.5f, -0.5f, -0.5f));

        // +Z
        addFace(
            XMFLOAT3(0, 0, 1),
            XMFLOAT4(1, 0, 0, 1),
            XMFLOAT3(-0.5f, -0.5f, 0.5f),
            XMFLOAT3(-0.5f, 0.5f, 0.5f),
            XMFLOAT3(0.5f, 0.5f, 0.5f),
            XMFLOAT3(0.5f, -0.5f, 0.5f));

        // -Z
        addFace(
            XMFLOAT3(0, 0, -1),
            XMFLOAT4(-1, 0, 0, 1),
            XMFLOAT3(0.5f, -0.5f, -0.5f),
            XMFLOAT3(0.5f, 0.5f, -0.5f),
            XMFLOAT3(-0.5f, 0.5f, -0.5f),
            XMFLOAT3(-0.5f, -0.5f, -0.5f));

        return mesh;
    }

    // ------------------------------------------------------------
    // CPU mesh generation - Sphere (UV sphere)
    // ------------------------------------------------------------
    static CPUGeneratedMesh GenerateSphere(uint32_t stacks = 16, uint32_t slices = 16)
    {
        if (stacks < 2) stacks = 2;
        if (slices < 3) slices = 3;

        CPUGeneratedMesh mesh;
        mesh.aabbMin = XMFLOAT3(-0.5f, -0.5f, -0.5f);
        mesh.aabbMax = XMFLOAT3(0.5f, 0.5f, 0.5f);

        const float radius = 0.5f;
        const float pi = std::numbers::pi_v<float>;

        mesh.vertices.reserve((stacks + 1) * (slices + 1));
        mesh.indices.reserve(stacks * slices * 6);

        for (uint32_t stack = 0; stack <= stacks; ++stack)
        {
            const float v = static_cast<float>(stack) / static_cast<float>(stacks);
            const float phi = v * pi;

            const float y = std::cos(phi);
            const float ringR = std::sin(phi);

            for (uint32_t slice = 0; slice <= slices; ++slice)
            {
                const float u = static_cast<float>(slice) / static_cast<float>(slices);
                const float theta = u * (2.0f * pi);

                const float x = ringR * std::cos(theta);
                const float z = ringR * std::sin(theta);

                XMFLOAT3 normal = Normalize3(XMFLOAT3(x, y, z));
                XMFLOAT3 position = XMFLOAT3(normal.x * radius, normal.y * radius, normal.z * radius);

                // longitude πÊ«‚ tangent
                XMFLOAT3 tangent3 = XMFLOAT3(-std::sin(theta), 0.0f, std::cos(theta));
                if (LengthSq3(tangent3) < 1e-6f)
                {
                    tangent3 = XMFLOAT3(1.0f, 0.0f, 0.0f);
                }
                else
                {
                    tangent3 = Normalize3(tangent3);
                }

                mesh.vertices.push_back({
                    position,
                    normal,
                    XMFLOAT4(tangent3.x, tangent3.y, tangent3.z, 1.0f),
                    XMFLOAT2(u, v)
                    });
            }
        }

        const uint32_t row = slices + 1;

        for (uint32_t stack = 0; stack < stacks; ++stack)
        {
            for (uint32_t slice = 0; slice < slices; ++slice)
            {
                const uint32_t i0 = stack * row + slice;
                const uint32_t i1 = i0 + 1;
                const uint32_t i2 = i0 + row;
                const uint32_t i3 = i2 + 1;

                mesh.indices.push_back(i0);
                mesh.indices.push_back(i1);
                mesh.indices.push_back(i2);

                mesh.indices.push_back(i1);
                mesh.indices.push_back(i3);
                mesh.indices.push_back(i2);
            }
        }

        return mesh;
    }

    static BuiltinAssets::MeshData UploadMesh(GraphicsDevice& device, const CPUGeneratedMesh& cpu)
    {
        BuiltinAssets::MeshData out{};
        out.indexOffset = 0;
        out.indexCount = static_cast<uint32_t>(cpu.indices.size());
        out.aabbMin = cpu.aabbMin;
        out.aabbMax = cpu.aabbMax;

        BufferDesc vbDesc = {};
        vbDesc.size = uint32_t(sizeof(Vertex) * cpu.vertices.size());
        vbDesc.stride = sizeof(Vertex);
        vbDesc.usage = BufferUsage::Vertex;
        vbDesc.access = MemoryAccess::GpuOnly; 
        out.vb = device.CreateBuffer(vbDesc, cpu.vertices.data());
       
        BufferDesc ibDesc = {};
        ibDesc.size = uint32_t(sizeof(uint32_t) * cpu.indices.size());
        ibDesc.stride = sizeof(uint32_t);
        ibDesc.usage = BufferUsage::Index;
        ibDesc.access = MemoryAccess::GpuOnly; 
        out.ib = device.CreateBuffer(ibDesc, cpu.indices.data());

        return out;
    }

    static TextureHandle Create1x1TextureRGBA8(GraphicsDevice& device, uint8_t r, uint8_t g, uint8_t b, uint8_t a)
    {
        const uint8_t pixel[4] = { r, g, b, a };

        TextureDesc desc = {};
        desc.width = 1;
        desc.height = 1;
        desc.format = Format::R8G8B8A8_UNORM;
        desc.usage = TextureUsage::ShaderResource;
        return device.CreateTexture(desc, pixel);
    }

    static void DestroyMesh(GraphicsDevice& device, BuiltinAssets::MeshData& mesh)
    {
        if (mesh.vb.IsValid()) device.DestroyBuffer(mesh.vb);
        if (mesh.ib.IsValid()) device.DestroyBuffer(mesh.ib);

        mesh = {};
    }

    static void DestroyTexture(GraphicsDevice& device, TextureHandle& tex)
    {
        if (tex.IsValid()) device.DestroyTexture(tex);
        tex = {};
    }
}

void BuiltinAssets::Initialize(GraphicsDevice& device)
{
    if (g_builtin.initialized)
        return;

    try
    {
        g_builtin.cube = UploadMesh(device, GenerateCube());
        g_builtin.sphere = UploadMesh(device, GenerateSphere(32, 32));

        g_builtin.defaultWhite = Create1x1TextureRGBA8(device, 255, 255, 255, 255);
        g_builtin.defaultNormal = Create1x1TextureRGBA8(device, 128, 128, 255, 255);

        // MR packed:
        // R = metallic = 0
        // G = roughness = 1
        // B = unused = 0
        // A = unused = 255
        g_builtin.defaultMR = Create1x1TextureRGBA8(device, 0, 255, 0, 255);

        g_builtin.initialized = true;
    }
    catch (...)
    {
        DestroyMesh(device, g_builtin.cube);
        DestroyMesh(device, g_builtin.sphere);

        DestroyTexture(device, g_builtin.defaultWhite);
        DestroyTexture(device, g_builtin.defaultNormal);
        DestroyTexture(device, g_builtin.defaultMR);

        g_builtin.initialized = false;
        throw;
    }
}

void BuiltinAssets::Shutdown(GraphicsDevice& device)
{
    if (!g_builtin.initialized)
        return;

    DestroyMesh(device, g_builtin.cube);
    DestroyMesh(device, g_builtin.sphere);

    DestroyTexture(device, g_builtin.defaultWhite);
    DestroyTexture(device, g_builtin.defaultNormal);
    DestroyTexture(device, g_builtin.defaultMR);

    g_builtin.initialized = false;
}

bool BuiltinAssets::IsInitialized()
{
    return g_builtin.initialized;
}

const BuiltinAssets::MeshData& BuiltinAssets::GetCube()
{
    MOKO_ASSERT(g_builtin.initialized);
    return g_builtin.cube;
}

const BuiltinAssets::MeshData& BuiltinAssets::GetSphere()
{
    MOKO_ASSERT(g_builtin.initialized);
    return g_builtin.sphere;
}

TextureHandle BuiltinAssets::GetDefaultWhite()
{
    MOKO_ASSERT(g_builtin.initialized);
    return g_builtin.defaultWhite;
}

TextureHandle BuiltinAssets::GetDefaultNormal()
{
    MOKO_ASSERT(g_builtin.initialized);
    return g_builtin.defaultNormal;
}

TextureHandle BuiltinAssets::GetDefaultMR()
{
    MOKO_ASSERT(g_builtin.initialized);
    return g_builtin.defaultMR;
}