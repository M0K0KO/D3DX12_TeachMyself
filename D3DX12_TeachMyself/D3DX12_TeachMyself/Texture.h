#pragma once
#include "stdafx.h"
#include "GraphicsDevice.h"

struct Texture
{
	GPUTextureHandle gpu;
	uint32_t width = 0;
	uint32_t height = 0;
	uint32_t mipLevels = 1;
	uint32_t arraySize = 1;
	Format format = Format::UNKNOWN;
	bool sRGB = false;
	bool isCubemap = false;
};