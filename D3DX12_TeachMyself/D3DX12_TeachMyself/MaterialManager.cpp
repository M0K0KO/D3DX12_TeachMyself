#include "stdafx.h"
#include "MaterialManager.h"
#include "BuiltinAssets.h"
#include "MeshManager.h"

MaterialManager::MaterialManager(GraphicsDevice* device, TextureManager* textures)
    : m_device(device), m_textures(textures)
{
    BufferDesc desc{};
    desc.size = MAX_MATERIALS * sizeof(GPUMaterialData);
    desc.stride = sizeof(GPUMaterialData);
    desc.access = MemoryAccess::GpuOnly;
    desc.usage = BufferUsage::Structured;
    m_gpuBuffer = device->CreateBuffer(desc);
}

MaterialManager::~MaterialManager()
{}

void MaterialManager::InitDefaults()
{
    m_defaultBaseColor = BuiltinAssets::GetDefaultWhite();
    m_defaultNormal = BuiltinAssets::GetDefaultNormal();
    m_defaultMR = BuiltinAssets::GetDefaultMR();
    m_defaultBlack = BuiltinAssets::GetDefaultBlack();
}

MaterialHandle MaterialManager::Create(const CreateDesc & desc)
{
    Material mat;
    mat.baseColor = desc.baseColor.IsValid() ? desc.baseColor : BuiltinAssets::GetDefaultWhite();
    mat.normal = desc.normal.IsValid() ? desc.normal : BuiltinAssets::GetDefaultNormal();
    mat.metallicRoughness = desc.metallicRoughness.IsValid() ? desc.metallicRoughness : BuiltinAssets::GetDefaultMR();
    mat.emissive = desc.emissive.IsValid() ? desc.emissive : BuiltinAssets::GetDefaultBlack();
    mat.occlusion = desc.occlusion.IsValid() ? desc.occlusion : BuiltinAssets::GetDefaultWhite();

    mat.factors = desc.factors;

    mat.alphaMode = desc.alphaMode;
    mat.alphaCutoff = desc.alphaCutoff;
    return m_pool.Create(std::move(mat));
}

void MaterialManager::Destroy(MaterialHandle h)
{
    m_pool.Destroy(h);
}

void MaterialManager::UploadIfDirty(CommandContext& ctx)
{
    if (!m_dirty) return;

    const size_t count = m_pool.Size();
    if (count == 0)
    {
        m_dirty = false;
        return;
    }

    std::vector<GPUMaterialData> gpuData(count);

    m_pool.ForEach([&](MaterialHandle h, const Material& mat) {
        GPUMaterialData data{};
        data.baseColorIdx = GetSrvIndex(mat.baseColor);
        data.normalIdx = GetSrvIndex(mat.normal);
        data.mrIdx = GetSrvIndex(mat.metallicRoughness);
        data.emissiveIdx = GetSrvIndex(mat.emissive);
        data.occlusionIdx = GetSrvIndex(mat.occlusion);

        auto& baseColor = mat.factors.baseColorFactor;
        data.baseColorFactor = { baseColor.x, baseColor.y, baseColor.z, baseColor.w };
        auto& emissive = mat.factors.emissiveFactor;
        data.emissiveFactor = { emissive.x, emissive.y, emissive.z };

        data.occlusionStrength = mat.factors.occlusionStrength;
        data.metallicFactor = mat.factors.metallicFactor;
        data.roughnessFactor = mat.factors.roughnessFactor;
        data.alphaCutoff = mat.alphaCutoff;
        data.alphaMode = (uint32_t)mat.alphaMode;

        gpuData[h.index] = data;
        });

    m_device->UpdateBuffer(m_gpuBuffer, gpuData.data(), gpuData.size() * sizeof(GPUMaterialData), 0);
    m_dirty = false;
}

GPUMaterial MaterialManager::ToGPU(const Material& mat)  
{
    auto getGpuOrFallback = [&](TextureHandle h, TextureHandle fallback) -> GPUTextureHandle {
        TextureHandle resolved = h.IsValid() ? h : fallback;
        const Texture* t = m_textures->Get(resolved);
        return t ? t->gpu : GPUTextureHandle{};
        };

    GPUMaterial g{};
    g.baseColor = getGpuOrFallback(mat.baseColor, BuiltinAssets::GetDefaultWhite());
    g.normal = getGpuOrFallback(mat.normal, BuiltinAssets::GetDefaultNormal());
    g.metallicRoughness = getGpuOrFallback(mat.metallicRoughness, BuiltinAssets::GetDefaultMR());
    g.emissive = getGpuOrFallback(mat.emissive, BuiltinAssets::GetDefaultBlack());
    g.occlusion = getGpuOrFallback(mat.occlusion, BuiltinAssets::GetDefaultWhite());

    g.baseColorFactor = mat.factors.baseColorFactor;
    g.emissiveFactor = mat.factors.emissiveFactor;
    g.alphaMode = mat.alphaMode;
    g.alphaCutoff = mat.alphaCutoff;
    g.metallicFactor = mat.factors.metallicFactor;
    g.roughnessFactor = mat.factors.roughnessFactor;
    g.occlusionStrength = mat.factors.occlusionStrength;
    return g;
}

uint32_t MaterialManager::GetSrvIndex(TextureHandle h) const
{
    if (!h.IsValid()) return UINT32_MAX;

    const Texture* tex = m_textures->Get(h);
    if (!tex) return UINT32_MAX;

    return m_device->GetSRVHandle(tex->gpu).index;
}

uint32_t MaterialManager::GetMaterialBufferSrvIndex() const
{
    return m_device->GetSRVHandle(m_gpuBuffer).index;
}
