#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include <cstdint>

#include "RHITypes.h"

class GraphicsDevice;
class CommandContext;

enum class RGResourceType { Texture, Buffer };
struct ResourceRef { RGResourceType type; uint32_t index; };

struct RGTextureHandle
{
	uint32_t index	  = UINT32_MAX;
	uint32_t version  = 0;

	bool IsValid() const { return index != UINT32_MAX; }
};

struct RGBufferHandle
{
	uint32_t index = UINT32_MAX;
	uint32_t version = 0;

	bool IsValid() const { return index != UINT32_MAX; }
};

struct RGTextureDesc
{
	uint32_t		width	= 0;
	uint32_t		height	= 0;
	Format			format	= Format::R8G8B8A8_UNORM;
	TextureUsage	usage   = TextureUsage::ShaderResource;

	bool operator==(const RGTextureDesc& other) const
	{
		return width == other.width
			&& height == other.height
			&& format == other.format
			&& usage == other.usage;
	}
};

struct RGTextureDescHash
{
	size_t operator()(const RGTextureDesc& d) const
	{
		size_t h = 0;
		h ^= std::hash<uint32_t>()(d.width) + 0x9e3779b9 + (h << 6) + (h >> 2);
		h ^= std::hash<uint32_t>()(d.height) + 0x9e3779b9 + (h << 6) + (h >> 2);
		h ^= std::hash<int>()(static_cast<int>(d.format)) + 0x9e3779b9 + (h << 6) + (h >> 2);
		h ^= std::hash<int>()(static_cast<int>(d.usage)) + 0x9e3779b9 + (h << 6) + (h >> 2);
		return h;
	}
};

struct RGBufferDesc
{
	uint32_t    size = 0;
	uint32_t    stride = 0;
	BufferUsage usage = BufferUsage::None;

	bool operator==(const RGBufferDesc& other) const
	{
		return size == other.size
			&& stride == other.stride
			&& usage == other.usage;
	}
};

struct RGBufferDescHash
{
	size_t operator()(const RGBufferDesc& d) const
	{
		size_t h = 0;
		h ^= std::hash<uint32_t>()(d.size) + 0x9e3779b9 + (h << 6) + (h >> 2);
		h ^= std::hash<uint32_t>()(d.stride) + 0x9e3779b9 + (h << 6) + (h >> 2);
		h ^= std::hash<int>()(static_cast<int>(d.usage)) + 0x9e3779b9 + (h << 6) + (h >> 2);
		return h;
	}
};

// metadata of virtual resource -> Execute() will make it real
struct RGTexture
{
	RGTextureDesc desc;
	RGResourceState initialState;
	RGResourceState currentState;

	uint32_t refCount			= 0;
	uint32_t producerPassIndex  = UINT32_MAX;
	uint32_t firstUserIndex		= UINT32_MAX;
	uint32_t lastUserIndex		= UINT32_MAX;

	bool		  imported		 = false;
	GPUTextureHandle realizedHandle = {};
};

struct RGBuffer
{
	RGBufferDesc desc;
	RGResourceState initialState;
	RGResourceState currentState;

	uint32_t refCount = 0;
	uint32_t producerPassIndex = UINT32_MAX;
	uint32_t firstUserIndex = UINT32_MAX;
	uint32_t lastUserIndex = UINT32_MAX;

	bool		  imported = false;
	GPUBufferHandle realizedHandle = {};
};

struct TextureBarrierInfo
{
	RGTextureHandle handle;
	RGResourceState before;
	RGResourceState after;
};

struct BufferBarrierInfo
{
	RGBufferHandle  handle;
	RGResourceState before;
	RGResourceState after;
};

// declaration of a single pass, constructed on Setup API, culled will be set by Compile()
struct RGPass
{
	std::string                           name;
	std::vector<RGTextureHandle>          textureReads;
	std::vector<RGTextureHandle>          textureWrites;
	std::vector<RGBufferHandle>           bufferReads;
	std::vector<RGBufferHandle>           bufferWrites;

	std::unordered_map<uint32_t, RGResourceState> textureStates;
	std::unordered_map<uint32_t, RGResourceState> bufferStates;
	std::vector<TextureBarrierInfo> textureBarrierInfos;
	std::vector<BufferBarrierInfo>  bufferBarrierInfos;

	std::function<void(CommandContext&)>   executeFunc;

	uint32_t refCount = 0;
	bool     culled   = false;
};

// Callback Helper
class RGBuilder
{
public:
	void Read(RGTextureHandle handle, RGResourceState state);
	RGTextureHandle Write(RGTextureHandle handle, RGResourceState state);

	void Read(RGBufferHandle handle, RGResourceState state);
	RGBufferHandle Write(RGBufferHandle handle, RGResourceState state);
private:
	friend class RenderGraph;

	RenderGraph* m_graph     = nullptr;
	uint32_t	 m_passIndex = 0;
};

class RenderGraph
{
public:
	explicit RenderGraph(GraphicsDevice* device);

	RGTextureHandle CreateTexture(RGTextureDesc desc, RGResourceState state);
	RGTextureHandle ImportTexture(GPUTextureHandle existing, RGTextureDesc desc, RGResourceState state);
	RGBufferHandle CreateBuffer(RGBufferDesc desc, RGResourceState state);
	RGBufferHandle ImportBuffer(GPUBufferHandle existing, RGBufferDesc desc, RGResourceState state);

	void AddPass(
		std::string name, 
		std::function<void(RGBuilder&)> setupFunc, 
		std::function<void(CommandContext&)> executeFunc);

	// culling, barrier calculation
	void Compile();

	// make actual resource, execute the passes
	void Execute(CommandContext& ctx);

	// clear the internal state -> fresh build every frame
	void Clear();

	void DebugPrintPasses() const;
	void DebugPrintBarriers() const;
	
private:
	friend class RGBuilder;

	GPUTextureHandle RealizeResource(const RGTextureDesc& desc);
	GPUBufferHandle RealizeResource(const RGBufferDesc& desc);

	GraphicsDevice*			m_device	= nullptr;
	std::vector<RGTexture> m_textures;
	std::vector<RGBuffer> m_buffers;
	std::vector<RGPass>		m_passes;

	std::unordered_map<RGTextureDesc, std::vector<GPUTextureHandle>, RGTextureDescHash> m_textureResourcePool;
	std::unordered_map<RGBufferDesc, std::vector<GPUBufferHandle>, RGBufferDescHash> m_bufferResourcePool;

	std::vector<TextureBarrierInfo> m_textureEpilogueBarriers;
	std::vector<BufferBarrierInfo> m_bufferEpilogueBarriers;
};