#include "stdafx.h"
#include "RenderGraph.h"
#include "GraphicsDevice.h"
#include "CommandContext.h"
#include <debugapi.h>
#include "MokoLogger.h"

void RGBuilder::Read(RGTextureHandle handle, RGResourceState state)
{
	auto& resource = m_graph->m_textures[handle.index];
	resource.refCount++;

	auto& pass = m_graph->m_passes[m_passIndex];
	pass.textureReads.push_back(handle);
	pass.textureStates[handle.index] = state;

	if (resource.firstUserIndex == UINT32_MAX)
	{
		resource.firstUserIndex = m_passIndex;
	}
	resource.lastUserIndex = m_passIndex;
}

RGTextureHandle RGBuilder::Write(RGTextureHandle handle, RGResourceState state)
{
	auto& resource = m_graph->m_textures[handle.index];
	resource.producerPassIndex = m_passIndex;
	
	auto& pass = m_graph->m_passes[m_passIndex];
	pass.textureWrites.push_back(handle);
	pass.textureStates[handle.index] = state;
	pass.refCount++;

	if (resource.firstUserIndex == UINT32_MAX)
	{
		resource.firstUserIndex = m_passIndex;
	}
	resource.lastUserIndex = m_passIndex;

	RGTextureHandle newHandle = handle;
	newHandle.version++;
	return newHandle;
}

void RGBuilder::Read(RGBufferHandle handle, RGResourceState state)
{
	auto& resource = m_graph->m_buffers[handle.index];
	resource.refCount++;

	auto& pass = m_graph->m_passes[m_passIndex];
	pass.bufferReads.push_back(handle);
	pass.bufferStates[handle.index] = state;

	if (resource.firstUserIndex == UINT32_MAX)
	{
		resource.firstUserIndex = m_passIndex;
	}
	resource.lastUserIndex = m_passIndex;
}

RGBufferHandle RGBuilder::Write(RGBufferHandle handle, RGResourceState state)
{
	auto& resource = m_graph->m_buffers[handle.index];
	resource.producerPassIndex = m_passIndex;

	auto& pass = m_graph->m_passes[m_passIndex];
	pass.bufferWrites.push_back(handle);
	pass.bufferStates[handle.index] = state;
	pass.refCount++;

	if (resource.firstUserIndex == UINT32_MAX)
	{
		resource.firstUserIndex = m_passIndex;
	}
	resource.lastUserIndex = m_passIndex;

	RGBufferHandle newHandle = handle;
	newHandle.version++;
	return newHandle;
}





RenderGraph::RenderGraph(GraphicsDevice* device)
	:
	m_device(device)
{
}

RGTextureHandle RenderGraph::CreateTexture(RGTextureDesc desc, RGResourceState state)
{
	RGTexture resource = {};
	resource.desc = desc;
	resource.initialState = state;
	resource.currentState = state;

	uint32_t index = static_cast<uint32_t>(m_textures.size());
	m_textures.push_back(resource);

	return RGTextureHandle{ index, 0 };
}

RGTextureHandle RenderGraph::ImportTexture(GPUTextureHandle existing, RGTextureDesc desc, RGResourceState state)
{
	RGTexture resource = {};
	resource.desc = desc;
	resource.initialState = state;
	resource.currentState = state;
	resource.imported = true;
	resource.realizedHandle = existing;

	uint32_t index = static_cast<uint32_t>(m_textures.size());
	m_textures.push_back(resource);

	return RGTextureHandle{ index, 0 };
}

RGBufferHandle RenderGraph::CreateBuffer(RGBufferDesc desc, RGResourceState state)
{
	RGBuffer resource = {};
	resource.desc = desc;
	resource.initialState = state;
	resource.currentState = state;

	uint32_t index = static_cast<uint32_t>(m_buffers.size());
	m_buffers.push_back(resource);

	return RGBufferHandle{ index, 0 };
}

RGBufferHandle RenderGraph::ImportBuffer(GPUBufferHandle existing, RGBufferDesc desc, RGResourceState state)
{
	RGBuffer resource = {};
	resource.desc = desc;
	resource.initialState = state;
	resource.currentState = state;
	resource.imported = true;
	resource.realizedHandle = existing;

	uint32_t index = static_cast<uint32_t>(m_buffers.size());
	m_buffers.push_back(resource);

	return RGBufferHandle{ index, 0 };
}

