#pragma once
#include "stdafx.h"
#include "HandlePool.h"
#include <unordered_map>
#include "Texture.h"

class GraphicsDevice;

class TextureManager
{
public:
	struct LoadDesc
	{
		std::string path;
		bool sRGB = false;
	};

	TextureManager(GraphicsDevice* device);
	~TextureManager();

	TextureHandle Load(const LoadDesc& desc);
	TextureHandle GetOrLoad(const std::string& path, bool sRGB);

	const Texture* Get(TextureHandle h) const { return m_pool.Get(h); };
	void Destroy(TextureHandle h);

private:
	GraphicsDevice* m_device;
	HandlePool<Texture, TextureTag> m_pool;
	std::unordered_map<std::string, TextureHandle> m_pathCache;
};