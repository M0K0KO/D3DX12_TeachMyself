#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include <cstdint>

#include "RHITypes.h"

class GraphicsDevice;
class CommandContext;

struct RGResourceHandle
{
	uint32_t index	  = UINT32_MAX;
	uint32_t version  = 0;

	bool IsValid() const { return index != UINT32_MAX; }
};

struct RGResourceDesc
{
	uint32_t		width	= 0;
	uint32_t		height	= 0;
	Format			format	= Format::R8G8B8A8_UNORM;
	TextureUsage	usage   = TextureUsage::ShaderResource;

	bool operator==(const RGResourceDesc& other) const
	{
		return width == other.width
			&& height == other.height
			&& format == other.format
			&& usage == other.usage;
	}
};

struct RGResourceDescHash
{
	size_t operator()(const RGResourceDesc& d) const
	{
		size_t h = 0;
		h ^= std::hash<uint32_t>()(d.width) + 0x9e3779b9 + (h << 6) + (h >> 2);
		h ^= std::hash<uint32_t>()(d.height) + 0x9e3779b9 + (h << 6) + (h >> 2);
		h ^= std::hash<int>()(static_cast<int>(d.format)) + 0x9e3779b9 + (h << 6) + (h >> 2);
		h ^= std::hash<int>()(static_cast<int>(d.usage)) + 0x9e3779b9 + (h << 6) + (h >> 2);
		return h;
	}
};

// metadata of virtual resource -> Execute() will make it real
struct RGResource
{
	RGResourceDesc desc;
	RGResourceState initialState;
	RGResourceState currentState;

	uint32_t refCount			= 0;
	uint32_t producerPassIndex  = UINT32_MAX;
	uint32_t firstUserIndex		= UINT32_MAX;
	uint32_t lastUserIndex		= UINT32_MAX;

	bool		  imported		 = false;
	GPUTextureHandle realizedHandle = {};
};

struct BarrierInfo
{
	RGResourceHandle handle;
	RGResourceState before;
	RGResourceState after;
};

// declaration of a single pass, constructed on Setup API, culled will be set by Compile()
struct RGPass
{
	std::string                            name;
	std::vector<RGResourceHandle>          reads;
	std::vector<RGResourceHandle>          writes;

	std::unordered_map<uint32_t, RGResourceState> resourceStates;
	std::vector<BarrierInfo> barrierInfos;

	std::function<void(CommandContext&)>   executeFunc;

	uint32_t refCount = 0;
	bool     culled   = false;
};

// Callback Helper
class RGBuilder
{
public:
	void Read(RGResourceHandle handle, RGResourceState state);
	RGResourceHandle Write(RGResourceHandle handle, RGResourceState state);
private:
	friend class RenderGraph;

	RenderGraph* m_graph     = nullptr;
	uint32_t	 m_passIndex = 0;
};

class RenderGraph
{
public:
	explicit RenderGraph(GraphicsDevice* device);

	RGResourceHandle CreateTexture(RGResourceDesc desc, RGResourceState state);
	RGResourceHandle ImportTexture(GPUTextureHandle existing, RGResourceDesc desc, RGResourceState state);
	void AddPass(
		std::string name, 
		std::function<void(RGBuilder&)> setupFunc, 
		std::function<void(CommandContext&)> executeFunc);

	// culling, barrier calculation
	void Compile();

	// make actual resource, execute the passes
	void Execute(CommandContext& ctx);
	const std::vector<RGPass>& GetPasses() const { return m_passes; }

	// clear the internal state -> fresh build every frame
	void Clear();

	void DebugPrintPasses() const;
	void DebugPrintBarriers() const;
	
private:
	friend class RGBuilder;

	GPUTextureHandle RealizeResource(const RGResourceDesc& desc);

	GraphicsDevice*			m_device	= nullptr;
	std::vector<RGResource> m_resources;
	std::vector<RGPass>		m_passes;

	std::unordered_map<RGResourceDesc, std::vector<GPUTextureHandle>, RGResourceDescHash> m_resourcePool;

	std::vector<BarrierInfo> m_epilogueBarriers;
};
