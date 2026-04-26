#include "stdafx.h"
#include "MaterialManager.h"
#include "BuiltinAssets.h"

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
    Material mat{
        .baseColor = desc.baseColor.IsValid() ? desc.baseColor : m_defaultBaseColor,
        .normal = desc.normal.IsValid() ? desc.normal : m_defaultNormal,
        .metallicRoughness = desc.metallicRoughness.IsValid() ? desc.metallicRoughness : m_defaultMR,
        .factors = desc.factors,
        .alphaMode = desc.alphaMode,
        .alphaCutoff = desc.alphaCutoff,
        .doubleSided = desc.doubleSided,
    };
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
