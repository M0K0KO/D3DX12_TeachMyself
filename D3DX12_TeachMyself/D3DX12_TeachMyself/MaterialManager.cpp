#include "stdafx.h"
#include "MaterialManager.h"
#include "BuiltinAssets.h"
#include "MeshManager.h"

MaterialManager::MaterialManager(TextureManager* textures)
    : m_textures(textures)
{}

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
