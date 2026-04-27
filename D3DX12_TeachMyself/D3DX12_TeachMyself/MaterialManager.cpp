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
    auto getGpu = [&](TextureHandle h) -> GPUTextureHandle {
        const Texture* t = m_textures->Get(h);
        return t ? t->gpu : GPUTextureHandle{};
        };

    GPUMaterial g{};
    g.baseColor = getGpu(mat.baseColor);
    g.normal = getGpu(mat.normal);
    g.metallicRoughness = getGpu(mat.metallicRoughness);
    g.alphaMode = mat.alphaMode;
    g.alphaCutoff = mat.alphaCutoff;
    g.metallicFactor = mat.factors.metallicFactor;
    g.roughnessFactor = mat.factors.roughnessFactor;
    return g;
}