void RenderGraph::AddPass(std::string name, std::function<void(RGBuilder&)> setupFunc, std::function<void(CommandContext&)> exectueFunc)
{
	RGPass pass = {};
	pass.name = name;
	pass.executeFunc = exectueFunc;

	uint32_t passIndex = static_cast<uint32_t>(m_passes.size());
	m_passes.push_back(pass);

	RGBuilder builder;
	builder.m_graph = this;
	builder.m_passIndex = passIndex;

	setupFunc(builder);
}

void RenderGraph::Compile()
{
	for (auto& pass : m_passes)
	{
		for (auto& writeHandle : pass.textureWrites)
		{
			auto& resource = m_textures[writeHandle.index];
			if (resource.imported)
			{
				pass.refCount++;
			}
		}

		for (auto& writeHandle : pass.bufferWrites)
		{
			auto& resource = m_buffers[writeHandle.index];
			if (resource.imported)
			{
				pass.refCount++;
			}
		}
	}

	std::vector<ResourceRef> stack;

	for (uint32_t i = 0; i < m_textures.size(); i++)
	{
		if (!m_textures[i].imported && m_textures[i].refCount == 0)
			stack.push_back({ RGResourceType::Texture, i });
	}
	for (uint32_t i = 0; i < m_buffers.size(); i++)
	{
		if (!m_buffers[i].imported && m_buffers[i].refCount == 0)
			stack.push_back({ RGResourceType::Buffer, i });
	}

	while (!stack.empty())
	{
		auto ref = stack.back(); stack.pop_back();

		uint32_t producerIdx = (ref.type == RGResourceType::Texture)
			? m_textures[ref.index].producerPassIndex
			: m_buffers[ref.index].producerPassIndex;

		if (producerIdx == UINT32_MAX) continue;

		auto& producer = m_passes[producerIdx];
		producer.refCount--;
		if (producer.refCount == 0)
		{
			producer.culled = true;

			for (auto& h : producer.textureReads)
			{
				auto& r = m_textures[h.index];
				r.refCount--;
				if (r.refCount == 0 && !r.imported)
					stack.push_back({ RGResourceType::Texture, h.index });
			}
			for (auto& h : producer.bufferReads)
			{
				auto& r = m_buffers[h.index];
				r.refCount--;
				if (r.refCount == 0 && !r.imported)
					stack.push_back({ RGResourceType::Buffer, h.index });
			}
		}
	}

	for (auto& res : m_textures) res.currentState = res.initialState;
	for (auto& res : m_buffers)  res.currentState = res.initialState;

	for (auto& pass : m_passes)
	{
		if (pass.culled) continue;

		auto checkTextureBarrier = [&](RGTextureHandle handle) {
			auto& res = m_textures[handle.index];
			auto required = pass.textureStates[handle.index];
			if (res.currentState != required)
			{
				pass.textureBarrierInfos.push_back({ handle, res.currentState, required });
				res.currentState = required;
			}
			};

		auto checkBufferBarrier = [&](RGBufferHandle handle) {
			auto& res = m_buffers[handle.index];
			auto required = pass.bufferStates[handle.index];
			if (res.currentState != required)
			{
				pass.bufferBarrierInfos.push_back({ handle, res.currentState, required });
				res.currentState = required;
			}
			};

		for (auto& h : pass.textureReads)   checkTextureBarrier(h);
		for (auto& h : pass.textureWrites)  checkTextureBarrier(h);
		for (auto& h : pass.bufferReads)    checkBufferBarrier(h);
		for (auto& h : pass.bufferWrites)   checkBufferBarrier(h);
	}

	for (uint32_t i = 0; i < m_textures.size(); i++)
	{
		auto& res = m_textures[i];
		if (res.imported && res.currentState != res.initialState)
		{
			RGTextureHandle handle{ i, 0 };
			m_textureEpilogueBarriers.push_back({ handle, res.currentState, res.initialState });
		}
	}

	for (uint32_t i = 0; i < m_buffers.size(); i++)
	{
		auto& res = m_buffers[i];
		if (res.imported && res.currentState != res.initialState)
		{
			RGBufferHandle handle{ i, 0 };
			m_bufferEpilogueBarriers.push_back({ handle, res.currentState, res.initialState });
		}
	}
}

