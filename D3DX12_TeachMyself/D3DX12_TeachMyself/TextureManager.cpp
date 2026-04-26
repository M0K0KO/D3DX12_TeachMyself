#include "stdafx.h"

#include "TextureManager.h"
#include "DirectXTex/DirectXTex.h"
#include "RHITypes.h"
#include "DX12Helpers.h"
#include "GraphicsDevice.h"
#include "MokoPath.h"

using namespace DirectX;

TextureManager::TextureManager(GraphicsDevice* device)
	: m_device(device)
{}

TextureManager::~TextureManager()
{
	m_pool.ForEach([this](TextureHandle, Texture& tex) {
		m_device->DestroyTexture(tex.gpu);
		});
	m_pathCache.clear();
}

TextureHandle TextureManager::Load(const LoadDesc & desc)
{
	ScratchImage image;
	TexMetadata meta;
	
	HRESULT hr = DirectX::LoadFromDDSFile(
		MokoPath::ToWString(desc.path).c_str(),
		DDS_FLAGS_NONE,
		&meta, image);

	if (FAILED(hr))
	{
		MOKOLOG_ERROR("DDS load failed: {}", desc.path);
		return {};
	}

	const uint32_t subCount = (uint32_t)(meta.mipLevels * meta.arraySize);
	std::vector<SubresourceData> subs(subCount);
	for (uint32_t i = 0; i < subCount; i++)
	{
		const DirectX::Image* img = image.GetImage(
			i * meta.mipLevels,
			i / meta.mipLevels, 0);

		subs[i] = {
			.data = img->pixels,
			.rowPitch = img->rowPitch,
			.slicePitch = img->slicePitch
		};
	}

	const bool isCubemap = (meta.miscFlags & DirectX::TEX_MISC_TEXTURECUBE) != 0;

	TextureInitDesc init{
		.desc = {
			.width = (uint32_t)meta.width,
			.height = (uint32_t)meta.height,
			.mipLevels = (uint32_t)meta.mipLevels,
			.arraySize = (uint32_t)meta.arraySize,
			.format = DX12Helpers::ToRHIFormat(meta.format),
			.usage = TextureUsage::ShaderResource,
			.isCubemap = isCubemap
		},
		.subresources = subs
	};

	GPUTextureHandle gpu = m_device->CreateTexture(init);
	if (!gpu.IsValid())
	{
		MOKOLOG_ERROR("CreateTexture failed: {}", desc.path);
		return {};
	}

	Texture tex{
	.gpu = gpu,
	.width = (uint32_t)meta.width,
	.height = (uint32_t)meta.height,
	.mipLevels = (uint32_t)meta.mipLevels,
	.format = DX12Helpers::ToRHIFormat(meta.format),
	.sRGB = desc.sRGB,
	};
	return m_pool.Create(std::move(tex));
}

TextureHandle TextureManager::GetOrLoad(const std::string& path, bool sRGB)
{
	if (auto it = m_pathCache.find(path); it != m_pathCache.end())
	{
		return it->second;
	}
	TextureHandle h = Load({ path, sRGB });
	if (h.IsValid())
	{
		m_pathCache[path] = h;
	}
	return h;
}

void TextureManager::Destroy(TextureHandle h)
{
	if (Texture* tex = m_pool.Get(h))
	{
		m_device->DestroyTexture(tex->gpu);
		m_pool.Destroy(h);
	}
}


