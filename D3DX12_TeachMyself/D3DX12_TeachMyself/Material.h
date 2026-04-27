#pragma once
#include "HandlePool.h"
#include "RHITypes.h"

struct MaterialFactors
{
	float4 baseColorFactor = { 1,1,1,1 };
	float metallicFactor = 1.0f;
	float roughnessFactor = 1.0f;
	float3 emissiveFactor = { 1,1,1 };
	float occlusionStrength = 1.0f;
	float normalScale = 1.0f;
};

struct Material
{
	TextureHandle baseColor;
	TextureHandle normal;
	TextureHandle metallicRoughness;
	TextureHandle emissive;
	TextureHandle occlusion;

	MaterialFactors factors;

	AlphaMode alphaMode = AlphaMode::Opaque;
	float alphaCutoff = 0.5f;
	bool doubleSided = false;
};