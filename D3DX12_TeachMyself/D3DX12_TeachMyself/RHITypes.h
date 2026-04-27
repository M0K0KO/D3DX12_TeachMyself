#pragma once
#include "stdafx.h"

namespace
{
	struct float2 { float x, y; };
	struct float3 { float x, y, z; };
	struct float4 { float x, y, z, w; };
	struct float4x4 { float m[16]; };
}

enum class Format
{
	UNKNOWN,

	R8_UNORM,
	R8G8B8A8_UNORM,
	R8G8B8A8_UNORM_SRGB,

	R11G11B10_FLOAT,

	R16G16_FLOAT,
	R16G16B16A16_FLOAT,
	R16G16B16A16_SNORM,
	R16G16B16A16_UNORM,

	R32_FLOAT,
	R32G32_FLOAT,
	R32G32B32_FLOAT,
	R32G32B32A32_FLOAT,

	R16_UINT,
	R32_UINT,
	D24_UNORM_S8_UINT,
	D32_FLOAT,
	R32_TYPELESS,

	BC1_UNORM,
	BC1_UNORM_SRGB,
	BC3_UNORM,
	BC3_UNORM_SRGB,
	BC4_UNORM,
	BC5_UNORM,
	BC5_SNORM,
	BC6H_UF16,
	BC6H_SF16,
	BC7_UNORM,
	BC7_UNORM_SRGB,
};

enum class BufferUsage
{
	Vertex,
	Index,
	Constant,
	Structured
};

enum class MemoryAccess
{
	GpuOnly,
	CpuWrite
};

enum class TextureUsage
{
	ShaderResource,
	RenderTarget,
	DepthStencil,
	UnorderedAccess,
};

enum class Semantic
{
	POSITION,
	NORMAL,
	TANGENT,
	TEXCOORD,
	COLOR,
};

enum class ComparisonFunc
{
	Less,
	LessEqual,
	Equal,
};

enum class RGResourceState
{
	RenderTarget,
	DepthWrite,
	DepthRead,
	ShaderResource,
	UnorderedAccess,
	Present,
	CopyDest,
};

enum class RangeType
{
	SRV,
	UAV,
	CBV,
	Sampler
};

enum class RootParamType
{
	DescriptorTable,
	RootCBV,
	RootConstants,
};

enum class ShaderVisibility
{
	Vertex,
	Pixel,
	All,
};

enum class AlphaMode
{
	Opaque,
	Mask,
	Blend
};

enum class CullMode
{
	None, 
	Front, 
	Back
};

enum class SamplerFilter 
{ 
	Point,
	Bilinear, 
	Trilinear, 
	Anisotropic,
	Comparison
};

enum class SamplerAddressMode{ 
	Wrap, 
	Clamp,
	Border
};

struct CBHandle
{
	unsigned long long gpuAddress;
};

struct GPUBufferHandle 
{ 
	uint32_t id = UINT32_MAX;
	bool IsValid() const { return id != UINT32_MAX; }
};

struct GPUTextureHandle 
{
	uint32_t id = UINT32_MAX;
	bool IsValid() const { return id != UINT32_MAX; }
};

struct PipelineHandle
{
	uint32_t id = UINT32_MAX;
	bool IsValid() const { return id != UINT32_MAX; }
};

struct BufferDesc
{
	uint32_t size;
	uint32_t stride;
	BufferUsage usage;
	MemoryAccess access;
};

struct TextureDesc
{
	uint32_t width;
	uint32_t height;
	uint32_t mipLevels = 1;
	uint32_t arraySize = 1;
	Format format;
	TextureUsage usage = TextureUsage::ShaderResource;
	bool isCubemap = false;
};

struct SubresourceData 
{ 
	const void* data; 
	size_t rowPitch;
	size_t slicePitch; 
};

struct TextureInitDesc
{
	TextureDesc desc;
	std::span<const SubresourceData> subresources;
};

struct CubemapTextureDesc
{
	uint32_t width;
	uint32_t height;
	Format format;
	TextureUsage usage;
	uint32_t mipLevels = 1;
};

struct VertexAttribute
{
	Semantic semantic;
	Format format;
	uint32_t offset;
};

struct ShaderBytecode
{
	const void* data;
	uint64_t size;
	std::shared_ptr<void> blob;
};

struct GPUMaterial
{
	GPUTextureHandle baseColor;
	GPUTextureHandle normal;
	GPUTextureHandle metallicRoughness;
	GPUTextureHandle emissive;
	GPUTextureHandle occlusion;

	AlphaMode alphaMode = AlphaMode::Opaque;
	float alphaCutoff = 0.5f;
	float metallicFactor = 1.0f;
	float roughnessFactor = 1.0f;
	float occlusionStrength = 1.0f;
	float3 emissiveFactor = { 0.0f, 0.0f, 0.0f };
	float4 baseColorFactor = { 1.0f, 1.0f, 1.0f, 1.0f };
};

struct RootParamDesc
{
	RootParamType type = RootParamType::DescriptorTable;
	RangeType rangeType;
	uint32_t baseRegister;
	uint32_t numDescriptors = 1;
	ShaderVisibility visibility;
};

struct StaticSamplerDesc
{
	SamplerFilter filter = SamplerFilter::Anisotropic;
	SamplerAddressMode addressMode = SamplerAddressMode::Wrap;
	uint32_t shaderRegister = 0;
	ShaderVisibility visibility = ShaderVisibility::Pixel;
};

struct RootSignatureDesc
{
	bool allowIA = true;
	std::vector<RootParamDesc> rootParamDescs;
	std::vector<StaticSamplerDesc> staticSamplers;
};

struct PipelineDesc
{
	RootSignatureDesc rootSignatureDesc;

	ShaderBytecode vs;
	ShaderBytecode ps;
	std::vector<VertexAttribute> vertexAttributes;
	std::vector<Format> rtvFormats;
	Format dsvFormat;
	bool depthEnable;
	bool depthWrite;
	ComparisonFunc depthFunc;
	CullMode cullMode;
};

struct ComputePipelineDesc
{
	RootSignatureDesc rootSignatureDesc;
	ShaderBytecode cs;
};