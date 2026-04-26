#include "stdafx.h"

#include "TextureManager.h"
#include "DirectXTex/DirectXTex.h"
#include "RHITypes.h"
#include "DX12Helpers.h"
#include "GraphicsDevice.h"
#include "MokoPath.h"
#include "Mesh.h"

using namespace DirectX;

namespace
{
	std::vector<uint8_t> EnsureRGBA(const std::vector<uint8_t>& src,
		int width, int height, int channels)
	{
		if (channels == 4) return src; 
		if (channels == 3)
		{
			std::vector<uint8_t> dst((size_t)width * height * 4);
			for (int i = 0; i < width * height; ++i)
			{
				dst[i * 4 + 0] = src[i * 3 + 0];
				dst[i * 4 + 1] = src[i * 3 + 1];
				dst[i * 4 + 2] = src[i * 3 + 2];
				dst[i * 4 + 3] = 255;
			}
			return dst;
		}
		return src;
	}

	Format DetermineFormat(int channels, int bytesPerChannel, bool sRGB)
	{
		if (bytesPerChannel == 1)
		{
			if (channels == 4) return sRGB ? Format::R8G8B8A8_UNORM_SRGB : Format::R8G8B8A8_UNORM;
			if (channels == 1) return Format::R8_UNORM;
		}
		if (bytesPerChannel == 2)
		{
			if (channels == 4) return Format::R16G16B16A16_UNORM;
		}
		return Format::UNKNOWN;
	}
}

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
			i % meta.mipLevels,
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

TextureHandle TextureManager::CreateRaw(const RawDesc& desc)
{
	SubresourceData sub{
			.data = desc.data,
			.rowPitch = (size_t)desc.width * DX12Helpers::GetBytesPerPixel(desc.format),
			.slicePitch = (size_t)desc.width * desc.height * DX12Helpers::GetBytesPerPixel(desc.format),
	};

	TextureInitDesc init{
		.desc = {
			.width = desc.width,
			.height = desc.height,
			.mipLevels = 1,
			.arraySize = 1,
			.format = desc.format,
			.usage = TextureUsage::ShaderResource,
			.isCubemap = false,
		},
		.subresources = std::span(&sub, 1),
	};

	GPUTextureHandle gpu = m_device->CreateTexture(init);
	if (!gpu.IsValid()) return {};

	Texture tex{
		.gpu = gpu,
		.width = desc.width,
		.height = desc.height,
		.mipLevels = 1,
		.format = desc.format,
		.sRGB = desc.sRGB,
	};
	return m_pool.Create(std::move(tex));
}

TextureHandle TextureManager::CreateFromEmbedded(const Mesh::Texture& tex, bool sRGB)
{
	std::vector<uint8_t> rgba;
	const void* data;
	int finalChannels;

	if (tex.channels == 3 && tex.bytesPerChannel == 1)
	{
		rgba = EnsureRGBA(tex.data, tex.width, tex.height, 3);
		data = rgba.data();
		finalChannels = 4;
	}
	else
	{
		data = tex.data.data();
		finalChannels = tex.channels;
	}

	Format format = DetermineFormat(finalChannels, tex.bytesPerChannel, sRGB);
	if (format == Format::UNKNOWN)
	{
		MOKOLOG_ERROR("Unsupported embedded texture format: ch={}, bpc={}",
			tex.channels, tex.bytesPerChannel);
		return {};
	}

	// 3. CreateRaw¿¡ À§ÀÓ
	RawDesc desc{
		.width = (uint32_t)tex.width,
		.height = (uint32_t)tex.height,
		.format = format,
		.data = data,
		.dataSize = (size_t)tex.width * tex.height * finalChannels * tex.bytesPerChannel,
		.sRGB = sRGB,
	};
	return CreateRaw(desc);
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


