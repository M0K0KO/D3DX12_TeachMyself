#pragma once
#include "Material.h"
#include "TextureManager.h"

#define MAX_MATERIALS 1024

class GraphicsDevice;

struct GPUMaterialData
{
	uint32_t baseColorIdx;
	uint32_t normalIdx;
	uint32_t mrIdx;
	uint32_t emissiveIdx;
	uint32_t occlusionIdx;
	uint32_t _pad0[3];               // 16-align

	XMFLOAT4 baseColorFactor;        // 16
	XMFLOAT3 emissiveFactor;
	float    occlusionStrength;      // 16
	float    metallicFactor;
	float    roughnessFactor;
	float    alphaCutoff;
	uint32_t alphaMode;              // 16
};

class MaterialManager
{
public:
	struct CreateDesc
	{
		TextureHandle baseColor;
		TextureHandle normal;
		TextureHandle metallicRoughness;
		TextureHandle emissive;
		TextureHandle occlusion;

		MaterialFactors factors;
		AlphaMode alphaMode = AlphaMode::Opaque;
		float alphaCutoff = 0.5f;
	};

	MaterialManager(GraphicsDevice* device, TextureManager* textures);
	~MaterialManager();

	void InitDefaults();

	MaterialHandle Create(const CreateDesc& desc);
	void Destroy(MaterialHandle h);

	void UploadIfDirty(CommandContext& ctx);
	GPUBufferHandle GetGPUBuffer() const { return m_gpuBuffer; }

	GPUMaterial ToGPU(const Material& mat);

	void SetDirty() { m_dirty = true; }
	Material* GetMutable(MaterialHandle h) {
		Material* m = m_pool.Get(h);
		if (m) m_dirty = true;
		return m;
	}
	Material* Get(MaterialHandle h) { return m_pool.Get(h); }
	const Material* Get(MaterialHandle h) const { return m_pool.Get(h); }

	TextureHandle DefaultBaseColor() const { return m_defaultBaseColor; }
	TextureHandle DefaultNormal()    const { return m_defaultNormal; }
	TextureHandle DefaultMR()        const { return m_defaultMR; }

private:
	HandlePool<Material, MaterialTag> m_pool;
	GPUBufferHandle m_gpuBuffer;
	uint32_t m_gpuCapacity = 0;
	bool m_dirty = true;

	uint32_t GetSrvIndex(TextureHandle h) const;
	uint32_t GetMaterialBufferSrvIndex() const;

	GraphicsDevice* m_device;
	TextureManager* m_textures;

	TextureHandle m_defaultBaseColor;  
	TextureHandle m_defaultNormal;     
	TextureHandle m_defaultMR;         // (0, 1, 0) = R:occlusion, G:roughness=1, B:metallic=0
	TextureHandle m_defaultBlack;      
};