#pragma once
#include "Material.h"
#include "TextureManager.h"

class MaterialManager
{
public:
	struct CreateDesc
	{
		TextureHandle baseColor;
		TextureHandle normal;
		TextureHandle metallicRoughness;
		MaterialFactors factors;
		AlphaMode alphaMode = AlphaMode::Opaque;
		float alphaCutoff = 0.5f;
		bool doubleSided = false;
	};

	MaterialManager(TextureManager* textures);
	~MaterialManager();

	void InitDefaults();

	MaterialHandle Create(const CreateDesc& desc);
	void Destroy(MaterialHandle h);

	GPUMaterial ToGPU(const Material& mat);

	Material* Get(MaterialHandle h) { return m_pool.Get(h); }
	const Material* Get(MaterialHandle h) const { return m_pool.Get(h); }

	TextureHandle DefaultBaseColor() const { return m_defaultBaseColor; }
	TextureHandle DefaultNormal()    const { return m_defaultNormal; }
	TextureHandle DefaultMR()        const { return m_defaultMR; }
private:
	TextureManager* m_textures;
	HandlePool<Material, MaterialTag> m_pool;

	TextureHandle m_defaultBaseColor;  
	TextureHandle m_defaultNormal;     
	TextureHandle m_defaultMR;         // (0, 1, 0) = R:occlusion, G:roughness=1, B:metallic=0
	TextureHandle m_defaultBlack;      
};