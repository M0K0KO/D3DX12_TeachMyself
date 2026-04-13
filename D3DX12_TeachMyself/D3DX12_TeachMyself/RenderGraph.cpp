#include "RenderGraph.h"
#include "GraphicsDevice.h"
#include "CommandContext.h"
#include <debugapi.h>
#include "MokoLog.h"

void RGBuilder::Read(RGResourceHandle handle, RGResourceState state)
{
	auto& resource = m_graph->m_resources[handle.index];
	resource.refCount++;

	auto& pass = m_graph->m_passes[m_passIndex];
	pass.reads.push_back(handle);
	pass.resourceStates[handle.index] = state;

	if (resource.firstUserIndex == UINT32_MAX)
	{
		resource.firstUserIndex = m_passIndex;
	}
	resource.lastUserIndex = m_passIndex;
}

RGResourceHandle RGBuilder::Write(RGResourceHandle handle, RGResourceState state)
{
	auto& resource = m_graph->m_resources[handle.index];
	resource.producerPassIndex = m_passIndex;
	
	auto& pass = m_graph->m_passes[m_passIndex];
	pass.writes.push_back(handle);
	pass.resourceStates[handle.index] = state;
	pass.refCount++;

	if (resource.firstUserIndex == UINT32_MAX)
	{
		resource.firstUserIndex = m_passIndex;
	}
	resource.lastUserIndex = m_passIndex;

	RGResourceHandle newHandle = handle;
	newHandle.version++;
	return newHandle;
}





RenderGraph::RenderGraph(GraphicsDevice* device)
	:
	m_device(device)
{
}

RGResourceHandle RenderGraph::CreateTexture(RGResourceDesc desc, RGResourceState state)
{
	RGResource resource = {};
	resource.desc = desc;
	resource.initialState = state;
	resource.currentState = state;

	uint32_t index = static_cast<uint32_t>(m_resources.size());
	m_resources.push_back(resource);

	return RGResourceHandle{ index, 0 };
}

RGResourceHandle RenderGraph::ImportTexture(TextureHandle existing, RGResourceDesc desc, RGResourceState state)
{
	RGResource resource = {};
	resource.desc = desc;
	resource.initialState = state;
	resource.currentState = state;
	resource.imported = true;
	resource.realizedHandle = existing;

	uint32_t index = static_cast<uint32_t>(m_resources.size());
	m_resources.push_back(resource);

	return RGResourceHandle{ index, 0 };
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
		for (auto& writeHandle : pass.writes)
		{
			auto& resource = m_resources[writeHandle.index];
			if (resource.imported)
			{
				pass.refCount++;
			}
		}
	}

	std::vector<uint32_t> stack;

	for (uint32_t i = 0; i < m_resources.size(); i++)
	{
		if (!m_resources[i].imported && m_resources[i].refCount == 0)
		{
			stack.push_back(i);
		}
	}

	while (!stack.empty())
	{
		uint32_t resourceIndex = stack.back();
		stack.pop_back();
		
		auto& resource = m_resources[resourceIndex];

		if (resource.producerPassIndex != UINT32_MAX)
		{
			auto& producer = m_passes[resource.producerPassIndex];
			producer.refCount--;

			if (producer.refCount == 0)
			{
				producer.
					culled = true;

				for (auto& readHandle : producer.reads)
				{
					auto& readResource = m_resources[readHandle.index];
					readResource.refCount--;

					if (readResource.refCount == 0 && !readResource.imported)
					{
						stack.push_back(readHandle.index);
					}
				}
			}
		}
	}

	for (auto& res : m_resources)
	{
		res.currentState = res.initialState;
	}

	for (auto& pass : m_passes)
	{
		if (pass.culled) continue;

		auto checkBarrier = [&](RGResourceHandle handle) {
			auto& res = m_resources[handle.index];
			auto required = pass.resourceStates[handle.index];
			if (res.currentState != required)
			{
				pass.barrierInfos.push_back({ handle, res.currentState, required });
				res.currentState = required;
			}
			};

		for (auto& h : pass.reads)  checkBarrier(h);
		for (auto& h : pass.writes) checkBarrier(h);
	}

	for (uint32_t i = 0; i < m_resources.size(); i++)
	{
		auto& res = m_resources[i];
		if (res.imported && res.currentState != res.initialState)
		{
			RGResourceHandle handle{ i, 0 };
			m_epilogueBarriers.push_back({ handle, res.currentState, res.initialState });
		}
	}
}

void RenderGraph::Execute(CommandContext& ctx)
{
	for (auto& pass : m_passes)
	{
		if (pass.culled) continue;

		for (auto& writeHandle : pass.writes)
		{
			auto& resource = m_resources[writeHandle.index];
			if (!resource.imported && !resource.realizedHandle.IsValid())
			{
				resource.realizedHandle = RealizeResource(resource.desc);
			}
		}

		for (auto& readHandle : pass.reads)
		{
			auto& resource = m_resources[readHandle.index];
			if (!resource.imported && !resource.realizedHandle.IsValid())
			{
				resource.realizedHandle = RealizeResource(resource.desc);
			}
		}
	}

	for (auto& pass : m_passes)
	{
		if (pass.culled) continue;

		for (auto& barrierInfo : pass.barrierInfos)
		{
			auto& res = m_resources[barrierInfo.handle.index];
			ctx.TransitionBarrier(res.realizedHandle, barrierInfo.before, barrierInfo.after);
		}

		pass.executeFunc(ctx);
	}


	for (auto& barrierInfo : m_epilogueBarriers)
	{
		auto& res = m_resources[barrierInfo.handle.index];
		ctx.TransitionBarrier(res.realizedHandle, barrierInfo.before, barrierInfo.after);
	}
}

TextureHandle RenderGraph::RealizeResource(const RGResourceDesc& desc)
{
	auto& pool = m_resourcePool[desc];

	if (!pool.empty())
	{
		TextureHandle handle = pool.back();
		pool.pop_back();
		return handle;
	}

	TextureDesc texDesc = {};
	texDesc.width = desc.width;
	texDesc.height = desc.height;
	texDesc.format = desc.format;
	texDesc.usage = desc.usage;

	return m_device->CreateTexture(texDesc, nullptr);
}

void RenderGraph::Clear()
{
	/*
	for (auto& resource : m_resources)
	{
		if (!resource.imported && resource.realizedHandle.IsValid())
		{
			m_resourcePool[resource.desc].push_back(resource.realizedHandle);
		}
	}
	*/

	m_resources.clear();
	m_passes.clear();
	m_epilogueBarriers.clear();
}

void RenderGraph::DebugPrintPasses() const
{
	for (uint32_t i = 0; i < m_passes.size(); i++)
	{
		const auto& pass = m_passes[i];

		LOG_INFO("  [%u] %-12s | refCount: %u | culled: %s\n",
			i,
			pass.name.c_str(),
			pass.refCount,
			pass.culled ? "YES" : "no");
	}
}

void RenderGraph::DebugPrintBarriers() const
{
	LOG_INFO("=== Barriers ===\n");
	for (auto& pass : m_passes)
	{
		if (pass.culled) continue;
		for (auto& b : pass.barrierInfos)
		{
			LOG_INFO("  [%s] res[%u]: %d -> %d\n",
				pass.name.c_str(), b.handle.index,
				(int)b.before, (int)b.after);
		}
	}
	for (auto& b : m_epilogueBarriers)
	{
		LOG_INFO("  [Epilogue] res[%u]: %d -> %d\n",
			b.handle.index, (int)b.before, (int)b.after);
	}
}