void RenderGraph::Execute(CommandContext& ctx)
{
	// 1. Realize
	for (auto& pass : m_passes)
	{
		if (pass.culled) continue;

		auto realizeTexture = [&](RGTextureHandle h) {
			auto& r = m_textures[h.index];
			if (!r.imported && !r.realizedHandle.IsValid())
				r.realizedHandle = RealizeResource(r.desc);
			};
		auto realizeBuffer = [&](RGBufferHandle h) {
			auto& r = m_buffers[h.index];
			if (!r.imported && !r.realizedHandle.IsValid())
				r.realizedHandle = RealizeResource(r.desc);
			};

		for (auto& h : pass.textureWrites) realizeTexture(h);
		for (auto& h : pass.textureReads)  realizeTexture(h);
		for (auto& h : pass.bufferWrites)  realizeBuffer(h);
		for (auto& h : pass.bufferReads)   realizeBuffer(h);
	}

	// 2. Execute
	for (auto& pass : m_passes)
	{
		if (pass.culled) continue;

		for (auto& bi : pass.textureBarrierInfos)
		{
			auto& res = m_textures[bi.handle.index];
			ctx.TransitionBarrier(res.realizedHandle, bi.before, bi.after);
		}
		for (auto& bi : pass.bufferBarrierInfos)
		{
			auto& res = m_buffers[bi.handle.index];
			ctx.TransitionBarrier(res.realizedHandle, bi.before, bi.after);
		}

		pass.executeFunc(ctx);
	}

	// 3. Epilogue
	for (auto& bi : m_textureEpilogueBarriers)
	{
		auto& res = m_textures[bi.handle.index];
		ctx.TransitionBarrier(res.realizedHandle, bi.before, bi.after);
	}
	for (auto& bi : m_bufferEpilogueBarriers)
	{
		auto& res = m_buffers[bi.handle.index];
		ctx.TransitionBarrier(res.realizedHandle, bi.before, bi.after);
	}
}

GPUTextureHandle RenderGraph::RealizeResource(const RGTextureDesc& desc)
{
	auto& pool = m_textureResourcePool[desc];

	if (!pool.empty())
	{
		GPUTextureHandle handle = pool.back();
		pool.pop_back();
		return handle;
	}

	TextureInitDesc texDesc =
	{
		{
			desc.width, desc.height,
			1, 1,
			desc.format,
			desc.usage,
			false
		},
		{}
	};

	return m_device->CreateTexture(texDesc);
}

GPUBufferHandle RenderGraph::RealizeResource(const RGBufferDesc& desc)
{
	auto& pool = m_bufferResourcePool[desc];

	if (!pool.empty())
	{
		GPUBufferHandle handle = pool.back();
		pool.pop_back();
		return handle;
	}

	BufferDesc bufDesc =
	{
		desc.size,
		desc.stride,
		desc.usage,
		MemoryAccess::GpuOnly,
	};

	return m_device->CreateBuffer(bufDesc);
}

void RenderGraph::Clear()
{
	for (auto& resource : m_textures)
	{
		if (!resource.imported && resource.realizedHandle.IsValid())
		{
			m_textureResourcePool[resource.desc].push_back(resource.realizedHandle);
		}
	}

	for (auto& resource : m_buffers)
	{
		if (!resource.imported && resource.realizedHandle.IsValid())
		{
			m_bufferResourcePool[resource.desc].push_back(resource.realizedHandle);
		}
	}

	m_textures.clear();
	m_buffers.clear();
	m_passes.clear();
	m_textureEpilogueBarriers.clear();
	m_bufferEpilogueBarriers.clear();
}

void RenderGraph::DebugPrintPasses() const
{
	for (uint32_t i = 0; i < m_passes.size(); i++)
	{
		const auto& pass = m_passes[i];

		MOKOLOG_INFO("  [{}] {:<12} | refCount: {} | culled: {}",
			i,
			pass.name,
			pass.refCount,
			pass.culled ? "YES" : "no");
	}
}

void RenderGraph::DebugPrintBarriers() const
{
	MOKOLOG_INFO("=== Barriers ===");
	for (auto& pass : m_passes)
	{
		if (pass.culled) continue;
		for (auto& b : pass.textureBarrierInfos)
		{
			MOKOLOG_INFO("  [{}] res[{}]: {} -> {}",
				pass.name, b.handle.index,
				static_cast<int>(b.before), static_cast<int>(b.after));
		}
	}
	for (auto& b : m_textureEpilogueBarriers)
	{
		MOKOLOG_INFO("  [Epilogue] res[{}]: {} -> {}",
			b.handle.index, static_cast<int>(b.before), static_cast<int>(b.after));
	}
}