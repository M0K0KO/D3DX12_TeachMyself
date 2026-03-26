#pragma once
#include "RHITypes.h"

class CommandContext
{
public:
	virtual ~CommandContext() = default;

	virtual void SetPipeline(PipelineHandle handle) = 0;
	virtual void SetVertexBuffer(BufferHandle handle) = 0;
	virtual void SetIndexBuffer(BufferHandle handle) = 0;
	virtual void BindConstantBuffer(BufferHandle handle, uint32_t slot) = 0;
	virtual void DrawIndexed(uint32_t indexCount, uint32_t startIndex, uint32_t baseVertex) = 0;
};