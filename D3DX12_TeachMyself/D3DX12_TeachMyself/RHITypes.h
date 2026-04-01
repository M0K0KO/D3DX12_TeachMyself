#pragma once
#include <cstdint>
#include <memory>
#include <vector>

enum class Format
{
	UNKNOWN,
	R8_UNORM,
	R8G8B8A8_UNORM,
	R16G16_FLOAT,
	R16G16B16A16_FLOAT,
	R16G16B16A16_SNORM,
	R32_FLOAT,
	R32G32_FLOAT,
	R32G32B32_FLOAT,
	R32G32B32A32_FLOAT,
	R16_UINT,
	R32_UINT,
	D24_UNORM_S8_UINT,
	D32_FLOAT,
	R32_TYPELESS
};

enum class BufferUsage
{
	Vertex,
	Index,
	Constant
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
	DepthStencil
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
	RootCBV
};

enum class ShaderVisibility
{
	Vertex,
	Pixel,
	All,
};

struct CBHandle
{
	unsigned long long gpuAddress;
};

struct BufferHandle 
{ 
	uint32_t id = UINT32_MAX;
	bool IsValid() const { return id != UINT32_MAX; }
};

struct TextureHandle 
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
	Format format;
	TextureUsage usage;
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
	TextureHandle baseColor;
	TextureHandle normal;
	TextureHandle metallicRoughness;
};

struct RootParamDesc
{
	RootParamType type = RootParamType::DescriptorTable;
	RangeType rangeType;
	uint32_t baseRegister;
	uint32_t numDescriptors = 1;
	ShaderVisibility visibility;
};

struct RootSignatureDesc
{
	std::vector<RootParamDesc> rootParamDescs;
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
};